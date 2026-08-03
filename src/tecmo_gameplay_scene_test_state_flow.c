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
             scene->ball_position.y_q8 != 181 * 256) ||
            (frame == 3U &&
             scene->ball_position.y_q8 != 182 * 256) ||
            (frame == 7U &&
             scene->ball_position.y_q8 != 187 * 256) ||
            (frame == 11U &&
             scene->ball_position.y_q8 != 199 * 256)) {
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

static bool scene_test_live_foundation_regressions(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneLaunch *launch_input,
    TecmoControlFrame *p1_input,
    TecmoControlFrame *p2_input,
    char *message,
    size_t message_size)
{
    static const TecmoGameplayCourtCoordinate exact_positions[10] = {
        {528, 144}, {448, 144}, {362, 112}, {364, 192}, {392, 144},
        {176, 144}, {320, 144}, {408, 112}, {400, 192}, {372, 144}
    };
    static const uint8_t exact_directions[10] = {
        1U, 1U, 2U, 5U, 1U, 0U, 0U, 2U, 5U, 0U
    };
    static const uint8_t exact_links[10] = {
        5U, 6U, 7U, 8U, 9U, 0U, 1U, 2U, 3U, 4U
    };
    static const uint8_t away_permutation[5] = {5U, 6U, 10U, 11U, 0U};
    static const uint8_t home_permutation[5] = {11U, 10U, 6U, 5U, 1U};
    TecmoGameplaySceneLaunch bound;
    TecmoGameplaySceneLaunch legacy_launch;
    TecmoControlFrame legacy_p1;
    TecmoControlFrame legacy_p2;
    TecmoGameplaySceneLaunch malformed;
    TecmoGameplaySceneLaunch legacy;
    TecmoGameplaySceneLaunch manual;
    TecmoGameplaySceneLaunch cpu_only;
    TecmoGameplayScene malformed_scene;
    TecmoGameplayScene snapshot;
    TecmoGameplayScene broken;
    TecmoGameplayLiveFoundation foundation_before;
    TecmoGameplayLiveFoundation candidate_foundation;
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCpuSteeringPlayResult play_result;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCpuSteeringShotInput shot_input;
    TecmoGameplayCpuSteeringShotResult shot_result;
    TecmoGameplayCpuSteeringMovementInput movement_input;
    TecmoGameplayCpuSteeringMovementResult movement_result;
    TecmoGameplayCourtCoordinate positions[10];
    uint8_t actor_team[10];
    TecmoGameplaySceneCpuShotRequest shot_request;
    const TecmoTeamDataPlayer *player;
    TecmoControlFrame neutral_one;
    TecmoControlFrame neutral_two;
    uint8_t formation_index;
    uint16_t deferred_offset = 0U;
    uint16_t advanced_offset = 0U;
    uint8_t deferred_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t advanced_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t expected_direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    uint32_t sync_serial_before;
    bool found_deferred = false;
    bool found_advance = false;
    size_t actor;
    uint16_t offset;

#define LIVE_FAIL(text_value) do { \
        tecmo_gameplay_scene_test_message(message, message_size, \
                                           (text_value)); \
        tecmo_gameplay_scene_test_set_skip_pretip(false); \
        return false; \
    } while (0)

    if (scene == NULL || launch_input == NULL || p1_input == NULL ||
        p2_input == NULL) {
        LIVE_FAIL("LIVE regression context missing");
    }
    legacy_launch = *launch_input;
    legacy_p1 = *p1_input;
    legacy_p2 = *p2_input;
    bound = *launch_input;
    bound.starter_binding_bound = true;
    memcpy(bound.starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY],
           away_permutation, sizeof(away_permutation));
    memcpy(bound.starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME],
           home_permutation, sizeof(home_permutation));
    bound.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    bound.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    bound.game_music_enabled = false;

    /* The false source/default-initializer flag normalizes to canonical
       identity binding while the
       scene-owned origin bit preserves the old direct layout/cadence. */
    legacy = bound;
    legacy.starter_binding_bound = false;
    memset(legacy.starter_roster_index, 0,
           sizeof(legacy.starter_roster_index));
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &legacy)) {
        LIVE_FAIL("LIVE unbound legacy launch rejected");
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!scene->legacy_direct_launch ||
        !scene->launch.starter_binding_bound ||
        scene->launch.starter_roster_index[0U][0U] != 0U ||
        scene->launch.starter_roster_index[1U][4U] != 4U ||
        scene->actors[0U].position.x != 0x0160 ||
        scene->actors[0U].position.y != 198 ||
        scene->actors[4U].position.x != 0x01CF ||
        scene->actors[4U].position.y != 183) {
        LIVE_FAIL("LIVE unbound legacy layout compatibility failed");
    }

    /* Invalid bound arrays fail before launch mutation. */
    snapshot = *scene;
    malformed = bound;
    malformed.starter_roster_index[0U][1U] =
        malformed.starter_roster_index[0U][0U];
    if (tecmo_gameplay_scene_launch(scene, &malformed) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        LIVE_FAIL("LIVE duplicate lineup launch was not transactional");
    }
    malformed = bound;
    malformed.starter_roster_index[1U][2U] = 12U;
    if (tecmo_gameplay_scene_launch(scene, &malformed) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        LIVE_FAIL("LIVE out-of-range lineup launch was not transactional");
    }

    /* Production-style bound launch selects the exact Bank04 layout and the
       local-slot permutation, including profiles beyond the former 0..4. */
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &bound)) {
        LIVE_FAIL("LIVE bound launch rejected");
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (scene->legacy_direct_launch || !scene->launch.starter_binding_bound) {
        LIVE_FAIL("LIVE bound launch retained legacy origin");
    }
    for (actor = 0U; actor < 10U; ++actor) {
        uint8_t side = actor < 5U ? TECMO_GAMEPLAY_TEAM_AWAY
                                  : TECMO_GAMEPLAY_TEAM_HOME;
        uint8_t local = (uint8_t)(actor % 5U);
        uint8_t expected_roster =
            scene->launch.starter_roster_index[side][local];
        if (scene->actors[actor].position.x != exact_positions[actor].x ||
            scene->actors[actor].position.y != exact_positions[actor].y ||
            scene->actors[actor].movement_direction !=
                exact_directions[actor] ||
            scene->cpu_actors[actor].linked_actor != exact_links[actor] ||
            scene->actors[actor].roster_index != expected_roster ||
            (player = scene_actor_player(scene, &scene->actors[actor])) == NULL ||
            player != &scene->pretip_team_data->players[
                side == TECMO_GAMEPLAY_TEAM_AWAY
                    ? scene->launch.away_team : scene->launch.home_team]
                [expected_roster]) {
            LIVE_FAIL("LIVE bound slot/roster/profile binding failed");
        }
        positions[actor] = scene->actors[actor].position;
        actor_team[actor] = scene->actors[actor].team;
    }
    if (scene->live_foundation.play_state.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_PLAY_STATE_TAG ||
        scene->live_foundation.static_primary_seed != 4U ||
        scene->live_foundation.static_defender_seed != 9U ||
        scene->live_foundation.play_state.primary_actor != 4U ||
        scene->live_foundation.play_state.defender_actor != 9U ||
        scene->live_foundation.play_state.matchup_seed[0U] != 2U ||
        scene->live_foundation.play_state.matchup_seed[1U] != 7U ||
        !scene->live_foundation.first_sync_pending ||
        scene->live_foundation.formation_index != 30U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        LIVE_FAIL("LIVE initializer seeds or formation identity failed");
    }
    for (actor = 0U; actor < 10U; ++actor) {
        if (scene->live_foundation.formation_start_offset[actor] !=
                scene->cpu_steering_assets.formation_stream_offsets[30U][actor] ||
            memcmp(scene->live_foundation.play_state.fixed_link,
                   exact_links, sizeof(exact_links)) != 0) {
            LIVE_FAIL("LIVE formation start/fixed-link proof failed");
        }
    }

    /* The actual first post-handoff sync consumes the holder, not the exact
       static startup selector. */
    if (!scene_sync_live_foundation(scene) ||
        scene->live_foundation.first_sync_pending ||
        scene->live_foundation.primary_actor != 0U ||
        scene->live_foundation.defender_actor != 5U ||
        scene->live_foundation.static_primary_seed != 4U) {
        LIVE_FAIL("LIVE away holder 0 did not replace static primary seed");
    }
    foundation_before = scene->live_foundation;
    /* LIVE foundation invariants fail closed independently, rather than
       relying on the caller to preserve aligned stream/matchup metadata. */
    candidate_foundation = foundation_before;
    candidate_foundation.last_step_offset[0U] ^= 1U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE stream offset alignment negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.native_matchup_actor[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE fixed/native matchup separation negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.matchup_seed[0U] = 3U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE matchup seed negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.primary_actor = 1U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE primary adapter synchronization negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.native_matchup_inferred = false;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE native matchup classification negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.workspace_native_approximation = false;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE workspace classification negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.shot_request_native_approximation = false;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE shot workspace classification negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.formation_source_pinned = false;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE formation source classification negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.source_direction[0U] = 0U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE invalid source-direction sentinel was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.source_direction_valid[0U] = true;
    candidate_foundation.source_direction[0U] = 0U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE mismatched source-direction write was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.last_shot_deferred = true;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE deferred-without-request shot flags were accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.last_shot_request = true;
    candidate_foundation.last_shot_deferred = true;
    candidate_foundation.last_shot_playback_supported = true;
    candidate_foundation.last_shot_actor = 0U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE contradictory shot flags were accepted");
    }
    {
        const uint8_t duplicate_controller_team[2] = {
            TECMO_GAMEPLAY_TEAM_AWAY, TECMO_GAMEPLAY_TEAM_AWAY
        };
        const uint8_t duplicate_controlled_actor[2] = {0U, 1U};
        candidate_foundation = foundation_before;
        if (tecmo_gameplay_live_foundation_synchronize(
                &scene->cpu_steering_assets, positions, 0U,
                TECMO_GAMEPLAY_TEAM_AWAY, 0U, actor_team,
                duplicate_controller_team, duplicate_controlled_actor,
                &candidate_foundation) ||
            memcmp(&candidate_foundation, &foundation_before,
                   sizeof(candidate_foundation)) != 0) {
            LIVE_FAIL("LIVE duplicate controller-team routing was accepted");
        }
    }
    /* A real role/orientation transition invalidates all old command writes. */
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.target_actor[0U] = 5U;
    candidate_foundation.play_state.target_x[0U] = positions[5U].x;
    candidate_foundation.play_state.target_depth[0U] = positions[5U].y;
    candidate_foundation.source_target_valid[0U] = true;
    candidate_foundation.play_state.direction[0U] = 0U;
    candidate_foundation.source_direction[0U] = 0U;
    candidate_foundation.source_direction_valid[0U] = true;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation) ||
        !tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions, 1U,
            TECMO_GAMEPLAY_TEAM_AWAY, 0U, actor_team,
            scene->launch.controller_team, scene->controlled_actor,
            &candidate_foundation) ||
        candidate_foundation.play_state.target_actor[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        candidate_foundation.play_state.target_x[0U] != 0 ||
        candidate_foundation.play_state.target_depth[0U] != 0 ||
        candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.play_state.direction[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
        candidate_foundation.source_direction_valid[0U] ||
        candidate_foundation.source_direction[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION) {
        LIVE_FAIL("LIVE synchronization did not invalidate stale source metadata");
    }
    candidate_foundation = foundation_before;
    if (tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions, 0U,
            TECMO_GAMEPLAY_TEAM_HOME, 0U, actor_team,
            scene->launch.controller_team, scene->controlled_actor,
            &candidate_foundation) ||
        memcmp(&candidate_foundation, &foundation_before,
               sizeof(candidate_foundation)) != 0) {
        LIVE_FAIL("LIVE wrong-team holder was not rejected transactionally");
    }
    candidate_foundation = foundation_before;
    sync_serial_before = candidate_foundation.sync_serial;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions, 1U,
            TECMO_GAMEPLAY_TEAM_AWAY, 0U, actor_team,
            scene->launch.controller_team, scene->controlled_actor,
            &candidate_foundation) ||
        candidate_foundation.orientation != 1U ||
        candidate_foundation.sync_serial != sync_serial_before + 1U) {
        LIVE_FAIL("LIVE orientation transition was not a sync event");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.sync_serial = UINT32_MAX;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions,
            foundation_before.orientation == 0U ? 1U : 0U,
            TECMO_GAMEPLAY_TEAM_AWAY, 0U, actor_team,
            scene->launch.controller_team, scene->controlled_actor,
            &candidate_foundation) ||
        candidate_foundation.sync_serial != 0U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE sync serial MAX did not wrap transactionally");
    }
    candidate_foundation = foundation_before;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions, 0U,
            TECMO_GAMEPLAY_TEAM_HOME, 5U, actor_team,
            scene->launch.controller_team, scene->controlled_actor,
            &candidate_foundation) ||
        candidate_foundation.primary_actor != 5U ||
        candidate_foundation.defender_actor != 0U) {
        LIVE_FAIL("LIVE home holder 5 matchup synchronization failed");
    }
    if (!scene_handoff_possession(scene, TECMO_GAMEPLAY_TEAM_HOME, 5U) ||
        !scene_sync_live_foundation(scene) ||
        scene->live_foundation.primary_actor != 5U ||
        !scene_handoff_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        !scene_sync_live_foundation(scene) ||
        scene->live_foundation.primary_actor != 0U) {
        LIVE_FAIL("LIVE pass/switch/handoff synchronization failed");
    }

    /* Formation selector boundary and evolving command-stream offsets. */
    {
        TecmoGameplayCourtCoordinate boundary = {576, 192};
        TecmoGameplayCourtCoordinate unsupported46 = {640, 192};
        TecmoGameplayCourtCoordinate unsupported47 = {704, 192};
        if (!tecmo_gameplay_live_foundation_formation_index_for_coordinate(
                &boundary, &formation_index) || formation_index != 45U ||
            tecmo_gameplay_live_foundation_formation_index_for_coordinate(
                &unsupported46, &formation_index) ||
            tecmo_gameplay_live_foundation_formation_index_for_coordinate(
                &unsupported47, &formation_index)) {
            LIVE_FAIL("LIVE 46/48 formation boundary was not fail-closed");
        }
    }
    memset(&play_input, 0, sizeof(play_input));
    play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    play_input.actor = 0U;
    play_input.step_budget = 2U;
    play_input.orientation_035a = 0U;
    memcpy(play_input.actor_position, positions, sizeof(positions));
    foundation_before = scene->live_foundation;
    candidate_foundation = foundation_before;
    if (tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        memcmp(&candidate_foundation, &foundation_before,
               sizeof(candidate_foundation)) != 0) {
        LIVE_FAIL("LIVE step budget failure was not transactional");
    }
    for (actor = 0U; actor < 10U && !found_advance; ++actor) {
        candidate_foundation = foundation_before;
        memset(&play_input, 0, sizeof(play_input));
        play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
        play_input.actor = (uint8_t)actor;
        play_input.step_budget = 1U;
        memcpy(play_input.actor_position, positions, sizeof(positions));
        if (tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input, &candidate_foundation,
                &play_result) && play_result.advanced &&
            candidate_foundation.play_state.stream_offset[actor] !=
                foundation_before.formation_start_offset[actor] &&
            tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            advanced_actor = (uint8_t)actor;
            advanced_offset = candidate_foundation.play_state.stream_offset[actor];
            found_advance = true;
        }
    }
    if (!found_advance || advanced_actor >= 10U || advanced_offset == 0U) {
        LIVE_FAIL("LIVE source-order step did not advance one bounded record");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.tick_serial = UINT32_MAX;
    play_input.actor = advanced_actor;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        candidate_foundation.tick_serial != 0U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE tick serial MAX did not wrap transactionally");
    }
    memset(&play_input, 0, sizeof(play_input));
    play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    play_input.step_budget = 1U;
    memcpy(play_input.actor_position, positions, sizeof(positions));
    for (offset = 0U;
         offset < scene->cpu_steering_assets.command_record_count * 5U;
         offset = (uint16_t)(offset + 5U)) {
        if (!tecmo_gameplay_cpu_steering_decode_command(
                &scene->cpu_steering_assets, offset, &command)) {
            continue;
        }
        if (command.opcode == 5U || command.opcode == 6U ||
            command.opcode == 8U || command.opcode == 10U ||
            command.opcode == 12U || command.opcode == 13U ||
            command.opcode == 15U || command.opcode == 16U ||
            command.opcode == 20U || command.opcode == 23U) {
            deferred_actor = 0U;
            deferred_offset = offset;
            found_deferred = true;
            break;
        }
    }
    if (!found_deferred) LIVE_FAIL("LIVE deferred source fixture missing");
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.stream_offset[deferred_actor] =
        deferred_offset;
    candidate_foundation.last_step_offset[deferred_actor] = deferred_offset;
    candidate_foundation.play_state.target_actor[deferred_actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[deferred_actor] = 0;
    candidate_foundation.play_state.target_depth[deferred_actor] = 0;
    candidate_foundation.source_target_valid[deferred_actor] = false;
    play_input.actor = deferred_actor;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input, &candidate_foundation,
            &play_result) || !play_result.deferred ||
        candidate_foundation.source_target_valid[deferred_actor] ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE deferred target became an unproven movement target");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.target_actor[0U] = 5U;
    candidate_foundation.play_state.target_x[0U] = positions[5U].x;
    candidate_foundation.play_state.target_depth[0U] = positions[5U].y;
    candidate_foundation.source_target_valid[0U] = true;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE source actor-target valid-coordinate fixture rejected");
    }
    candidate_foundation.play_state.target_x[0U] =
        TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1;
    candidate_foundation.play_state.target_depth[0U] =
        positions[5U].y;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE source actor-target invalid coordinate was accepted");
    }

    /* Shot predicate positive/negative and explicit deterministic mapping. */
    memset(&shot_input, 0, sizeof(shot_input));
    shot_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_SHOT_INPUT_TAG;
    shot_input.timer_0798 = 1U;
    shot_input.difficulty = 0U;
    shot_input.timer_0760 = 12U;
    shot_input.rating_0533 = 0xFFU;
    candidate_foundation = foundation_before;
    if (!tecmo_gameplay_live_foundation_shot_request(
            &scene->cpu_steering_assets, &shot_input, 0U,
            &candidate_foundation, &shot_result) || !shot_result.request ||
        !candidate_foundation.last_shot_request) {
        LIVE_FAIL("LIVE deterministic shot request positive fixture failed");
    }
    shot_input.target_delta_high = 1U;
    candidate_foundation = foundation_before;
    if (!tecmo_gameplay_live_foundation_shot_request(
            &scene->cpu_steering_assets, &shot_input, 0U,
            &candidate_foundation, &shot_result) || shot_result.request ||
        candidate_foundation.last_shot_request) {
        LIVE_FAIL("LIVE shot request negative gate failed");
    }
    memset(&movement_input, 0, sizeof(movement_input));
    movement_input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
    movement_input.steering.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    memcpy(movement_input.steering.actor_position, positions,
           sizeof(positions));
    movement_input.steering.actor = 0U;
    movement_input.steering.possession = TECMO_GAMEPLAY_TEAM_AWAY;
    movement_input.steering.orientation = 0U;
    movement_input.steering.ball_holder = 0U;
    movement_input.steering.matchup_actor = 5U;
    movement_input.steering.difficulty = 0U;
    movement_input.primary_selected_actor = true;
    movement_input.steering.has_explicit_target = true;
    movement_input.steering.explicit_target.x =
        positions[5U].x;
    movement_input.steering.explicit_target.y = positions[5U].y;
    if (!scene_actor_movement_state(scene, &scene->actors[0U],
                                    &movement_input.movement) ||
        (player = scene_actor_player(scene, &scene->actors[0U])) == NULL) {
        LIVE_FAIL("LIVE direction/TGMO fixture setup failed");
    }
    movement_input.player_movement_rating = player->profile[0];
    movement_input.condition = scene->actors[0U].condition;
    movement_input.speed_value = scene->launch.speed_value;
    memset(&movement_result, 0, sizeof(movement_result));
    if (!tecmo_gameplay_cpu_steering_direction_for_delta(
            &scene->cpu_steering_assets,
            (int16_t)(positions[5U].x - positions[0U].x),
            (int16_t)(positions[5U].y - positions[0U].y),
            &expected_direction)) {
        LIVE_FAIL("LIVE source direction fixture quantizer failed");
    }
    if (!tecmo_gameplay_cpu_steering_movement_step(
            &scene->cpu_steering_assets, &scene->movement_assets,
            &movement_input, &movement_result)) {
        LIVE_FAIL("LIVE source direction fixture TGMO composition failed");
    }
    if (movement_result.steering.direction != expected_direction ||
        movement_result.held_direction_bits >=
            sizeof(scene->movement_assets.direction_map) ||
        scene->movement_assets.direction_map[
            movement_result.held_direction_bits] != expected_direction) {
        (void)snprintf(message, message_size,
                        "LIVE source direction did not reach TGMO input: expected=%u actual=%u held=%u map=%u",
                        (unsigned)expected_direction,
                        (unsigned)movement_result.steering.direction,
                        (unsigned)movement_result.held_direction_bits,
                        movement_result.held_direction_bits <
                                sizeof(scene->movement_assets.direction_map)
                            ? (unsigned)scene->movement_assets.direction_map[
                                  movement_result.held_direction_bits]
                            : 0xFFU);
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        return false;
    }

    /* A PRETIP transient update must not synchronize LIVE state. */
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!tecmo_gameplay_scene_launch(scene, &bound)) {
        LIVE_FAIL("LIVE PRETIP safety launch rejected");
    }
    foundation_before = scene->live_foundation;
    memset(&neutral_one, 0, sizeof(neutral_one));
    memset(&neutral_two, 0, sizeof(neutral_two));
    if (!tecmo_gameplay_scene_update(scene, &neutral_one, &neutral_two) ||
        memcmp(&scene->live_foundation, &foundation_before,
               sizeof(foundation_before)) != 0) {
        LIVE_FAIL("PRETIP update synchronized transient LIVE state");
    }
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &bound)) {
        LIVE_FAIL("LIVE post-PRETIP bound launch rejected");
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!scene_sync_live_foundation(scene)) {
        LIVE_FAIL("LIVE post-PRETIP handoff sync rejected");
    }

    /* Canonical stored arrays are part of ownership, not merely a bound flag. */
    malformed_scene = *scene;
    malformed_scene.launch.starter_roster_index[0U][0U] = 12U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE normalized stored lineup corruption was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.launch.starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][1U] =
        malformed_scene.launch.starter_roster_index
            [TECMO_GAMEPLAY_TEAM_AWAY][0U];
    malformed_scene.actors[1U].roster_index =
        malformed_scene.actors[0U].roster_index;
    malformed_scene.actors[1U].condition = malformed_scene.fatigue_state
        .condition[TECMO_GAMEPLAY_TEAM_AWAY]
        [malformed_scene.actors[1U].roster_index];
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE post-launch duplicate starter uniqueness was accepted");
    }
    broken = *scene;
    broken.live_foundation.orientation = 9U;
    snapshot = broken;
    memset(&shot_request, 0, sizeof(shot_request));
    if (scene_update_ai(&broken, &shot_request) ||
        memcmp(&broken, &snapshot, sizeof(broken)) != 0) {
        LIVE_FAIL("LIVE failed transaction changed the complete scene");
    }

    /* Manual routing is not positional: P1=home/P2=away and CPU-side with no
       controllers both remain coherent at the adapter seam. */
    manual = bound;
    manual.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    manual.controller_team[1] = TECMO_GAMEPLAY_TEAM_AWAY;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &manual) ||
        scene->controlled_actor[0U] != 5U ||
        scene->controlled_actor[1U] != 0U ||
        !scene_sync_live_foundation(scene)) {
        LIVE_FAIL("LIVE swapped controller-team routing was rejected");
    }
    cpu_only = bound;
    cpu_only.controller_team[0] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    cpu_only.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
        scene->controlled_actor[0U] != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->controlled_actor[1U] != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        !scene_sync_live_foundation(scene)) {
        LIVE_FAIL("LIVE CPU-side controller routing was rejected");
    }

    /* Bound LIVE still owns the human TGMO path. Exercise its one-update
       latency, offensive A pass, defensive A nearest-switch, and a swapped
       controller-team route while checking that selected roster identity and
       fatigue condition remain attached to the local stable slot. */
    {
        TecmoGameplaySceneLaunch human = bound;
        TecmoGameplaySceneLaunch swapped = bound;
        TecmoControlFrame human_p1;
        TecmoControlFrame human_p2;
        int16_t start_x;
        uint8_t switched_actor;
        uint8_t expected_roster;
        uint8_t expected_team;

        human.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
        human.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &human) ||
            !scene_sync_live_foundation(scene) ||
            scene->controlled_actor[0U] != 0U ||
            scene->controlled_actor[1U] != 5U) {
            LIVE_FAIL("LIVE bound human routing setup failed");
        }
        memset(&human_p1, 0, sizeof(human_p1));
        memset(&human_p2, 0, sizeof(human_p2));
        start_x = scene->actors[0U].position.x;
        human_p1.held.right = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->actors[0U].position.x != start_x ||
            scene->actors[0U].movement_action_state !=
                TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT ||
            !tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->actors[0U].position.x != start_x + 1) {
            LIVE_FAIL("LIVE bound human TGMO one-update latency failed");
        }
        expected_roster =
            scene->launch.starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][0U];
        if (scene->actors[0U].roster_index != expected_roster ||
            scene->actors[0U].condition != scene->fatigue_state.condition
                [TECMO_GAMEPLAY_TEAM_AWAY][expected_roster]) {
            LIVE_FAIL("LIVE human movement lost away roster condition");
        }

        memset(&human_p1, 0, sizeof(human_p1));
        human_p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->ball_holder != 1U ||
            scene->controlled_actor[0U] != 1U ||
            scene->actors[1U].team != TECMO_GAMEPLAY_TEAM_AWAY) {
            LIVE_FAIL("LIVE bound human offensive A pass failed");
        }
        expected_roster =
            scene->launch.starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][1U];
        if (scene->actors[1U].roster_index != expected_roster ||
            scene->actors[1U].condition != scene->fatigue_state.condition
                [TECMO_GAMEPLAY_TEAM_AWAY][expected_roster]) {
            LIVE_FAIL("LIVE pass changed away roster condition");
        }

        if (!scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_HOME, 5U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE human defensive handoff setup failed");
        }
        switched_actor = scene_nearest_actor_for_team(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, scene->ball_holder);
        memset(&human_p1, 0, sizeof(human_p1));
        human_p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->controlled_actor[0U] != switched_actor ||
            scene->actors[scene->controlled_actor[0U]].team !=
                TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->controlled_actor[1U] != 5U) {
            LIVE_FAIL("LIVE bound human defensive A nearest-switch failed");
        }
        expected_roster = scene->actors[switched_actor].roster_index;
        if (scene->actors[switched_actor].condition != scene->fatigue_state
                .condition[TECMO_GAMEPLAY_TEAM_AWAY][expected_roster]) {
            LIVE_FAIL("LIVE defensive switch changed away roster condition");
        }

        swapped.controller_team[0U] = TECMO_GAMEPLAY_TEAM_HOME;
        swapped.controller_team[1U] = TECMO_GAMEPLAY_TEAM_AWAY;
        if (!tecmo_gameplay_scene_launch(scene, &swapped) ||
            !scene_sync_live_foundation(scene) ||
            scene->controlled_actor[0U] != 5U ||
            scene->controlled_actor[1U] != 0U) {
            LIVE_FAIL("LIVE swapped bound human routing setup failed");
        }
        memset(&human_p1, 0, sizeof(human_p1));
        memset(&human_p2, 0, sizeof(human_p2));
        start_x = scene->actors[5U].position.x;
        human_p1.held.right = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->actors[5U].position.x != start_x ||
            !tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->actors[5U].position.x != start_x + 1) {
            LIVE_FAIL("LIVE swapped controller human TGMO routing failed");
        }
        memset(&human_p1, 0, sizeof(human_p1));
        memset(&human_p2, 0, sizeof(human_p2));
        human_p2.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->ball_holder != 1U ||
            scene->controlled_actor[1U] != 1U) {
            LIVE_FAIL("LIVE swapped controller offensive A pass failed");
        }
        expected_team = scene->actors[scene->controlled_actor[0U]].team;
        if (expected_team != TECMO_GAMEPLAY_TEAM_HOME ||
            scene->actors[5U].roster_index !=
                scene->launch.starter_roster_index
                    [TECMO_GAMEPLAY_TEAM_HOME][0U]) {
            LIVE_FAIL("LIVE swapped controller home roster binding failed");
        }
        if (!scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_HOME, 5U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE swapped controller defensive handoff failed");
        }
        switched_actor = scene_nearest_actor_for_team(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, scene->ball_holder);
        memset(&human_p2, 0, sizeof(human_p2));
        human_p2.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->controlled_actor[1U] != switched_actor ||
            scene->actors[switched_actor].team != TECMO_GAMEPLAY_TEAM_AWAY) {
            LIVE_FAIL("LIVE swapped controller defensive nearest-switch failed");
        }
        expected_roster = scene->actors[switched_actor].roster_index;
        if (scene->actors[switched_actor].condition != scene->fatigue_state
                .condition[TECMO_GAMEPLAY_TEAM_AWAY][expected_roster]) {
            LIVE_FAIL("LIVE swapped defensive roster condition failed");
        }
    }

    /* An accepted direction can point out of the court at an edge or corner.
       The LIVE adapter must preserve that source direction, classify only
       its TGMO composition as inert/deferred, and leave the edge actor,
       attached ball, and ownership unchanged. The stream offsets below are
       discovered from the accepted payload so this remains a source-backed
       fixture rather than an invented command argument. */
    {
        typedef struct LiveEdgeDirectionCase {
            TecmoGameplayCourtCoordinate position;
            int16_t probe_horizontal;
            int16_t probe_depth;
        } LiveEdgeDirectionCase;
        static const LiveEdgeDirectionCase edge_cases[3] = {
            {{TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - 120 / 2, 120}, -64, 0},
            {{TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + 120 / 2, 120}, 64, 0},
            {{TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE -
                  TECMO_GAMEPLAY_COURT_WORLD_MAX_Y / 2,
              TECMO_GAMEPLAY_COURT_WORLD_MAX_Y}, -32, 64}
        };
        size_t edge_case;
        const uint8_t edge_actor = 1U;
        TecmoGameplaySceneLaunch edge_launch = cpu_only;
        edge_launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
        edge_launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        for (edge_case = 0U; edge_case < 3U; ++edge_case) {
            uint8_t edge_direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            TecmoGameplaySceneActor edge_actors_before[10];
            TecmoGameplayLiveFoundation edge_foundation_before;
            TecmoGameplayCourtCoordinate edge_positions[10];
            TecmoGameplayCourtCoordinateQ8 edge_ball_before;
            TecmoGameplayCourtCoordinate edge_target;
            uint8_t edge_controlled_before[2];
            uint8_t edge_holder_before;
            uint8_t edge_orientation_before;
            uint8_t edge_possession_before;
            uint16_t edge_action_serial_before;
            bool edge_target_found;
            bool edge_update_ok;
            if (!found_deferred) {
                LIVE_FAIL("LIVE edge/corner source-direction fixture missing");
            }
            tecmo_gameplay_scene_test_set_skip_pretip(true);
            if (!tecmo_gameplay_scene_launch(scene, &edge_launch) ||
                !scene_sync_live_foundation(scene)) {
                LIVE_FAIL("LIVE edge/corner direction setup rejected");
            }
            if (!scene_ownership_valid(scene)) {
                LIVE_FAIL("LIVE edge/corner ownership setup rejected");
            }
            scene->actors[edge_actor].position =
                edge_cases[edge_case].position;
            scene->actors[edge_actor].anchor =
                edge_cases[edge_case].position;
            edge_foundation_before = scene->live_foundation;
            for (actor = 0U; actor < 10U; ++actor) {
                edge_positions[actor] = scene->actors[actor].position;
            }
            if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                    &scene->cpu_steering_assets,
                    edge_cases[edge_case].probe_horizontal,
                    edge_cases[edge_case].probe_depth,
                    &edge_direction)) {
                LIVE_FAIL("LIVE edge/corner direction quantizer rejected");
            }
            edge_target_found = scene_cpu_target_for_source_direction(
                &scene->cpu_steering_assets,
                &edge_cases[edge_case].position,
                edge_direction, &edge_target);
            if (edge_target_found) {
                LIVE_FAIL("LIVE edge/corner outward target was fabricated");
            }
            memset(&play_input, 0, sizeof(play_input));
            play_input.contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
            play_input.actor = edge_actor;
            play_input.step_budget = 1U;
            play_input.orientation_035a =
                scene->orientation_state.current_direction;
            memcpy(play_input.actor_position, edge_positions,
                   sizeof(edge_positions));
            /* The accepted source executor currently leaves its direction
               sentinel false; this bounded fixture injects a validated
               direction metadata record and uses a known deferred source
               record to exercise only the owned TGMO composition branch. */
            candidate_foundation = scene->live_foundation;
            candidate_foundation.play_state.stream_offset[edge_actor] =
                deferred_offset;
            candidate_foundation.last_step_offset[edge_actor] =
                deferred_offset;
            candidate_foundation.play_state.target_actor[edge_actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            candidate_foundation.play_state.target_x[edge_actor] = 0;
            candidate_foundation.play_state.target_depth[edge_actor] = 0;
            candidate_foundation.play_state.direction[edge_actor] =
                edge_direction;
            candidate_foundation.source_target_valid[edge_actor] = false;
            candidate_foundation.source_direction_valid[edge_actor] = true;
            candidate_foundation.source_direction[edge_actor] = edge_direction;
            if (!tecmo_gameplay_live_foundation_play_step(
                    &scene->cpu_steering_assets, &play_input,
                    &candidate_foundation, &play_result) ||
                !candidate_foundation.source_direction_valid[edge_actor] ||
                candidate_foundation.source_direction[edge_actor] !=
                    edge_direction ||
                    candidate_foundation.source_target_valid[edge_actor]) {
                LIVE_FAIL("LIVE edge/corner source-direction fixture missing");
            }
            /* Install the same source-backed deferred record for every actor
               so unrelated stream commands cannot make this production
               fixture fail. Only the edge actor carries the injected,
               validated direction. This is a deterministic adapter fixture,
               not a claim that the incomplete original reset path writes
               this exact direction record. */
            candidate_foundation = scene->live_foundation;
            for (actor = 0U; actor < 10U; ++actor) {
                candidate_foundation.play_state.stream_offset[actor] =
                    deferred_offset;
                candidate_foundation.last_step_offset[actor] =
                    deferred_offset;
                candidate_foundation.play_state.target_actor[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
                candidate_foundation.play_state.target_x[actor] = 0;
                candidate_foundation.play_state.target_depth[actor] = 0;
                candidate_foundation.play_state.direction[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
                candidate_foundation.source_target_valid[actor] = false;
                candidate_foundation.source_direction_valid[actor] = false;
                candidate_foundation.source_direction[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
                candidate_foundation.deferred[actor] = false;
            }
            candidate_foundation.play_state.direction[edge_actor] =
                edge_direction;
            candidate_foundation.source_direction_valid[edge_actor] = true;
            candidate_foundation.source_direction[edge_actor] = edge_direction;
            if (!tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &candidate_foundation)) {
                LIVE_FAIL("LIVE edge/corner injected foundation was invalid");
            }
            scene->live_foundation = candidate_foundation;
            /* Poison a previously valid target record. The real scene tick
               must clear it when the source direction has no legal in-court
               target; a direct helper assertion alone cannot prove this. */
            scene->cpu_actors[edge_actor].decision_serial = 7U;
            scene->cpu_actors[edge_actor].snapshot_fingerprint = 0x12345678U;
            scene->cpu_actors[edge_actor].target_position = positions[0U];
            scene->cpu_actors[edge_actor].target_kind =
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
            scene->cpu_actors[edge_actor].direction =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            scene->cpu_actors[edge_actor].held_direction_bits =
                TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            scene->cpu_actors[edge_actor].target_valid = true;
            scene->cpu_actors[edge_actor].writes_direction = false;
            memcpy(edge_actors_before, scene->actors,
                   sizeof(edge_actors_before));
            edge_ball_before = scene->ball_position;
            edge_holder_before = scene->ball_holder;
            edge_orientation_before =
                scene->orientation_state.current_direction;
            edge_possession_before = (uint8_t)scene->state.possession;
            edge_action_serial_before = scene->action_serial;
            memcpy(edge_controlled_before, scene->controlled_actor,
                   sizeof(edge_controlled_before));
            edge_foundation_before = scene->live_foundation;
            memset(&shot_request, 0, sizeof(shot_request));
            edge_update_ok = scene_update_ai(scene, &shot_request);
            if (!edge_update_ok || shot_request.requested ||
                shot_request.playback_supported || shot_request.deferred ||
                memcmp(scene->actors, edge_actors_before,
                       sizeof(edge_actors_before)) != 0 ||
                memcmp(&scene->ball_position, &edge_ball_before,
                       sizeof(edge_ball_before)) != 0 ||
                scene->ball_holder != edge_holder_before ||
                scene->orientation_state.current_direction !=
                    edge_orientation_before ||
                (uint8_t)scene->state.possession != edge_possession_before ||
                scene->action_serial != edge_action_serial_before ||
                memcmp(scene->controlled_actor, edge_controlled_before,
                       sizeof(edge_controlled_before)) != 0 ||
                scene->live_foundation.orientation !=
                    edge_foundation_before.orientation ||
                scene->live_foundation.last_possession !=
                    edge_foundation_before.last_possession ||
                scene->live_foundation.last_ball_holder !=
                    edge_foundation_before.last_ball_holder ||
                scene->live_foundation.primary_actor !=
                    edge_foundation_before.primary_actor ||
                scene->live_foundation.defender_actor !=
                    edge_foundation_before.defender_actor ||
                !scene->live_foundation.source_direction_valid[edge_actor] ||
                scene->live_foundation.source_direction[edge_actor] !=
                    edge_direction ||
                scene->live_foundation.play_state.direction[edge_actor] !=
                    edge_direction ||
                scene->live_foundation.source_target_valid[edge_actor] ||
                scene->live_foundation.play_state.target_actor[edge_actor] !=
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
                scene->live_foundation.play_state.target_x[edge_actor] != 0 ||
                scene->live_foundation.play_state.target_depth[edge_actor] != 0 ||
                !scene->live_foundation.deferred[edge_actor] ||
                scene->cpu_actors[edge_actor].decision_serial != 0U ||
                scene->cpu_actors[edge_actor].snapshot_fingerprint != 0U ||
                scene->cpu_actors[edge_actor].target_position.x != 0 ||
                scene->cpu_actors[edge_actor].target_position.y != 0 ||
                scene->cpu_actors[edge_actor].target_kind !=
                    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT ||
                scene->cpu_actors[edge_actor].direction !=
                    TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
                scene->cpu_actors[edge_actor].held_direction_bits !=
                    TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL ||
                scene->cpu_actors[edge_actor].target_valid ||
                scene->cpu_actors[edge_actor].writes_direction ||
                scene->cpu_actors[edge_actor].linked_actor !=
                    scene->live_foundation.play_state.fixed_link[edge_actor] ||
                !scene_ownership_valid(scene)) {
                (void)snprintf(
                    message, message_size,
                    "LIVE edge/corner production inert mismatch case=%u update=%u status=%s phase=%u foundation=%u source=%u/%u/%u target=%u/%d,%d deferred=%u cpu=%u/%u/%d,%d/%u/%u/%u/%u holder=%u/%u possession=%u/%u",
                    (unsigned)edge_case, edge_update_ok ? 1U : 0U,
                    scene->status, (unsigned)scene->state.phase,
                    tecmo_gameplay_live_foundation_valid(
                        &scene->cpu_steering_assets,
                        &scene->live_foundation) ? 1U : 0U,
                    scene->live_foundation.source_direction_valid[edge_actor]
                        ? 1U : 0U,
                    (unsigned)scene->live_foundation.source_direction[edge_actor],
                    scene->live_foundation.play_state.direction[edge_actor],
                    scene->live_foundation.source_target_valid[edge_actor]
                        ? 1U : 0U,
                    (int)scene->live_foundation.play_state.target_x[edge_actor],
                    (int)scene->live_foundation.play_state.target_depth[edge_actor],
                    scene->live_foundation.deferred[edge_actor] ? 1U : 0U,
                    (unsigned)scene->cpu_actors[edge_actor].decision_serial,
                    (unsigned)scene->cpu_actors[edge_actor].target_valid,
                    (int)scene->actors[edge_actor].position.x,
                    (int)scene->actors[edge_actor].position.y,
                    (unsigned)scene->cpu_actors[edge_actor].target_kind,
                    (unsigned)scene->cpu_actors[edge_actor].direction,
                    (unsigned)scene->cpu_actors[edge_actor].held_direction_bits,
                    scene->cpu_actors[edge_actor].writes_direction ? 1U : 0U,
                    (unsigned)scene->ball_holder,
                    (unsigned)edge_holder_before,
                    (unsigned)scene->state.possession,
                    (unsigned)edge_possession_before);
                tecmo_gameplay_scene_test_set_skip_pretip(false);
                return false;
            }
            /* Relaunch before the next case and before the following stale
               metadata regression; the edge coordinate is intentionally not
               carried into another production fixture. */
            tecmo_gameplay_scene_test_set_skip_pretip(true);
            if (!tecmo_gameplay_scene_launch(scene, &edge_launch) ||
                !scene_sync_live_foundation(scene)) {
                LIVE_FAIL("LIVE edge/corner clean reset rejected");
            }
        }
    }

    if (!found_deferred) LIVE_FAIL("LIVE stale CPU metadata fixture unavailable");
    candidate_foundation = scene->live_foundation;
    candidate_foundation.play_state.stream_offset[1U] = deferred_offset;
    candidate_foundation.last_step_offset[1U] = deferred_offset;
    candidate_foundation.play_state.target_actor[1U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[1U] = 0;
    candidate_foundation.play_state.target_depth[1U] = 0;
    candidate_foundation.source_target_valid[1U] = false;
    scene->live_foundation = candidate_foundation;
    scene->cpu_actors[1U].decision_serial = 7U;
    scene->cpu_actors[1U].snapshot_fingerprint = 0x12345678U;
    scene->cpu_actors[1U].target_position = positions[0U];
    scene->cpu_actors[1U].target_kind =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
    scene->cpu_actors[1U].direction =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    scene->cpu_actors[1U].held_direction_bits =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    scene->cpu_actors[1U].target_valid = true;
    scene->cpu_actors[1U].writes_direction = false;
    scene->actors[0U].movement_boundary_latched = true;
    foundation_before = scene->live_foundation;
    memset(&shot_request, 0, sizeof(shot_request));
    {
        bool stale_update_ok = scene_update_ai(scene, &shot_request);
        if (!stale_update_ok || shot_request.requested ||
        scene->cpu_actors[1U].decision_serial != 0U ||
        scene->cpu_actors[1U].snapshot_fingerprint != 0U ||
        scene->cpu_actors[1U].target_position.x != 0 ||
        scene->cpu_actors[1U].target_position.y != 0 ||
        scene->cpu_actors[1U].target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT ||
        scene->cpu_actors[1U].direction !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
        scene->cpu_actors[1U].held_direction_bits !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL ||
        scene->cpu_actors[1U].target_valid ||
        scene->cpu_actors[1U].writes_direction ||
        scene->live_foundation.last_shot_request ||
        scene->live_foundation.last_shot_deferred ||
        scene->live_foundation.last_shot_playback_supported) {
            (void)snprintf(
                message, message_size,
                "LIVE deferred metadata mismatch update=%u status=%s cpu=%lu/%lu/%d,%d/%u/%u/%u/%u shot=%u/%u/%u req=%u",
                stale_update_ok ? 1U : 0U,
                scene->status,
                (unsigned long)scene->cpu_actors[1U].decision_serial,
                (unsigned long)scene->cpu_actors[1U].snapshot_fingerprint,
                (int)scene->cpu_actors[1U].target_position.x,
                (int)scene->cpu_actors[1U].target_position.y,
                (unsigned)scene->cpu_actors[1U].target_kind,
                (unsigned)scene->cpu_actors[1U].direction,
                (unsigned)scene->cpu_actors[1U].held_direction_bits,
                scene->cpu_actors[1U].target_valid ? 1U : 0U,
                scene->live_foundation.last_shot_request ? 1U : 0U,
                scene->live_foundation.last_shot_deferred ? 1U : 0U,
                scene->live_foundation.last_shot_playback_supported ? 1U : 0U,
                shot_request.requested ? 1U : 0U);
            tecmo_gameplay_scene_test_set_skip_pretip(false);
            return false;
        }
    }
    scene->actors[0U].movement_boundary_latched = true;
    foundation_before = scene->live_foundation;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(scene, &shot_request) || shot_request.requested ||
        scene->live_foundation.last_shot_request ||
        scene->live_foundation.last_shot_deferred) {
        LIVE_FAIL("LIVE boundary latch did not suppress shot evaluation");
    }
    (void)foundation_before;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &cpu_only)) {
        LIVE_FAIL("LIVE unsupported-shot fixture launch rejected");
    }
    scene->actors[0U].position.x =
        scene->orientation_state.offensive_hoop.x;
    scene->actors[0U].position.y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
    scene->actors[0U].movement_boundary_latched = false;
    candidate_foundation = scene->live_foundation;
    if (!found_deferred) LIVE_FAIL("LIVE unsupported-shot fixture unavailable");
    candidate_foundation.play_state.stream_offset[0U] = deferred_offset;
    candidate_foundation.last_step_offset[0U] = deferred_offset;
    candidate_foundation.play_state.target_actor[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[0U] = 0;
    candidate_foundation.play_state.target_depth[0U] = 0;
    candidate_foundation.source_target_valid[0U] = false;
    scene->live_foundation = candidate_foundation;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(scene, &shot_request) || shot_request.requested ||
        !shot_request.deferred || !scene->live_foundation.last_shot_request ||
        !scene->live_foundation.last_shot_deferred ||
        scene->live_foundation.last_shot_playback_supported ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        LIVE_FAIL("LIVE unsupported shot playback was not classified");
    }

    /* Both autonomous sides must reject the controller-dependent far/jump
       profile without exposing a requested launch. The source predicate is
       still recorded as a deferred/non-launch classification, and the
       existing shots.c action serial remains untouched. */
    for (size_t far_case = 0U; far_case < TECMO_GAMEPLAY_TEAM_COUNT;
         ++far_case) {
        uint8_t far_holder = far_case == TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 5U;
        uint32_t action_before;
        TecmoGameplayTeam far_team = (TecmoGameplayTeam)far_case;
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(scene, far_team, far_holder) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL(far_case == TECMO_GAMEPLAY_TEAM_AWAY
                          ? "LIVE away far-shot setup rejected"
                          : "LIVE home far-shot setup rejected");
        }
        scene->actors[far_holder].position.x = (int16_t)(
            scene->orientation_state.offensive_hoop.x + 8);
        scene->actors[far_holder].position.y =
            TECMO_GAMEPLAY_COURT_WORLD_MAX_Y - 15;
        scene->actors[far_holder].anchor = scene->actors[far_holder].position;
        scene->state.shot_clock = 12U;
        scene->state.clock_divider = 1U;
        scene->actors[far_holder].movement_boundary_latched = false;
        candidate_foundation = scene->live_foundation;
        candidate_foundation.play_state.stream_offset[far_holder] =
            deferred_offset;
        candidate_foundation.last_step_offset[far_holder] = deferred_offset;
        candidate_foundation.play_state.target_actor[far_holder] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        candidate_foundation.play_state.target_x[far_holder] = 0;
        candidate_foundation.play_state.target_depth[far_holder] = 0;
        candidate_foundation.source_target_valid[far_holder] = false;
        scene->live_foundation = candidate_foundation;
        if (!scene_attach_ball(scene)) {
            LIVE_FAIL(far_case == TECMO_GAMEPLAY_TEAM_AWAY
                          ? "LIVE away far-shot ball setup rejected"
                          : "LIVE home far-shot ball setup rejected");
        }
        action_before = scene->action_serial;
        shot_request.requested = true;
        shot_request.actor_index = 1U;
        shot_request.playback_supported = true;
        shot_request.deferred = false;
        if (!scene_update_ai(scene, &shot_request) ||
            shot_request.requested ||
            shot_request.actor_index != far_holder ||
            shot_request.playback_supported || !shot_request.deferred ||
            !scene->live_foundation.last_shot_request ||
            !scene->live_foundation.last_shot_deferred ||
            scene->live_foundation.last_shot_playback_supported ||
            scene->action_serial != action_before ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            LIVE_FAIL(far_case == TECMO_GAMEPLAY_TEAM_AWAY
                          ? "LIVE away far-shot launch was not deferred"
                          : "LIVE home far-shot launch was not deferred");
        }
    }

    /* A separate close-range CPU fixture proves the positive scene adapter:
       the deterministic TGAI predicate requests once, the existing shots.c
       playback accepts it once, and a repeat while the shot is active cannot
       create a second action. The caller workspaces remain native
       approximations; the playback/classification seam itself is tested. */
    {
        TecmoGameplaySceneLaunch close_shot_launch = bound;
        TecmoControlFrame repeat_p1;
        TecmoControlFrame repeat_p2;
        TecmoGameplayCourtCoordinate close_position;
        uint32_t action_before;
        close_shot_launch.controller_team[0U] =
            TECMO_GAMEPLAY_SCENE_NO_TEAM;
        close_shot_launch.controller_team[1U] =
            TECMO_GAMEPLAY_SCENE_NO_TEAM;
        close_shot_launch.difficulty = 0U;
        close_shot_launch.game_music_enabled = false;
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &close_shot_launch) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE supported close-shot setup rejected");
        }
        close_position.x = (int16_t)(
            scene->orientation_state.offensive_hoop.x + 8);
        close_position.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
        if (!scene_actor_coordinate_valid(&close_position)) {
            LIVE_FAIL("LIVE supported close-shot coordinate was invalid");
        }
        scene->actors[0U].position = close_position;
        scene->actors[0U].anchor = close_position;
        scene->state.shot_clock = 12U;
        scene->state.clock_divider = 1U;
        if (!scene_attach_ball(scene)) {
            LIVE_FAIL("LIVE supported close-shot ball attachment failed");
        }
        action_before = scene->action_serial;
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) ||
            !shot_request.requested ||
            shot_request.actor_index != 0U ||
            !shot_request.playback_supported ||
            shot_request.deferred ||
            scene->action_serial != action_before + 1U ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != 0U ||
            !scene->live_foundation.last_shot_request ||
            !scene->live_foundation.last_shot_playback_supported ||
            scene->live_foundation.last_shot_deferred ||
            scene->live_foundation.last_shot_actor != 0U) {
            LIVE_FAIL("LIVE supported close-shot request/playback fixture failed");
        }
        action_before = scene->action_serial;
        memset(&repeat_p1, 0, sizeof(repeat_p1));
        memset(&repeat_p2, 0, sizeof(repeat_p2));
        /* Playback owns the next update after launch; the holder is already
           NO_ACTOR, so calling scene_update_ai directly would be an invalid
           entry rather than a repeat. */
        if (!tecmo_gameplay_scene_update(scene, &repeat_p1, &repeat_p2) ||
            scene->action_serial != action_before ||
            scene->shot_actor != 0U ||
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            LIVE_FAIL("LIVE supported close-shot repeat was not transactional");
        }
    }

    /* Sustained bound running-clock integration: 120 neutral outer updates
       cover two game-clock seconds at the native 45-frame divider and are
       long enough to traverse multiple accepted source records, deferred
       effects, and any bounded close-shot playback selected by the normal
       adapter. Every update must retain roster/condition identity and scene
       ownership; action_serial may advance for a real launch but never more
       than once in one outer update. */
    {
        TecmoGameplaySceneLaunch sustained = bound;
        TecmoControlFrame sustained_p1;
        TecmoControlFrame sustained_p2;
        uint16_t clock_before;
        uint16_t clock_after;
        uint16_t action_before;
        uint16_t action_delta;
        size_t update;
        sustained.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        sustained.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        sustained.game_music_enabled = false;
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &sustained) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE sustained running-clock launch rejected");
        }
        scene->state.clock_minutes = 2U;
        scene->state.clock_seconds = 0U;
        scene->state.clock_divider = 1U;
        scene->state.shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
        clock_before = (uint16_t)(scene->state.clock_minutes * 60U +
                                  scene->state.clock_seconds);
        memset(&sustained_p1, 0, sizeof(sustained_p1));
        memset(&sustained_p2, 0, sizeof(sustained_p2));
        for (update = 0U; update < 120U; ++update) {
            action_before = scene->action_serial;
            if (!tecmo_gameplay_scene_update(
                    scene, &sustained_p1, &sustained_p2)) {
                LIVE_FAIL("LIVE sustained running-clock update rejected");
            }
            action_delta = (uint16_t)(scene->action_serial - action_before);
            if (action_delta > 1U || !scene_ownership_valid(scene)) {
                LIVE_FAIL("LIVE sustained running-clock duplicate launch/ownership failure");
            }
            for (actor = 0U; actor < 10U; ++actor) {
                uint8_t side = actor < 5U
                    ? TECMO_GAMEPLAY_TEAM_AWAY : TECMO_GAMEPLAY_TEAM_HOME;
                uint8_t local = (uint8_t)(actor % 5U);
                uint8_t expected_roster =
                    scene->launch.starter_roster_index[side][local];
                if (scene->actors[actor].roster_index != expected_roster ||
                    scene->actors[actor].condition !=
                        scene->fatigue_state.condition[side][expected_roster]) {
                    LIVE_FAIL("LIVE sustained running-clock roster drifted");
                }
            }
        }
        clock_after = (uint16_t)(scene->state.clock_minutes * 60U +
                                 scene->state.clock_seconds);
        if (clock_after >= clock_before ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE sustained running-clock did not progress coherently");
        }
    }

    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &bound) ||
        !scene_sync_live_foundation(scene)) {
        LIVE_FAIL("LIVE deterministic repeat launch rejected");
    }
    for (actor = 0U; actor < 10U; ++actor) {
        if (scene->actors[actor].position.x != exact_positions[actor].x ||
            scene->actors[actor].position.y != exact_positions[actor].y) {
            LIVE_FAIL("LIVE deterministic bound relaunch diverged");
        }
    }
    if (!scene_ownership_valid(scene)) {
        LIVE_FAIL("LIVE ownership baseline rejected after synchronization");
    }
    malformed_scene = *scene;
    malformed_scene.live_foundation.orientation =
        (uint8_t)(scene->orientation_state.current_direction ^ 1U);
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership orientation mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.live_foundation.last_possession =
        scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY
            ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_TEAM_AWAY;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership possession mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_HOME;
    malformed_scene.controlled_actor[0U] = 5U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership controller-team mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.controlled_actor[0U] = 1U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership controlled-slot mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.ball_holder = 1U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership holder/primary/defender mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.live_foundation.defender_actor = 0U;
    malformed_scene.live_foundation.play_state.defender_actor = 0U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE ownership defender mismatch was accepted");
    }
    malformed_scene = *scene;
    malformed_scene.cpu_actors[0U].linked_actor = 6U;
    if (scene_ownership_valid(&malformed_scene)) {
        LIVE_FAIL("LIVE CPU fixed-link ownership mismatch was accepted");
    }
    /* The preceding accepted suites enter state-flow with direct launches
       already configured to skip PRETIP. Preserve that harness contract for
       the following legacy state-flow suites; the orchestrator owns the
       final PASS-path reset to false. */
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    /* This helper is additive. Restore the incoming launch and input bytes so
       the unchanged downstream state-flow suites retain their legacy-direct
       fixture; bound LIVE behavior is exercised only inside this helper. */
    *launch_input = legacy_launch;
    *p1_input = legacy_p1;
    *p2_input = legacy_p2;
#undef LIVE_FAIL
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
        !scene_test_live_foundation_regressions(
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
