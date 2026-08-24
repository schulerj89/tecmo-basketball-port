#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scene_test_enable_captured_play_inputs(
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (input == NULL) return;
    /* Direct executor fixtures below are explicit synthetic captures. They
       intentionally exercise bounded source handlers; production scene input
       leaves these false unless it has a typed owner. */
    input->common_tail_ba_available = true;
    input->actor_046e_probe_available = true;
    input->opcode21_gate_inputs_available = true;
    input->special_actor_07df_available = true;
    input->special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_actor_branch_context_available = true;
}

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
        scene->orientation_state.attack_direction !=
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

static bool scene_test_ot_ordinary_transitions_do_not_reseed(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayScene *probe;
    TecmoGameplayFoulRequest foul;
    TecmoGameplayTeam other_team;
    uint8_t overtime_count;
    uint32_t seed_serial;
    bool ok = false;

    if (scene == NULL || scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.period != 5U || scene->state.overtime_count == 0U) {
        return false;
    }
    probe = (TecmoGameplayScene *)malloc(sizeof(*probe));
    if (probe == NULL) return false;
    overtime_count = scene->live_foundation.overtime_entry_last_applied_count;
    seed_serial = scene->live_foundation.regulation_entry_seed_serial;
    other_team = scene_other_team(scene->state.possession);

    *probe = *scene;
    if (!scene_begin_inbound(probe, scene->state.possession) ||
        probe->live_foundation.overtime_entry_last_applied_count !=
            overtime_count ||
        probe->live_foundation.regulation_entry_seed_serial != seed_serial) {
        goto done;
    }

    *probe = *scene;
    if (!scene_begin_scored_inbound(probe, other_team) ||
        probe->live_foundation.overtime_entry_last_applied_count !=
            overtime_count ||
        probe->live_foundation.regulation_entry_seed_serial != seed_serial) {
        goto done;
    }

    *probe = *scene;
    foul.fouling_team = other_team;
    foul.free_throw_team = scene->state.possession;
    foul.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    foul.player_index = 0U;
    foul.free_throw_attempts = 1U;
    if (!tecmo_gameplay_request_foul(&probe->state, &foul) ||
        probe->live_foundation.overtime_entry_last_applied_count !=
            overtime_count ||
        probe->live_foundation.regulation_entry_seed_serial != seed_serial) {
        goto done;
    }
    ok = true;

done:
    free(probe);
    return ok;
}

static bool scene_test_player_stats_contract(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch;
    TecmoGameplaySceneLaunch malformed_launch;
    TecmoGameplayScene snapshot;
    TecmoGameplayScene failed_shot;
    TecmoGameplayScene failed_shot_before;
    TecmoPlayerStatsGameLedger ledger;
    TecmoPlayerStatsGameLedger live_stats_before;
    TecmoPlayerStatsGameLedger rejected;
    TecmoPlayerStatsGameLedger rejected_before;
    TecmoGameplayScene threshold_before;
    TecmoGameplayScene threshold_after;
    TecmoControlFrame neutral;
    if (scene == NULL || base_launch == NULL) return false;
    if (scene->active) tecmo_gameplay_scene_end(scene);

    launch = *base_launch;
    malformed_launch = launch;
    malformed_launch.home_team = malformed_launch.away_team;
    snapshot = *scene;
    if (tecmo_gameplay_scene_launch(scene, &malformed_launch) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "player-stats failed launch mutated scene");
        return false;
    }

    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        tecmo_gameplay_scene_test_message(
            message, message_size, "player-stats launch rejected");
        return false;
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (scene->player_stats.coverage !=
            TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE ||
        !tecmo_player_stats_game_ledger_valid(&scene->player_stats)) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "player-stats launch ledger invalid");
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    live_stats_before = scene->player_stats;
    failed_shot = *scene;
    failed_shot.actors[0U].roster_index = TECMO_PLAYER_STATS_ROSTER_COUNT;
    failed_shot_before = failed_shot;
    if (scene_start_shot_actor(&failed_shot, 0U, 0U) ||
        memcmp(&failed_shot, &failed_shot_before,
               sizeof(failed_shot)) != 0 ||
        memcmp(&failed_shot.player_stats, &live_stats_before,
               sizeof(live_stats_before)) != 0)
        goto player_stats_failure;

    tecmo_player_stats_game_ledger_initialize(&ledger);
    for (uint8_t side = 0U;
         side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT; ++side)
        for (uint8_t roster = 0U;
             roster < TECMO_PLAYER_STATS_ROSTER_COUNT; ++roster) {
            if (!tecmo_player_stats_record_shot_attempt(
                    &ledger, side, roster, 2U) ||
                !tecmo_player_stats_record_shot_make(
                    &ledger, side, roster, 2U) ||
                !tecmo_player_stats_record_shot_attempt(
                    &ledger, side, roster, 3U) ||
                !tecmo_player_stats_record_shot_make(
                    &ledger, side, roster, 3U) ||
                !tecmo_player_stats_record_free_throw(
                    &ledger, side, roster, false) ||
                !tecmo_player_stats_record_free_throw(
                    &ledger, side, roster, true) ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_FGA] != 2U ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_FGM] != 2U ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PA] != 1U ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PM] != 1U ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_FTA] != 2U ||
                ledger.counters[side][roster][
                    TECMO_PLAYER_STATS_COUNTER_FTM] != 1U)
                goto player_stats_failure;
        }
    ledger.counters[0U][0U][TECMO_PLAYER_STATS_COUNTER_FGA] = 0xFFU;
    if (!tecmo_player_stats_record_shot_attempt(
            &ledger, 0U, 0U, 2U) ||
        ledger.counters[0U][0U][TECMO_PLAYER_STATS_COUNTER_FGA] != 0U)
        goto player_stats_failure;
    for (uint8_t side = 0U;
         side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT; ++side)
        for (uint8_t roster = 0U;
             roster < TECMO_PLAYER_STATS_ROSTER_COUNT; ++roster)
            for (uint8_t counter = TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT;
                 counter < TECMO_PLAYER_STATS_COUNTER_DIMENSION; ++counter)
                if (ledger.counters[side][roster][counter] != 0U)
                    goto player_stats_failure;
    if (!tecmo_player_stats_game_ledger_valid(&ledger))
        goto player_stats_failure;

    tecmo_player_stats_game_ledger_clear(&rejected);
    rejected_before = rejected;
    if (tecmo_player_stats_record_shot_attempt(&rejected, 0U, 0U, 2U) ||
        tecmo_player_stats_record_shot_attempt(&rejected, 0U, 0U, 3U) ||
        tecmo_player_stats_record_shot_make(&rejected, 0U, 0U, 2U) ||
        tecmo_player_stats_record_shot_make(&rejected, 0U, 0U, 3U) ||
        tecmo_player_stats_record_free_throw(&rejected, 0U, 0U, false) ||
        tecmo_player_stats_record_free_throw(&rejected, 0U, 0U, true) ||
        memcmp(&rejected, &rejected_before, sizeof(rejected)) != 0)
        goto player_stats_failure;
    rejected.coverage = 1U;
    rejected_before = rejected;
    if (tecmo_player_stats_record_shot_attempt(&rejected, 0U, 0U, 2U) ||
        tecmo_player_stats_record_shot_attempt(&rejected, 0U, 0U, 3U) ||
        tecmo_player_stats_record_shot_make(&rejected, 0U, 0U, 2U) ||
        tecmo_player_stats_record_shot_make(&rejected, 0U, 0U, 3U) ||
        tecmo_player_stats_record_free_throw(&rejected, 0U, 0U, false) ||
        tecmo_player_stats_record_free_throw(&rejected, 0U, 0U, true) ||
        memcmp(&rejected, &rejected_before, sizeof(rejected)) != 0)
        goto player_stats_failure;

    tecmo_gameplay_scene_end(scene);
    launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        goto player_stats_failure;
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!scene_test_enter_free_throw_sequence(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 1U))
        goto player_stats_failure;
    scene->free_throw_frame =
        (uint16_t)(TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES - 1U);
    scene->player_stats.coverage = 0U;
    threshold_before = *scene;
    memset(&neutral, 0, sizeof(neutral));
    if (tecmo_gameplay_scene_update(scene, &neutral, &neutral))
        goto player_stats_failure;
    threshold_after = *scene;
    memcpy(threshold_after.status, threshold_before.status,
           sizeof(threshold_after.status));
    if (memcmp(&threshold_after, &threshold_before,
               sizeof(threshold_before)) != 0)
        goto player_stats_failure;
    tecmo_gameplay_scene_end(scene);
    return true;

player_stats_failure:
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (scene->active) tecmo_gameplay_scene_end(scene);
    tecmo_gameplay_scene_test_message(
        message, message_size, "player-stats gameplay contract failed");
    return false;
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
        scene->audio_player.sfx_pending) {
        char failure[384];
        (void)snprintf(
            failure, sizeof(failure),
            "TGMO/TPNL out-of-bounds entry failed: phase=%u violation=%u restart=%u x=%d latch=%u holder=%u sfx_pending=%u/%u control=%u team=%u action=%u direction=%u shot=%u",
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
        scene->inbound_state.phase != TECMO_GAMEPLAY_SCENE_INBOUND_SETUP ||
        scene->inbound_state.restart_team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->inbound_state.passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->inbound_state.receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->ball_holder != scene->inbound_state.passer ||
        scene->actors[0U].movement_boundary_latched) {
        {
            char failure[384];
            (void)snprintf(
                failure, sizeof(failure),
                "out-of-bounds restart settlement failed phase=%u possession=%u holder=%u inbound=%u passer=%u receiver=%u defender=%u direction=%u status=%s",
                (unsigned)scene->state.phase,
                (unsigned)scene->state.possession,
                (unsigned)scene->ball_holder,
                (unsigned)scene->inbound_state.phase,
                (unsigned)scene->inbound_state.passer,
                (unsigned)scene->inbound_state.receiver,
                (unsigned)scene->inbound_state.defender,
                (unsigned)scene->orientation_state.attack_direction,
                scene->status);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
        }
        return false;
    }
    {
        uint8_t receiver = scene->inbound_state.receiver;
        uint8_t clock_divider = scene->state.clock_divider;
        uint8_t shot_clock = scene->state.shot_clock;
        uint8_t clock_seconds = scene->state.clock_seconds;
        for (frame = 0U; frame < 64U && scene_inbound_active(scene); ++frame) {
            TecmoGameplayCourtCoordinate positions[
                TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
            for (size_t actor = 0U;
                 actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                positions[actor] = scene->actors[actor].position;
            }
            p1.held.left = true;
            p1.held.right = true;
            p1.pressed.shoot = true;
            p1.pressed.cancel = true;
            if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
                scene->state.clock_divider != clock_divider ||
                scene->state.shot_clock != shot_clock ||
                scene->state.clock_seconds != clock_seconds) {
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "inbound transport did not freeze state clocks");
                return false;
            }
            for (size_t actor = 0U;
                 actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                if (scene->actors[actor].position.x != positions[actor].x ||
                    scene->actors[actor].position.y != positions[actor].y) {
                    tecmo_gameplay_scene_test_message(
                        message, message_size,
                        "inbound transport allowed live actor mutation");
                    return false;
                }
            }
            memset(&p1, 0, sizeof(p1));
        }
        if (scene_inbound_active(scene) ||
            scene->ball_holder != receiver ||
            scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "out-of-bounds inbound catch transfer failed");
            return false;
        }
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
        scene->audio_player.pending_sfx_id !=
            TECMO_GAMEPLAY_SFX_EXPIRY_ID ||
        scene->audio_player.dmc.active ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "shot-clock violation reset/pre-delay ordering failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != TECMO_GAMEPLAY_SFX_EXPIRY_ID) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "shot-clock expiry buzzer consumption failed");
        return false;
    }
    for (frame = 0U; frame < 15U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.phase_frame != (uint16_t)(frame + 1U) ||
            scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "shot-clock violation queued SFX6 before frame 16");
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene->state.phase_frame != 16U ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 6U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "shot-clock violation did not queue SFX6 at frame 16");
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
    for (frame = 16U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.phase_frame != (uint16_t)(frame + 1U) ||
            scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "violation reset/cue repeated after frame 16");
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
    uint8_t expected_p3_primary;
    uint8_t expected_p3_defender;
    uint8_t expected_p4_primary;
    uint8_t expected_p4_defender;
    uint8_t expected_ot_primary;
    uint8_t expected_ot_defender;
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
    /* This legacy-direct period-expiry fixture starts after P1 setup. Bind
       the exact prior-entry lifecycle explicitly; organic PRETIP coverage
       owns the real P1 writer and separate regulation vectors own P1-P4. */
    scene->live_foundation.period_entry_selector =
        TECMO_GAMEPLAY_TEAM_HOME;
    scene->live_foundation.regulation_entry_seeded_period = 1U;
    scene->live_foundation.regulation_entry_seed_serial = 1U;
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
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 9U || scene->controlled_actor[0] != 0U ||
        scene->live_foundation.regulation_entry_seeded_period != 2U ||
        scene->live_foundation.regulation_entry_seed_serial != 2U ||
        scene->actors[0].position.x != x || scene->action_serial != 3U ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 5U ||
        !scene->audio_player.dmc.active ||
        !scene->audio_player.music->track_pending ||
        scene->audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        (void)snprintf(message, message_size,
            "period restart action suppression failed status=%s phase=%u possession=%u holder=%u control=%u x=%d/%d serial=%u/%u",
            scene->status, (unsigned)scene->state.phase,
            (unsigned)scene->state.possession, (unsigned)scene->ball_holder,
            (unsigned)scene->controlled_actor[0], scene->actors[0].position.x,
            x, (unsigned)scene->live_foundation.regulation_entry_seeded_period,
            (unsigned)scene->live_foundation.regulation_entry_seed_serial);
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
    /* Continue through the real P2 and P3 clock-expiry/banner seams. This
       proves the scene, rather than only the foundation vector, publishes
       the R,R,R^1 entries atomically and replaces the prior period's
       clamp owner without consuming the incoming selected-primary wait. */
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    for (frame = 0U; frame < 40U &&
         (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT); ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) break;
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        scene->state.period != 3U ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_HALFTIME) {
        (void)snprintf(message, message_size,
            "halftime expiry/banner transition failed status=%s phase=%u period=%u banner=%u clock=%u:%u/%u",
            scene->status, (unsigned)scene->state.phase,
            (unsigned)scene->state.period, (unsigned)scene->state.banner,
            (unsigned)scene->state.clock_minutes,
            (unsigned)scene->state.clock_seconds,
            (unsigned)scene->state.clock_divider);
        return false;
    }
    scene->state.phase_frame =
        TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES - 1U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "halftime score transition failed");
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "third-period banner entry failed");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    expected_p3_primary = scene->live_foundation.primary_actor;
    expected_p3_defender = scene->live_foundation.defender_actor;
    scene->state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    scene->live_foundation.play_state.wait_counter[expected_p3_primary] = 23U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != expected_p3_primary ||
        scene->orientation_state.attack_direction != 1U ||
        scene->live_foundation.offense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->live_foundation.primary_actor != expected_p3_primary ||
        scene->live_foundation.defender_actor != expected_p3_defender ||
        scene->live_foundation.regulation_entry_seeded_period != 3U ||
        scene->live_foundation.regulation_entry_seed_serial != 3U ||
        !scene->live_foundation.regulation_entry_clamp_exemption_active ||
        scene->live_foundation.regulation_entry_clamp_exempt_actor !=
            expected_p3_primary ||
        !scene_actor_world_position_valid(
            &scene->actors[expected_p3_defender]) ||
        scene->live_foundation.play_state.wait_counter[
            expected_p3_primary] != 23U) {
        (void)snprintf(message, message_size,
            "third-period regulation entry failed status=%s possession=%u holder=%u orientation=%u primary=%u defender=%u serial=%u/%u wait=%u",
            scene->status, (unsigned)scene->state.possession,
            (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.attack_direction,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation.regulation_entry_seeded_period,
            (unsigned)scene->live_foundation.regulation_entry_seed_serial,
            (unsigned)scene->live_foundation.play_state.wait_counter[
                expected_p3_primary]);
        return false;
    }
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    for (frame = 0U; frame < 40U &&
         (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT); ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) break;
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.period != 4U ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD) {
        (void)snprintf(message, message_size,
            "third-period expiry/banner transition failed status=%s phase=%u period=%u banner=%u clock=%u:%u/%u",
            scene->status, (unsigned)scene->state.phase,
            (unsigned)scene->state.period, (unsigned)scene->state.banner,
            (unsigned)scene->state.clock_minutes,
            (unsigned)scene->state.clock_seconds,
            (unsigned)scene->state.clock_divider);
        return false;
    }
    scene->state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    expected_p4_primary = scene->live_foundation.defender_actor;
    expected_p4_defender = scene->live_foundation.primary_actor;
    scene->live_foundation.play_state.wait_counter[expected_p4_primary] = 24U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != expected_p4_primary ||
        scene->orientation_state.attack_direction != 0U ||
        scene->live_foundation.offense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->live_foundation.primary_actor != expected_p4_primary ||
        scene->live_foundation.defender_actor != expected_p4_defender ||
        scene->live_foundation.regulation_entry_seeded_period != 4U ||
        scene->live_foundation.regulation_entry_seed_serial != 4U ||
        !scene->live_foundation.regulation_entry_clamp_exemption_active ||
        scene->live_foundation.regulation_entry_clamp_exempt_actor !=
            expected_p4_primary ||
        !scene_actor_world_position_valid(
            &scene->actors[expected_p4_defender]) ||
        scene->live_foundation.play_state.wait_counter[
            expected_p4_primary] != 24U) {
        (void)snprintf(message, message_size,
            "fourth-period regulation entry failed status=%s possession=%u holder=%u orientation=%u primary=%u defender=%u serial=%u/%u wait=%u",
            scene->status, (unsigned)scene->state.possession,
            (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.attack_direction,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation.regulation_entry_seeded_period,
            (unsigned)scene->live_foundation.regulation_entry_seed_serial,
            (unsigned)scene->live_foundation.play_state.wait_counter[
                expected_p4_primary]);
        return false;
    }
    if (scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            scene->state.score[TECMO_GAMEPLAY_TEAM_HOME]) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "overtime entry fixture was not tied");
        return false;
    }
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    for (frame = 0U; frame < 40U &&
         (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT); ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) break;
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.period != 5U || scene->state.overtime_count != 1U ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_OVERTIME) {
        (void)snprintf(message, message_size,
            "OT1 banner transition failed status=%s phase=%u period=%u overtime=%u banner=%u",
            scene->status, (unsigned)scene->state.phase,
            (unsigned)scene->state.period,
            (unsigned)scene->state.overtime_count,
            (unsigned)scene->state.banner);
        return false;
    }
    expected_ot_primary = scene->live_foundation.defender_actor;
    expected_ot_defender = scene->live_foundation.primary_actor;
    scene->live_foundation.play_state.wait_counter[expected_ot_primary] = 41U;
    scene->state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.period != 5U || scene->state.overtime_count != 1U ||
        scene->live_foundation.primary_actor != expected_ot_primary ||
        scene->live_foundation.defender_actor != expected_ot_defender ||
        scene->ball_holder != expected_ot_primary ||
        scene->live_foundation.offense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->orientation_state.attack_direction != 1U ||
        scene->live_foundation.overtime_entry_last_applied_count != 1U ||
        scene->live_foundation.overtime_entry_last_selector_raw != 0x01U ||
        scene->live_foundation.regulation_entry_seed_serial != 5U ||
        scene->live_foundation.play_state.wait_counter[
            expected_ot_primary] != 41U ||
        !scene->live_foundation.regulation_entry_clamp_exemption_active ||
        scene->live_foundation.regulation_entry_clamp_exempt_actor !=
            expected_ot_primary ||
        !scene_actor_world_position_valid(
            &scene->actors[expected_ot_defender])) {
        (void)snprintf(message, message_size,
            "OT1 period entry failed status=%s possession=%u holder=%u orientation=%u primary=%u defender=%u epoch=%u raw=%02X serial=%u wait=%u",
            scene->status, (unsigned)scene->state.possession,
            (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.attack_direction,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation
                .overtime_entry_last_applied_count,
            (unsigned)scene->live_foundation
                .overtime_entry_last_selector_raw,
            (unsigned)scene->live_foundation.regulation_entry_seed_serial,
            (unsigned)scene->live_foundation.play_state.wait_counter[
                expected_ot_primary]);
        return false;
    }
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    for (frame = 0U; frame < 40U &&
         (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT); ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) break;
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.period != 5U || scene->state.overtime_count != 2U ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_OVERTIME) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "OT2 banner transition failed");
        return false;
    }
    expected_ot_primary = scene->live_foundation.defender_actor;
    expected_ot_defender = scene->live_foundation.primary_actor;
    scene->live_foundation.play_state.wait_counter[expected_ot_primary] = 42U;
    scene->state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.overtime_count != 2U ||
        scene->live_foundation.primary_actor != expected_ot_primary ||
        scene->live_foundation.defender_actor != expected_ot_defender ||
        scene->ball_holder != expected_ot_primary ||
        scene->live_foundation.offense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->orientation_state.attack_direction != 0U ||
        scene->live_foundation.overtime_entry_last_applied_count != 2U ||
        scene->live_foundation.overtime_entry_last_selector_raw != 0x00U ||
        scene->live_foundation.regulation_entry_seed_serial != 6U ||
        scene->live_foundation.play_state.wait_counter[
            expected_ot_primary] != 42U ||
        !scene->live_foundation.regulation_entry_clamp_exemption_active ||
        scene->live_foundation.regulation_entry_clamp_exempt_actor !=
            expected_ot_primary ||
        !scene_actor_world_position_valid(
            &scene->actors[expected_ot_defender])) {
        (void)snprintf(message, message_size,
            "OT2 period entry failed status=%s possession=%u holder=%u orientation=%u primary=%u defender=%u epoch=%u raw=%02X serial=%u wait=%u",
            scene->status, (unsigned)scene->state.possession,
            (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.attack_direction,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation
                .overtime_entry_last_applied_count,
            (unsigned)scene->live_foundation
                .overtime_entry_last_selector_raw,
            (unsigned)scene->live_foundation.regulation_entry_seed_serial,
            (unsigned)scene->live_foundation.play_state.wait_counter[
                expected_ot_primary]);
        return false;
    }
    if (!tecmo_gameplay_set_score(
            &scene->state, TECMO_GAMEPLAY_TEAM_AWAY, 2U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "OT non-tie setup failed");
        return false;
    }
    if (!scene_test_ot_ordinary_transitions_do_not_reseed(scene)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "OT restart/inbound/foul transition reseeded entry epoch");
        return false;
    }
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 2U;
    scene->state.shot_clock = 20U;
    for (frame = 0U; frame < 40U &&
         (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
          scene->state.phase ==
              TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT); ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) break;
    }
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        scene->live_foundation.overtime_entry_last_applied_count != 2U ||
        scene->live_foundation.regulation_entry_seed_serial != 6U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "OT non-tie final seeded another epoch");
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
        (!scene->legacy_direct_launch &&
         (!scene_pass_active(scene) || scene->ball_holder != 0U ||
          scene->controlled_actor[0U] != 0U ||
          scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
          scene->action_serial != 1U)) ||
        (scene->legacy_direct_launch &&
         (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
          scene->shot_actor != 1U || scene->action_serial != 2U))) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "combined NES A+B did not retain passer ownership");
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
    if (scene->orientation_state.attack_direction != 0U ||
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
    zero_input.orientation = scene->orientation_state.attack_direction;
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
        scene->orientation_state.attack_direction != 1U ||
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
        scene->orientation_state.attack_direction != 1U ||
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

/* Exercise the production pre-tip handoff rather than injecting possession or
   any executor workspace.  The away pad remains assigned but idle, so the
   unassigned home side takes TPTI's own automatic jump route and becomes a
   genuine CPU ball holder. This proves the selected-primary command runs once
   before the ordinary loop while non-selected CPU motion remains active. */
static bool scene_test_pretip_cpu_common_tail_handoff(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    char *message,
    size_t message_size)
{
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame away;
    TecmoControlFrame home;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCourtCoordinate before[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint16_t stream_before = 0U;
    uint16_t last_step_before = 0U;
    bool reached_live = false;
    bool actor_moved = false;
    size_t update;
    size_t actor;
    char failure[256] = "PRETIP CPU handoff setup rejected";

    if (scene == NULL || base_launch == NULL) goto failed;
    launch = *base_launch;
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    memset(&away, 0, sizeof(away));
    memset(&home, 0, sizeof(home));
    memset(&command, 0, sizeof(command));
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        (void)snprintf(failure, sizeof(failure),
                       "PRETIP CPU handoff launch rejected: %s",
                       scene->status);
        goto failed;
    }

    /* No press/captured play input is supplied.  The home automatic decision
       is owned by TPTI; the idle human away side cannot win this fixture. */
    for (update = 0U; update < 2048U; ++update) {
        memset(&away, 0, sizeof(away));
        memset(&home, 0, sizeof(home));
        if (!tecmo_gameplay_scene_update(scene, &away, &home)) {
            (void)snprintf(failure, sizeof(failure),
                           "PRETIP CPU handoff update %u rejected: %s",
                           (unsigned)update, scene->status);
            goto failed;
        }
        if (!tecmo_gameplay_scene_in_pretip(scene)) {
            reached_live = true;
            break;
        }
    }
    if (!reached_live || scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->ball_holder != scene->pretip_state.receiver_actor ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->launch.controller_team[1U] !=
            TECMO_GAMEPLAY_SCENE_NO_TEAM ||
        scene->live_foundation.first_sync_pending ||
        scene->live_foundation.primary_actor != scene->ball_holder) {
        (void)snprintf(failure, sizeof(failure),
                       "PRETIP CPU handoff did not retain home CPU LIVE "
                       "owner phase=%u possession=%u holder=%u receiver=%u "
                       "controller=%u sync=%u",
                       (unsigned)scene->state.phase,
                       (unsigned)scene->state.possession,
                       (unsigned)scene->ball_holder,
                       (unsigned)scene->pretip_state.receiver_actor,
                       (unsigned)scene->launch.controller_team[1U],
                       scene->live_foundation.first_sync_pending ? 0U : 1U);
        goto failed;
    }
    holder = scene->ball_holder;
    {
        uint8_t side;
        uint8_t orientation;
        uint8_t slot;
        for (side = 0U; side < 2U; ++side) {
            for (orientation = 0U; orientation < 2U; ++orientation) {
                for (slot = 0U; slot < 5U; ++slot) {
                    TecmoGameplayLiveFoundation fixture =
                        scene->live_foundation;
                    TecmoGameplayLiveFoundation before_reject;
                    TecmoGameplayLiveFoundation malformed_fixture;
                    uint8_t primary = (uint8_t)(side * 5U + slot);
                    uint8_t expected_candidate =
                        (uint8_t)(side * 5U + (slot == 0U ? 1U : 0U));
                    uint16_t expected_stream[
                        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {0U};
                    uint8_t stream_choice_state = 0U;
                    int scan_y = 3;
                    int scan_actor;
                    size_t fixture_actor;
                    fixture.regulation_entry_seeded_period = 0U;
                    fixture.period_entry_selector =
                        TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                    fixture.regulation_entry_clamp_exemption_active = false;
                    fixture.regulation_entry_clamp_exempt_actor =
                        TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                    fixture.regulation_entry_seed_serial = 0U;
                    fixture.orientation = orientation;
                    fixture.last_possession = side;
                    fixture.offense_side = side;
                    fixture.defense_side = (uint8_t)(side ^ 1U);
                    fixture.primary_actor = primary;
                    fixture.play_state.primary_actor = primary;
                    fixture.last_ball_holder = primary;
                    fixture.defender_actor =
                        fixture.play_state.fixed_link[primary];
                    fixture.play_state.defender_actor = fixture.defender_actor;
                    fixture.selected_actor_by_side[side] = primary;
                    fixture.candidate_actor_by_side[side] =
                        (uint8_t)(side * 5U);
                    fixture.play_state.candidate_actor =
                        fixture.candidate_actor_by_side[side];
                    for (fixture_actor = 0U;
                         fixture_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                         ++fixture_actor) {
                        fixture.actor_selector_flags[fixture_actor] =
                            fixture.actor_team[fixture_actor] == side
                                ? 0U : 0x10U;
                        fixture.source_target_valid[fixture_actor] = false;
                        fixture.source_raw_target_valid[fixture_actor] = false;
                        fixture.source_inactive_target_storage[fixture_actor] =
                            false;
                        fixture.source_direction_valid[fixture_actor] = false;
                        fixture.source_direction[fixture_actor] =
                            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
                        fixture.play_state.direction[fixture_actor] =
                            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
                        fixture.play_state.route_motion[fixture_actor].active =
                            false;
                        if (fixture.actor_team[fixture_actor] == side) {
                            fixture.actor_position[fixture_actor].y =
                                ((fixture_actor + slot + orientation) & 1U) != 0U
                                    ? 0x0095 : 0x0096;
                        }
                    }
                    expected_stream[primary] = 0x017CU;
                    for (scan_actor = side == 0U ? 4 : 9;
                         scan_actor >= (side == 0U ? 0 : 5);
                         --scan_actor) {
                        bool choose_023a;
                        if ((uint8_t)scan_actor == primary) continue;
                        if (scan_y >= 2) {
                            choose_023a =
                                (fixture.actor_position[scan_actor].y >= 0x0096 &&
                                 stream_choice_state != 1U) ||
                                (fixture.actor_position[scan_actor].y < 0x0096 &&
                                 stream_choice_state == 2U);
                            expected_stream[scan_actor] =
                                choose_023a ? 0x023AU : 0x0226U;
                            stream_choice_state = choose_023a ? 1U : 2U;
                        } else {
                            expected_stream[scan_actor] =
                                scan_y == 1 ? 0x0208U : 0x0195U;
                        }
                        --scan_y;
                    }
                    before_reject = fixture;
                    if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 1U, side, side,
                            true, &fixture) ||
                        memcmp(&fixture, &before_reject,
                               sizeof(fixture)) != 0) {
                        (void)snprintf(failure, sizeof(failure),
                            "P1 claimant/selector mismatch rollback failed s%u",
                            (unsigned)side);
                        goto failed;
                    }
                    malformed_fixture = fixture;
                    malformed_fixture.source_direction_valid[primary] = true;
                    malformed_fixture.source_direction[primary] = 0U;
                    malformed_fixture.play_state.direction[primary] = 0U;
                    fixture = malformed_fixture;
                    if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 1U, side,
                            (uint8_t)(side ^ 1U), true,
                            &fixture) ||
                        memcmp(&fixture, &malformed_fixture,
                               sizeof(fixture)) != 0) {
                        (void)snprintf(failure, sizeof(failure),
                            "first-period stale-direction rollback failed");
                        goto failed;
                    }
                    fixture = before_reject;
                    if (!tecmo_gameplay_live_foundation_valid(
                            &scene->cpu_steering_assets, &fixture) ||
                        tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 1U, side,
                            (uint8_t)(side ^ 1U), false,
                            &fixture) ||
                        memcmp(&fixture, &before_reject,
                               sizeof(fixture)) != 0 ||
                        !tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 1U, side,
                            (uint8_t)(side ^ 1U), true,
                            &fixture) ||
                        fixture.primary_actor != primary ||
                        fixture.play_state.stream_offset[primary] != 0x017CU ||
                        fixture.play_state.actor_state[primary] != 0x04U ||
                        fixture.play_state.wait_counter[primary] != 0U ||
                        fixture.candidate_actor_by_side[side] !=
                            expected_candidate ||
                        fixture.play_state.candidate_actor !=
                            expected_candidate ||
                        fixture.regulation_entry_seeded_period != 1U ||
                        fixture.period_entry_selector !=
                            (uint8_t)(side ^ 1U) ||
                        !fixture.regulation_entry_clamp_exemption_active ||
                        fixture.regulation_entry_clamp_exempt_actor !=
                            primary) {
                        (void)snprintf(failure, sizeof(failure),
                            "first-period seed slot vector failed s%u o%u p%u",
                            (unsigned)side, (unsigned)orientation,
                            (unsigned)primary);
                        goto failed;
                    }
                    before_reject = fixture;
                    if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 2U, side,
                            fixture.period_entry_selector, true,
                            &fixture) ||
                        memcmp(&fixture, &before_reject,
                               sizeof(fixture)) != 0) {
                        (void)snprintf(failure, sizeof(failure),
                            "period selector target rollback failed s%u",
                            (unsigned)side);
                        goto failed;
                    }
                    for (fixture_actor = (size_t)side * 5U;
                         fixture_actor < (size_t)side * 5U + 5U;
                         ++fixture_actor) {
                        if (fixture.play_state.stream_offset[fixture_actor] !=
                                expected_stream[fixture_actor] ||
                            fixture.play_state.actor_state[fixture_actor] !=
                                0x04U) {
                            (void)snprintf(failure, sizeof(failure),
                                "first-period stream pairing failed a%u",
                                (unsigned)fixture_actor);
                            goto failed;
                        }
                    }
                    if (side == 0U && orientation == 0U && slot == 0U) {
                        TecmoGameplayLiveFoundation lifecycle = fixture;
                        TecmoGameplayLiveFoundation equality = fixture;
                        uint8_t preserved_actor = 2U;
                        uint8_t period;

                        lifecycle.source_target_valid[preserved_actor] = true;
                        lifecycle.play_state.target_x[preserved_actor] = 0x0180;
                        lifecycle.play_state.target_depth[preserved_actor] =
                            0x0090;
                        lifecycle.source_direction_valid[preserved_actor] = true;
                        lifecycle.source_direction[preserved_actor] = 3U;
                        lifecycle.play_state.direction[preserved_actor] = 3U;
                        lifecycle.play_state.wait_counter[preserved_actor] = 7U;
                        lifecycle.play_state.actor_state[preserved_actor] =
                            0x05U;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .horizontal_accumulator_q6 = 0x4567U;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .depth_accumulator_q6 = 0x2345U;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .horizontal_velocity_q6 = -32;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .depth_velocity_q6 = 16;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .remaining_timer = 9U;
                        lifecycle.play_state.route_motion[preserved_actor]
                            .active = true;
                        equality = lifecycle;
                        equality.offense_side = 1U;
                        equality.defense_side = 0U;
                        equality.last_possession = 1U;
                        equality.primary_actor = 5U;
                        equality.play_state.primary_actor = 5U;
                        equality.last_ball_holder = 5U;
                        equality.defender_actor = 0U;
                        equality.play_state.defender_actor = 0U;
                        equality.play_state.candidate_actor =
                            equality.candidate_actor_by_side[1U];
                        for (fixture_actor = 0U;
                             fixture_actor <
                                 TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                             ++fixture_actor) {
                            equality.actor_selector_flags[fixture_actor] ^=
                                0x10U;
                        }
                        equality.play_state.wait_counter[5U] = 13U;
                        for (fixture_actor = 0U;
                             fixture_actor <
                                 TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                             ++fixture_actor) {
                            equality.play_state.action_state_046e[fixture_actor] =
                                (uint8_t)(0x80U + fixture_actor);
                        }
                        before_reject = equality;
                        if (!tecmo_gameplay_live_foundation_valid(
                                &scene->cpu_steering_assets, &equality) ||
                            !tecmo_gameplay_live_foundation_regulation_entry_apply(
                                &scene->cpu_steering_assets, 2U, 1U,
                                equality.period_entry_selector, true,
                                &equality) ||
                            equality.primary_actor !=
                                before_reject.primary_actor ||
                            equality.defender_actor !=
                                before_reject.defender_actor ||
                            memcmp(equality.actor_selector_flags,
                                   before_reject.actor_selector_flags,
                                   sizeof(equality.actor_selector_flags)) != 0 ||
                            equality.play_state.actor_state[
                                equality.primary_actor] != 0x04U ||
                            equality.play_state.action[equality.primary_actor] !=
                                0x30U ||
                            equality.play_state.action[equality.defender_actor] !=
                                0x30U ||
                            equality.play_state.wait_counter[
                                equality.primary_actor] != 13U ||
                            equality.regulation_entry_seeded_period != 2U ||
                            equality.regulation_entry_seed_serial != 2U) {
                            (void)snprintf(failure, sizeof(failure),
                                "regulation equality atomic entry failed");
                            goto failed;
                        }
                        for (fixture_actor = 0U;
                             fixture_actor <
                                 TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                             ++fixture_actor) {
                            if (equality.play_state.action_state_046e[
                                    fixture_actor] != 0U) {
                                (void)snprintf(failure, sizeof(failure),
                                    "regulation equality BFA8 clear failed a%u",
                                    (unsigned)fixture_actor);
                                goto failed;
                            }
                        }
                        before_reject = equality;
                        if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                                &scene->cpu_steering_assets, 2U, 1U,
                                equality.period_entry_selector, true,
                                &equality) ||
                            memcmp(&equality, &before_reject,
                                   sizeof(equality)) != 0) {
                            (void)snprintf(failure, sizeof(failure),
                                "regulation equality repeat rollback failed");
                            goto failed;
                        }

                        before_reject = lifecycle;
                        if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                                &scene->cpu_steering_assets, 2U, 1U,
                                lifecycle.period_entry_selector, false,
                                &lifecycle) ||
                            memcmp(&lifecycle, &before_reject,
                                   sizeof(lifecycle)) != 0) {
                            (void)snprintf(failure, sizeof(failure),
                                "regulation atomic BA rollback failed");
                            goto failed;
                        }
                        equality = lifecycle;
                        equality.regulation_entry_seeded_period = 0U;
                        equality.regulation_entry_seed_serial = 0U;
                        equality.period_entry_selector =
                            TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                        equality.regulation_entry_clamp_exemption_active =
                            false;
                        equality.regulation_entry_clamp_exempt_actor =
                            TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                        before_reject = equality;
                        if (!tecmo_gameplay_live_foundation_valid(
                                &scene->cpu_steering_assets, &equality) ||
                            tecmo_gameplay_live_foundation_regulation_entry_apply(
                                &scene->cpu_steering_assets, 2U, 1U, 1U, true,
                                &equality) ||
                            memcmp(&equality, &before_reject,
                                   sizeof(equality)) != 0) {
                            (void)snprintf(failure, sizeof(failure),
                                "regulation post-role seed rollback failed");
                            goto failed;
                        }

                        for (period = 2U; period <= 4U; ++period) {
                            uint8_t target_side = (uint8_t)(
                                (side ^ 1U) ^ (period == 4U ? 1U : 0U));
                            uint8_t primary_before = lifecycle.primary_actor;
                            uint8_t defender_before = lifecycle.defender_actor;
                            bool expected_swap =
                                lifecycle.offense_side != target_side;
                            uint8_t expected_primary = expected_swap
                                ? defender_before : primary_before;
                            uint8_t flags_before[
                                TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
                            uint8_t primary_wait_before;
                            memcpy(flags_before,
                                   lifecycle.actor_selector_flags,
                                   sizeof(flags_before));
                            lifecycle.play_state.wait_counter[
                                expected_primary] = (uint8_t)(10U + period);
                            primary_wait_before = lifecycle.play_state
                                .wait_counter[expected_primary];
                            for (fixture_actor = 0U;
                                 fixture_actor <
                                     TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                                 ++fixture_actor) {
                                lifecycle.play_state.action_state_046e[
                                    fixture_actor] =
                                        (uint8_t)(0x40U + fixture_actor);
                            }
                            if (!tecmo_gameplay_live_foundation_regulation_entry_apply(
                                    &scene->cpu_steering_assets, period,
                                    target_side,
                                    lifecycle.period_entry_selector,
                                    true, &lifecycle) ||
                                lifecycle.primary_actor !=
                                    (expected_swap
                                         ? defender_before : primary_before) ||
                                lifecycle.defender_actor !=
                                    (expected_swap
                                         ? primary_before : defender_before) ||
                                lifecycle.play_state.actor_state[
                                    lifecycle.primary_actor] != 0x04U ||
                                lifecycle.play_state.action[
                                    lifecycle.primary_actor] != 0x30U ||
                                lifecycle.play_state.action[
                                    lifecycle.defender_actor] != 0x30U ||
                                lifecycle.play_state.wait_counter[
                                    lifecycle.primary_actor] !=
                                    primary_wait_before ||
                                lifecycle.source_target_valid[preserved_actor] !=
                                    true ||
                                lifecycle.play_state.target_x[preserved_actor] !=
                                    0x0180 ||
                                lifecycle.play_state.target_depth[
                                    preserved_actor] != 0x0090 ||
                                !lifecycle.source_direction_valid[
                                    preserved_actor] ||
                                lifecycle.source_direction[preserved_actor] !=
                                    3U ||
                                lifecycle.play_state.direction[
                                    preserved_actor] != 3U ||
                                lifecycle.play_state.wait_counter[
                                    preserved_actor] != 7U ||
                                lifecycle.play_state.route_motion[
                                    preserved_actor]
                                        .horizontal_accumulator_q6 != 0x4567U ||
                                lifecycle.play_state.route_motion[
                                    preserved_actor]
                                        .depth_accumulator_q6 != 0x2345U ||
                                lifecycle.play_state.route_motion[
                                    preserved_actor]
                                        .horizontal_velocity_q6 != -32 ||
                                lifecycle.play_state.route_motion[
                                    preserved_actor]
                                        .depth_velocity_q6 != 16 ||
                                lifecycle.play_state.route_motion[
                                    preserved_actor]
                                        .remaining_timer != 9U ||
                                lifecycle.regulation_entry_seeded_period !=
                                    period ||
                                lifecycle.regulation_entry_seed_serial !=
                                    (uint32_t)period) {
                                (void)snprintf(failure, sizeof(failure),
                                    "regulation entry failed p%u swap%u roles%u/%u expected%u/%u wait%u/%u selector%u route%u",
                                    (unsigned)period,
                                    expected_swap ? 1U : 0U,
                                    (unsigned)lifecycle.primary_actor,
                                    (unsigned)lifecycle.defender_actor,
                                    (unsigned)(expected_swap
                                        ? defender_before : primary_before),
                                    (unsigned)(expected_swap
                                        ? primary_before : defender_before),
                                    (unsigned)lifecycle.play_state.wait_counter[
                                        lifecycle.primary_actor],
                                    (unsigned)primary_wait_before,
                                    (unsigned)lifecycle.period_entry_selector,
                                    lifecycle.play_state.route_motion[
                                        preserved_actor].active ? 1U : 0U);
                                goto failed;
                            }
                            for (fixture_actor = 0U;
                                 fixture_actor <
                                     TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                                 ++fixture_actor) {
                                if (lifecycle.play_state.action_state_046e[
                                        fixture_actor] != 0U ||
                                    lifecycle.actor_selector_flags[
                                        fixture_actor] !=
                                        (uint8_t)(flags_before[fixture_actor] ^
                                            (expected_swap ? 0x10U : 0U))) {
                                    (void)snprintf(failure, sizeof(failure),
                                        "regulation mismatch BFA8/role-bit failed p%u a%u",
                                        (unsigned)period,
                                        (unsigned)fixture_actor);
                                    goto failed;
                                }
                            }
                            before_reject = lifecycle;
                            if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                                    &scene->cpu_steering_assets, period,
                                    target_side,
                                    lifecycle.period_entry_selector,
                                    true, &lifecycle) ||
                                memcmp(&lifecycle, &before_reject,
                                       sizeof(lifecycle)) != 0) {
                                (void)snprintf(failure, sizeof(failure),
                                    "regulation atomic repeat rollback failed p%u",
                                    (unsigned)period);
                                goto failed;
                            }
                        }
                    }
                    if (orientation == 0U && slot == 0U) {
                        TecmoGameplayLiveFoundation regulation_four = fixture;
                        TecmoGameplayLiveFoundation overtime;
                        TecmoGameplayLiveFoundation rollback;
                        uint8_t current_side;
                        uint8_t overtime_epoch;
                        uint8_t prior_epoch;
                        uint8_t regulation_period;

                        for (regulation_period = 2U;
                             regulation_period <= 4U;
                             ++regulation_period) {
                            uint8_t target_side = (uint8_t)(
                                (side ^ 1U) ^
                                (regulation_period == 4U ? 1U : 0U));
                            if (!tecmo_gameplay_live_foundation_regulation_entry_apply(
                                    &scene->cpu_steering_assets,
                                    regulation_period,
                                    target_side,
                                    regulation_four.period_entry_selector,
                                    true, &regulation_four)) {
                                (void)snprintf(failure, sizeof(failure),
                                    "overtime matrix P4 setup failed s%u p%u",
                                    (unsigned)side,
                                    (unsigned)regulation_period);
                                goto failed;
                            }
                        }
                        regulation_four.source_target_valid[2U] = true;
                        regulation_four.play_state.target_x[2U] = 0x0180;
                        regulation_four.play_state.target_depth[2U] = 0x0090;
                        regulation_four.source_direction_valid[2U] = true;
                        regulation_four.source_direction[2U] = 3U;
                        regulation_four.play_state.direction[2U] = 3U;
                        regulation_four.play_state.wait_counter[2U] = 77U;
                        regulation_four.play_state.actor_state[2U] = 0x05U;
                        regulation_four.play_state.route_motion[2U]
                            .horizontal_accumulator_q6 = 0x4567U;
                        regulation_four.play_state.route_motion[2U]
                            .depth_accumulator_q6 = 0x2345U;
                        regulation_four.play_state.route_motion[2U]
                            .horizontal_velocity_q6 = -32;
                        regulation_four.play_state.route_motion[2U]
                            .depth_velocity_q6 = 16;
                        regulation_four.play_state.route_motion[2U]
                            .remaining_timer = 9U;
                        regulation_four.play_state.route_motion[2U].active =
                            true;
                        rollback = regulation_four;
                        if (tecmo_gameplay_live_foundation_overtime_entry_apply(
                                &scene->cpu_steering_assets, 2U, true,
                                &rollback) ||
                            memcmp(&rollback, &regulation_four,
                                   sizeof(rollback)) != 0) {
                            (void)snprintf(failure, sizeof(failure),
                                "overtime skipped-epoch rollback failed s%u",
                                (unsigned)side);
                            goto failed;
                        }
                        for (current_side = 0U; current_side < 2U;
                             ++current_side) {
                            for (overtime_epoch = 1U; overtime_epoch <= 4U;
                                 ++overtime_epoch) {
                                uint8_t expected_offense;
                                uint8_t expected_primary;
                                uint8_t expected_defender;
                                uint8_t expected_raw =
                                    (overtime_epoch & 1U) != 0U
                                        ? (uint8_t)(side ^ 1U)
                                        : side;
                                bool expected_swap;
                                overtime = regulation_four;
                                for (prior_epoch = 1U;
                                     prior_epoch < overtime_epoch;
                                     ++prior_epoch) {
                                    if (!tecmo_gameplay_live_foundation_overtime_entry_apply(
                                            &scene->cpu_steering_assets,
                                            prior_epoch, true, &overtime)) {
                                        (void)snprintf(
                                            failure, sizeof(failure),
                                            "overtime prior-epoch setup failed s%u c%u n%u",
                                            (unsigned)side,
                                            (unsigned)current_side,
                                            (unsigned)prior_epoch);
                                        goto failed;
                                    }
                                }
                                if (overtime.offense_side != current_side) {
                                    uint8_t swap_actor =
                                        overtime.primary_actor;
                                    overtime.offense_side = current_side;
                                    overtime.defense_side =
                                        (uint8_t)(current_side ^ 1U);
                                    overtime.primary_actor =
                                        overtime.defender_actor;
                                    overtime.play_state.primary_actor =
                                        overtime.primary_actor;
                                    overtime.defender_actor = swap_actor;
                                    overtime.play_state.defender_actor =
                                        swap_actor;
                                    overtime.last_possession = current_side;
                                    overtime.last_ball_holder =
                                        overtime.primary_actor;
                                    overtime.play_state.candidate_actor =
                                        overtime.candidate_actor_by_side[
                                            current_side];
                                    for (fixture_actor = 0U;
                                         fixture_actor <
                                             TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                                         ++fixture_actor) {
                                        overtime.actor_selector_flags[
                                            fixture_actor] ^= 0x10U;
                                    }
                                }
                                expected_swap = current_side != expected_raw;
                                expected_offense = expected_raw;
                                expected_primary = expected_swap
                                    ? overtime.defender_actor
                                    : overtime.primary_actor;
                                expected_defender = expected_swap
                                    ? overtime.primary_actor
                                    : overtime.defender_actor;
                                overtime.play_state.wait_counter[
                                    expected_primary] =
                                        (uint8_t)(30U + overtime_epoch);
                                for (fixture_actor = 0U;
                                     fixture_actor <
                                         TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                                     ++fixture_actor) {
                                    overtime.play_state.action_state_046e[
                                        fixture_actor] =
                                            (uint8_t)(0x60U + fixture_actor);
                                }
                                if (!tecmo_gameplay_live_foundation_valid(
                                        &scene->cpu_steering_assets,
                                        &overtime) ||
                                    !tecmo_gameplay_live_foundation_overtime_entry_apply(
                                        &scene->cpu_steering_assets,
                                        overtime_epoch, true, &overtime) ||
                                    overtime.offense_side != expected_offense ||
                                    overtime.primary_actor != expected_primary ||
                                    overtime.defender_actor != expected_defender ||
                                    overtime.overtime_entry_last_applied_count !=
                                        overtime_epoch ||
                                    overtime.overtime_entry_last_selector_raw !=
                                        expected_raw ||
                                    overtime.regulation_entry_seed_serial !=
                                        (uint32_t)(4U + overtime_epoch) ||
                                    overtime.play_state.wait_counter[
                                        expected_primary] !=
                                        (uint8_t)(30U + overtime_epoch) ||
                                    overtime.play_state.action[
                                        expected_primary] != 0x30U ||
                                    overtime.play_state.action[
                                        expected_defender] != 0x30U ||
                                    !overtime.source_target_valid[2U] ||
                                    overtime.play_state.target_x[2U] !=
                                        0x0180 ||
                                    overtime.play_state.target_depth[2U] !=
                                        0x0090 ||
                                    !overtime.source_direction_valid[2U] ||
                                    overtime.source_direction[2U] != 3U ||
                                    overtime.play_state.direction[2U] != 3U ||
                                    overtime.play_state.wait_counter[2U] !=
                                        77U ||
                                    overtime.play_state.route_motion[2U]
                                        .horizontal_accumulator_q6 != 0x4567U ||
                                    overtime.play_state.route_motion[2U]
                                        .depth_accumulator_q6 != 0x2345U ||
                                    overtime.play_state.route_motion[2U]
                                        .horizontal_velocity_q6 != -32 ||
                                    overtime.play_state.route_motion[2U]
                                        .depth_velocity_q6 != 16 ||
                                    overtime.play_state.route_motion[2U]
                                        .remaining_timer != 9U) {
                                    (void)snprintf(failure, sizeof(failure),
                                        "overtime branch matrix failed s%u c%u n%u",
                                        (unsigned)side,
                                        (unsigned)current_side,
                                        (unsigned)overtime_epoch);
                                    goto failed;
                                }
                                for (fixture_actor = 0U;
                                     fixture_actor <
                                         TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
                                     ++fixture_actor) {
                                    if (overtime.play_state.action_state_046e[
                                            fixture_actor] != 0U) {
                                        (void)snprintf(
                                            failure, sizeof(failure),
                                            "overtime BFA8 clear failed s%u c%u n%u a%u",
                                            (unsigned)side,
                                            (unsigned)current_side,
                                            (unsigned)overtime_epoch,
                                            (unsigned)fixture_actor);
                                        goto failed;
                                    }
                                }
                                rollback = overtime;
                                if (tecmo_gameplay_live_foundation_overtime_entry_apply(
                                        &scene->cpu_steering_assets,
                                        overtime_epoch, true, &rollback) ||
                                    memcmp(&rollback, &overtime,
                                           sizeof(rollback)) != 0) {
                                    (void)snprintf(failure, sizeof(failure),
                                        "overtime duplicate rollback failed s%u c%u n%u",
                                        (unsigned)side,
                                        (unsigned)current_side,
                                        (unsigned)overtime_epoch);
                                    goto failed;
                                }
                                if (overtime_epoch > 1U) {
                                    rollback = overtime;
                                    if (tecmo_gameplay_live_foundation_overtime_entry_apply(
                                            &scene->cpu_steering_assets,
                                            (uint8_t)(overtime_epoch - 1U),
                                            true, &rollback) ||
                                        memcmp(&rollback, &overtime,
                                               sizeof(rollback)) != 0) {
                                        (void)snprintf(
                                            failure, sizeof(failure),
                                            "overtime decreasing rollback failed s%u c%u n%u",
                                            (unsigned)side,
                                            (unsigned)current_side,
                                            (unsigned)overtime_epoch);
                                        goto failed;
                                    }
                                }
                            }
                        }
                        overtime = regulation_four;
                        overtime.overtime_entry_last_applied_count =
                            (uint8_t)(UINT8_MAX - 1U);
                        overtime.period_entry_selector = side;
                        overtime.overtime_entry_last_selector_raw = side;
                        overtime.regulation_entry_seed_serial = 258U;
                        overtime.play_state.wait_counter[2U] = 79U;
                        rollback = overtime;
                        if (!tecmo_gameplay_live_foundation_valid(
                                &scene->cpu_steering_assets, &overtime) ||
                            !tecmo_gameplay_live_foundation_overtime_entry_apply(
                                &scene->cpu_steering_assets, UINT8_MAX, true,
                                &overtime) ||
                            overtime.overtime_entry_last_applied_count !=
                                UINT8_MAX ||
                            overtime.period_entry_selector !=
                                (uint8_t)(side ^ 1U) ||
                            overtime.overtime_entry_last_selector_raw !=
                                (uint8_t)(side ^ 1U) ||
                            overtime.regulation_entry_seed_serial != 259U ||
                            overtime.play_state.wait_counter[2U] != 79U) {
                            (void)snprintf(failure, sizeof(failure),
                                "overtime UINT8_MAX acceptance failed s%u",
                                (unsigned)side);
                            goto failed;
                        }
                        rollback = overtime;
                        if (tecmo_gameplay_live_foundation_overtime_entry_apply(
                                &scene->cpu_steering_assets, 1U, true,
                                &overtime) ||
                            memcmp(&overtime, &rollback,
                                   sizeof(overtime)) != 0) {
                            (void)snprintf(failure, sizeof(failure),
                                "overtime post-UINT8_MAX rollback failed s%u",
                                (unsigned)side);
                            goto failed;
                        }
                    }
                    before_reject = fixture;
                    if (tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 1U, side,
                            (uint8_t)(side ^ 1U), true,
                            &fixture) ||
                        memcmp(&fixture, &before_reject,
                               sizeof(fixture)) != 0 ||
                        tecmo_gameplay_live_foundation_regulation_entry_apply(
                            &scene->cpu_steering_assets, 3U,
                            side, fixture.period_entry_selector, true,
                            &fixture) ||
                        memcmp(&fixture, &before_reject,
                               sizeof(fixture)) != 0) {
                        (void)snprintf(failure, sizeof(failure),
                            "regulation seed duplicate/out-of-order rollback failed");
                        goto failed;
                    }
                }
            }
        }
    }
    stream_before = scene->live_foundation.play_state.stream_offset[holder];
    last_step_before = scene->live_foundation.last_step_offset[holder];
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &scene->cpu_steering_assets, stream_before, &command) ||
        command.opcode != 5U || stream_before != 0x017CU) {
        (void)snprintf(failure, sizeof(failure),
                       "PRETIP CPU holder stream is not Bank06 seed opcode-5 "
                       "offset=%04X opcode=%u holder=%u",
                       (unsigned)stream_before, (unsigned)command.opcode,
                       (unsigned)holder);
        goto failed;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        before[actor] = scene->actors[actor].position;
    }
    memset(&away, 0, sizeof(away));
    memset(&home, 0, sizeof(home));
    if (!tecmo_gameplay_scene_update(scene, &away, &home) ||
        scene->live_foundation.deferred[holder] ||
        scene->live_foundation.deferred_reason[holder] !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
        scene->live_foundation.play_state.stream_offset[holder] !=
            (uint16_t)(stream_before +
                       TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
        scene->live_foundation.last_step_offset[holder] !=
            (uint16_t)(last_step_before +
                       TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        (void)snprintf(failure, sizeof(failure),
                       "PRETIP CPU selected-primary single step failed "
                       "before=%04X after=%04X deferred=%u reason=%s status=%s",
                       (unsigned)stream_before,
                       (unsigned)scene->live_foundation.play_state
                           .stream_offset[holder],
                       scene->live_foundation.deferred[holder] ? 1U : 0U,
                       tecmo_gameplay_cpu_steering_deferred_reason_name(
                           scene->live_foundation.deferred_reason[holder]),
                       scene->status);
        goto failed;
    }
    for (update = 0U; update < 12U && !actor_moved; ++update) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            if (actor != holder &&
                (scene->actors[actor].position.x != before[actor].x ||
                 scene->actors[actor].position.y != before[actor].y)) {
                actor_moved = true;
                break;
            }
        }
        if (actor_moved) break;
        memset(&away, 0, sizeof(away));
        memset(&home, 0, sizeof(home));
        if (!tecmo_gameplay_scene_update(scene, &away, &home)) {
            (void)snprintf(failure, sizeof(failure),
                           "PRETIP CPU movement update %u rejected: %s",
                           (unsigned)update, scene->status);
            goto failed;
        }
    }
    if (!actor_moved) {
        (void)snprintf(failure, sizeof(failure),
                       "PRETIP CPU non-selected actors remained frozen "
                       "holder=%u offset=%04X",
                       (unsigned)holder,
                       (unsigned)scene->live_foundation.play_state
                           .stream_offset[holder]);
        goto failed;
    }
    for (update = 0U; update < 64U &&
         scene->pass_state.phase == TECMO_GAMEPLAY_SCENE_PASS_NONE;
         ++update) {
        memset(&away, 0, sizeof(away));
        memset(&home, 0, sizeof(home));
        if (!tecmo_gameplay_scene_update(scene, &away, &home)) {
            (void)snprintf(failure, sizeof(failure),
                "PRETIP CPU organic pass update %u rejected: %s",
                (unsigned)update, scene->status);
            goto failed;
        }
    }
    if (scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_GATHER ||
        scene->pass_state.passer != holder ||
        scene->pass_state.receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.play_state.stream_offset[holder] != 0x0190U) {
        (void)snprintf(failure, sizeof(failure),
            "PRETIP CPU opcode5-23-6 pass chain failed phase=%u cursor=%04X",
            (unsigned)scene->pass_state.phase,
            (unsigned)scene->live_foundation.play_state
                .stream_offset[holder]);
        goto failed;
    }
    {
        uint8_t receiver = scene->pass_state.receiver;
        for (update = 0U; update < 240U &&
             scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_NONE;
             ++update) {
            memset(&away, 0, sizeof(away));
            memset(&home, 0, sizeof(home));
            if (!tecmo_gameplay_scene_update(scene, &away, &home)) {
                (void)snprintf(failure, sizeof(failure),
                    "PRETIP CPU pass catch update %u rejected: %s",
                    (unsigned)update, scene->status);
                goto failed;
            }
        }
        if (scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_NONE ||
            scene->ball_holder != receiver || receiver == holder ||
            scene->live_foundation.regulation_entry_clamp_exemption_active ||
            scene->live_foundation.regulation_entry_clamp_exempt_actor !=
                TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->live_foundation.primary_actor != receiver ||
            !scene_actor_world_position_valid(&scene->actors[holder]) ||
            scene->actors[holder].anchor.x != scene->actors[holder].position.x ||
            scene->actors[holder].anchor.y != scene->actors[holder].position.y ||
            !scene_actor_position_valid_for_scene(scene, holder)) {
            (void)snprintf(failure, sizeof(failure),
                "PRETIP CPU catch did not retire seed exemption");
            goto failed;
        }
    }
    tecmo_gameplay_scene_end(scene);
    return true;

failed:
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    tecmo_gameplay_scene_test_message(message, message_size, failure);
    tecmo_gameplay_scene_end(scene);
    return false;
}

static bool scene_test_score_restart_auto_pass_selector(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveFoundation *restart_foundation)
{
    static const uint8_t directions[10] = {
        1U, 1U, 2U, 5U, 1U, 0U, 0U, 2U, 5U, 0U
    };
    TecmoGameplayCourtCoordinate positions[10];
    uint8_t orientation;
    uint8_t mismatch;
    size_t actor;
    if (scene == NULL || restart_foundation == NULL) return false;
    for (orientation = 0U; orientation < 2U; ++orientation) {
        for (mismatch = 0U; mismatch < 2U; ++mismatch) {
            TecmoGameplayLiveFoundation auto_pass = *restart_foundation;
            TecmoGameplayLiveFoundation auto_before;
            TecmoGameplayLiveAutoPassSelection selection;
            uint8_t expected_winner = mismatch != 0U ? 9U : 5U;
            uint8_t expected_final = mismatch != 0U ? 5U : 6U;
            auto_pass.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
            auto_pass.orientation = orientation;
            for (actor = 0U; actor < 10U; ++actor) {
                positions[actor] = auto_pass.actor_position[actor];
                if (auto_pass.actor_team[actor] == TECMO_GAMEPLAY_TEAM_HOME) {
                    uint16_t distance = (uint16_t)(340U + actor);
                    positions[actor].x = orientation == 0U
                        ? (int16_t)(0x0400U - distance) : (int16_t)distance;
                    positions[actor].y = (actor & 1U) != 0U ? 0x0095 : 0x0096;
                }
            }
            positions[expected_winner].x = orientation == 0U ? 724 : 300;
            if (mismatch != 0U) positions[8U].x = positions[9U].x;
            auto_pass.candidate_actor_by_side[TECMO_GAMEPLAY_TEAM_HOME] =
                expected_winner;
            auto_pass.play_state.wait_counter[expected_winner] =
                mismatch != 0U ? 0x2BU : 0x2AU;
            memset(&selection, 0xA5, sizeof(selection));
            if (!tecmo_gameplay_live_foundation_score_restart_auto_pass_select(
                    &scene->cpu_steering_assets, positions, directions,
                    &auto_pass, &selection) ||
                selection.contract_tag !=
                    TECMO_GAMEPLAY_LIVE_AUTO_PASS_SELECTION_TAG ||
                selection.old_primary != 5U ||
                selection.new_primary != expected_winner ||
                selection.winning_distance != 300U ||
                selection.primary_changed != (mismatch != 0U) ||
                !selection.candidate_collision_advanced ||
                selection.candidate_before_refresh !=
                    (mismatch != 0U ? 5U : 6U) ||
                selection.candidate_after_refresh != expected_final ||
                auto_pass.primary_actor != expected_winner ||
                auto_pass.last_ball_holder != 5U ||
                !auto_pass.score_restart_selection_active ||
                auto_pass.score_restart_passer != 5U ||
                auto_pass.play_state.stream_offset[expected_winner] != 0x0168U ||
                auto_pass.play_state.actor_state[expected_winner] != 4U ||
                auto_pass.play_state.wait_counter[expected_winner] !=
                    (mismatch != 0U ? 0x2BU : 0x2AU) ||
                auto_pass.play_state.action_state_046e[expected_winner] != 0U ||
                auto_pass.candidate_actor_by_side[TECMO_GAMEPLAY_TEAM_HOME] !=
                    expected_final ||
                auto_pass.play_state.candidate_actor != expected_final ||
                (mismatch != 0U &&
                 (auto_pass.play_state.pose[5U] != 0x0CU ||
                  selection.old_primary_pose_high_044d != 0x04U ||
                  selection.old_primary_sprite_flags_0479 != 0xC1U ||
                  auto_pass.play_state.action[5U] != 0x30U)) ||
                !tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &auto_pass)) return false;
            auto_before = auto_pass;
            memset(&selection, 0xA5, sizeof(selection));
            if (tecmo_gameplay_live_foundation_score_restart_auto_pass_select(
                    &scene->cpu_steering_assets, positions, directions,
                    &auto_pass, &selection) ||
                memcmp(&auto_pass, &auto_before, sizeof(auto_pass)) != 0 ||
                selection.contract_tag != 0xA5A5A5A5U) return false;
            memset(&selection, 0xA5, sizeof(selection));
            if (tecmo_gameplay_live_foundation_score_restart_gather(
                    &scene->cpu_steering_assets,
                    TECMO_GAMEPLAY_LIVE_SCORE_RESTART_GATHER_AUTOMATIC_SELECTED,
                    7U, expected_final, &auto_pass) ||
                memcmp(&auto_pass, &auto_before, sizeof(auto_pass)) != 0) {
                return false;
            }
        }
    }
    return true;
}

static bool scene_test_opcode15_live_owner(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayLiveFoundation *foundation_before,
    TecmoGameplayLiveFoundation *candidate)
{
    TecmoGameplayCpuSteeringOpcode15RawResult opcode15_result;
    TecmoGameplayCpuOpcode15State7Result state7_result;
    uint8_t direction_0463[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {0};
    uint8_t opcode15_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    uint8_t defense_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    size_t actor_index;
    if (assets == NULL || foundation_before == NULL || candidate == NULL) {
        return false;
    }
    *candidate = *foundation_before;
    for (actor_index = 0U;
         actor_index < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor_index) {
        if (candidate->actor_team[actor_index] == candidate->offense_side &&
            actor_index != candidate->primary_actor) {
            opcode15_actor = (uint8_t)actor_index;
            break;
        }
    }
    if (opcode15_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) return false;
    candidate->control_mode[candidate->offense_side] = 1U;
    candidate->play_state.stream_offset[opcode15_actor] = 0x0037U;
    candidate->last_step_offset[opcode15_actor] = 0x0037U;
    candidate->play_state.actor_state[opcode15_actor] = 4U;
    candidate->play_state.action_state_046e[opcode15_actor] = 0U;
    memset(&opcode15_result, 0, sizeof(opcode15_result));
    if (!tecmo_gameplay_live_foundation_opcode15_step_automatic(
            assets, opcode15_actor, 0x46U, direction_0463, candidate,
            &opcode15_result)) {
        return false;
    }
    if (
        opcode15_result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_REPLACED ||
        !opcode15_result.committed || candidate->primary_actor != opcode15_actor ||
        !candidate->opcode15_selection_latch.valid ||
        candidate->opcode15_selection_latch.actor_059e != opcode15_actor ||
        !tecmo_gameplay_live_foundation_valid(assets, candidate)) {
        return false;
    }
    for (actor_index = 0U;
         actor_index < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor_index) {
        if (candidate->actor_team[actor_index] == candidate->defense_side &&
            actor_index != candidate->defender_actor) {
            defense_actor = (uint8_t)actor_index;
            break;
        }
    }
    if (defense_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    candidate->control_mode[candidate->defense_side] = 1U;
    candidate->play_state.stream_offset[defense_actor] = 0x004BU;
    candidate->last_step_offset[defense_actor] = 0x004BU;
    candidate->play_state.actor_state[defense_actor] = 4U;
    candidate->play_state.action_state_046e[defense_actor] = 0U;
    memset(&opcode15_result, 0, sizeof(opcode15_result));
    if (!tecmo_gameplay_live_foundation_opcode15_step_automatic(
            assets, defense_actor, 0x46U, direction_0463, candidate,
            &opcode15_result) ||
        opcode15_result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED ||
        !opcode15_result.committed || candidate->defender_actor != defense_actor ||
        candidate->play_state.actor_state[defense_actor] != 7U ||
        candidate->opcode15_selection_latch.actor_059e != defense_actor ||
        !tecmo_gameplay_live_foundation_valid(assets, candidate)) {
        return false;
    }
    memset(&state7_result, 0, sizeof(state7_result));
    return tecmo_gameplay_live_foundation_opcode15_state7_step(
               defense_actor, candidate, &state7_result) &&
           state7_result.retired_state7 &&
           state7_result.actor_059e == defense_actor &&
           candidate->play_state.actor_state[defense_actor] == 0U &&
           candidate->opcode15_selection_latch.stale &&
           tecmo_gameplay_live_foundation_valid(assets, candidate);
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
    static const uint8_t exact_b87c_candidate_remap[10] = {
        1U, 2U, 3U, 4U, 0U, 6U, 7U, 8U, 9U, 5U
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
    TecmoGameplayLiveClaimantSettlement claimant_settlement;
    TecmoGameplayScenePossessionTraceSnapshot possession_trace;
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCpuSteeringPlayResult play_result;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCpuSteeringShotInput shot_input;
    TecmoGameplayCpuSteeringShotResult shot_result;
    TecmoGameplayCpuSteeringMovementInput movement_input;
    TecmoGameplayCpuSteeringMovementResult movement_result;
    TecmoGameplayCourtCoordinate positions[10];
    TecmoGameplayCourtCoordinate refresh_positions[10];
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
    int16_t preserved_target_x = 0;
    int16_t preserved_target_depth = 0;
    uint32_t sync_serial_before;
    uint8_t selector_flags_before[10];
    bool found_deferred = false;
    bool found_advance = false;
    bool found_absolute_target = false;
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

    /* Source-proven selected-primary action-$21 pass slice. Exact Bank04
       $A05F/stream-$0131 executes once through $8374->$8491->$8B90 before
       $8284's ordinary loop skips the primary, then Bank05 $89D7 gathers. */
    {
        const uint8_t passer = 2U;
        const uint8_t captured_receiver = 4U;
        const uint8_t ordinary_wait_actor = 4U;
        const uint16_t action21_offset = 0x0131U;
        TecmoGameplayCpuSteeringCommand action21;
        TecmoGameplayLiveFoundation pass_foundation;
        TecmoGameplayScene before;
        TecmoGameplaySceneCpuShotRequest no_shot;
        uint16_t x_accumulator;
        uint16_t y_accumulator;
        uint16_t x_position;
        uint8_t y_position;
        uint8_t receiver;
        uint8_t opposing_controlled_actor;
        uint16_t primary_stream_before;
        uint16_t ordinary_stream_before;
        uint16_t primary_last_step_before;
        int16_t expected_catch_target_x;
        size_t pass_updates;
        TecmoGameplayScene postcatch_scene;

        /* Bank05 $BD6E-$BDC6 exact uint16 accumulation/Q10.6 extraction.
           Include captured-shape values, carry, wrap, and a high-byte $FF
           delta so the C helper cannot regress to signed right shifts. */
        x_accumulator = 0x2422U;
        y_accumulator = 0x1040U;
        if (!scene_pass_bank05_bd6e_step(
                &x_accumulator, 0x0062U, &y_accumulator, 0x0020U,
                &x_position, &y_position) ||
            x_accumulator != 0x2484U || y_accumulator != 0x1060U ||
            x_position != 0x0092U || y_position != 0x41U) {
            LIVE_FAIL("LIVE pass BD6E captured-shape vector failed");
        }
        x_accumulator = 0x12FFU;
        y_accumulator = 0xFF00U;
        if (!scene_pass_bank05_bd6e_step(
                &x_accumulator, 0x0001U, &y_accumulator, 0x0200U,
                &x_position, &y_position) ||
            x_accumulator != 0x1300U || y_accumulator != 0x0100U ||
            x_position != 0x004CU || y_position != 0x04U) {
            LIVE_FAIL("LIVE pass BD6E carry/wrap vector failed");
        }
        x_accumulator = 0x3000U;
        y_accumulator = 0x0100U;
        if (!scene_pass_bank05_bd6e_step(
                &x_accumulator, 0xFF80U, &y_accumulator, 0xFFC0U,
                &x_position, &y_position) ||
            x_accumulator != 0x2F80U || y_accumulator != 0x00C0U ||
            x_position != 0x00BEU || y_position != 0x03U) {
            LIVE_FAIL("LIVE pass BD6E high-delta vector failed");
        }

        cpu_only = bound;
        cpu_only.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        /* Away is CPU-controlled; retain a real human controller on Home so
           controller=NONE catch proves it leaves unrelated human control
           byte-for-byte unchanged. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene) ||
            !tecmo_gameplay_cpu_steering_decode_command(
                &scene->cpu_steering_assets, action21_offset, &action21) ||
            action21.opcode != 9U || action21.arguments[0U] != 0U ||
            action21.arguments[1U] != 0x21U) {
            LIVE_FAIL("LIVE CPU action-21 pass fixture setup rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        opposing_controlled_actor = scene->controlled_actor[1U];
        if (opposing_controlled_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[opposing_controlled_actor].team !=
                TECMO_GAMEPLAY_TEAM_HOME) {
            LIVE_FAIL("LIVE CPU pass opposing human control fixture invalid");
        }
        /* FCEUX closes the downstream identity for this observed route:
           offense side 0 entered $89D7 with primary actor 2 and raw
           $037F[0]=4; genuine Bank05 $B24F later stored actor 4 to $0308. */
        receiver = captured_receiver;
        scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.offense_side] = receiver;
        if (scene->actors[receiver].team != TECMO_GAMEPLAY_TEAM_AWAY ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE CPU action-21 receiver fixture invalid");
        }

        /* Any action other than exact $21 and a self-candidate both reject
           transactionally; neither can smuggle a human-button pass into CPU
           offense. */
        scene->live_foundation.play_state.action_state_046e[passer] = 0x20U;
        before = *scene;
        if (scene_begin_cpu_pass_from_action21(scene, passer) ||
            memcmp(scene, &before, sizeof(before)) != 0) {
            LIVE_FAIL("LIVE non-21 CPU action started or mutated a pass");
        }
        scene->live_foundation.play_state.action_state_046e[passer] = 0x21U;
        scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.offense_side] = passer;
        before = *scene;
        if (scene_begin_cpu_pass_from_action21(scene, passer) ||
            memcmp(scene, &before, sizeof(before)) != 0) {
            LIVE_FAIL("LIVE invalid CPU pass candidate was not transactional");
        }
        scene->live_foundation = before.live_foundation;
        scene->live_foundation.play_state.action_state_046e[passer] = 0U;
        scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.offense_side] = receiver;

        /* Typed automatic ownership models the supported raw $030C!=0,
           ordinary $05A1=0 selected-primary gate. The exact opcode-9 record
           must advance once, write C9=$21, and enter $89D7 in this update;
           $8286/$8289 must prevent an ordinary-loop duplicate step. */
        pass_foundation = scene->live_foundation;
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            pass_foundation.play_state.wait_counter[actor] = 1U;
            pass_foundation.deferred[actor] = false;
            pass_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        pass_foundation.play_state.wait_counter[passer] = 0U;
        pass_foundation.play_state.actor_state[passer] = 0x04U;
        pass_foundation.play_state.stream_offset[passer] = action21_offset;
        pass_foundation.last_step_offset[passer] = action21_offset;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &pass_foundation)) {
            LIVE_FAIL("LIVE CPU action-21 source foundation rejected");
        }
        scene->live_foundation = pass_foundation;
        primary_stream_before = action21_offset;
        primary_last_step_before =
            scene->live_foundation.last_step_offset[passer];
        expected_catch_target_x =
            scene->orientation_state.attack_direction == 0U
                ? 0x00B4 : 0x024C;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            no_shot.playback_supported || no_shot.deferred ||
            !scene_pass_active(scene) ||
            scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_GATHER ||
            scene->pass_state.passer != passer ||
            scene->pass_state.receiver != receiver ||
            scene->pass_state.controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->pass_state.packed_animation_state != 0x32U ||
            scene->pass_state.receiver_locked ||
            scene->ball_holder != passer ||
            scene->live_foundation.play_state
                    .action_state_046e[passer] != 0x0FU ||
            /* Opcode 9 C8 writes actor state 0; $8284-$82A5 excludes the
               primary from ordinary $057C dispatch before $89D7 consumes
               action $21. The typed gather must preserve that separation. */
            scene->live_foundation.play_state.actor_state[passer] != 0U ||
            scene->live_foundation.play_state.actor_state[receiver] != 0x0CU ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                (uint16_t)(primary_stream_before +
                           TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
            scene->live_foundation.last_step_offset[passer] !=
                (uint16_t)(primary_last_step_before +
                           TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
            scene->controlled_actor[0U] != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->controlled_actor[1U] != opposing_controlled_actor) {
            LIVE_FAIL("LIVE selected-primary action-21 did not gather once");
        }
        for (pass_updates = 0U; pass_updates < 4U; ++pass_updates) {
            static const uint8_t expected_gather[4U] = {
                0x22U, 0x12U, 0x02U, 0x03U
            };
            if (!scene_update_pass(scene) ||
                scene->pass_state.phase !=
                    TECMO_GAMEPLAY_SCENE_PASS_GATHER ||
                scene->pass_state.receiver_locked ||
                scene->ball_holder != passer ||
                scene->pass_state.packed_animation_state !=
                    expected_gather[pass_updates]) {
                LIVE_FAIL("LIVE CPU pass gather order failed");
            }
        }
        if (!scene_update_pass(scene) ||
            scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_FLIGHT ||
            scene->pass_state.packed_animation_state != 0x04U ||
            !scene->pass_state.receiver_locked ||
            scene->ball_holder != passer ||
            scene->live_foundation.primary_actor != passer ||
            scene->live_foundation.selected_actor_by_side[
                scene->live_foundation.offense_side] != receiver ||
            scene->live_foundation.candidate_actor_by_side[
                scene->live_foundation.offense_side] != passer) {
            LIVE_FAIL("LIVE CPU pass launch identity lock failed");
        }
        for (pass_updates = 0U;
             scene_pass_active(scene) && pass_updates < 32U;
             ++pass_updates) {
            if (!scene_update_pass(scene)) {
                LIVE_FAIL("LIVE CPU pass flight/catch rejected");
            }
        }
        if (scene_pass_active(scene) || scene->ball_holder != receiver ||
            scene->controlled_actor[0U] != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->controlled_actor[1U] != opposing_controlled_actor ||
            scene->live_foundation.primary_actor != receiver ||
            scene->live_foundation.play_state
                    .action_state_046e[passer] != 0U ||
            /* $B24F clears the receiver to state/action 0, then its same-call
               $B2EC->$96B6 automatic tail writes action $18 and returns the
               selected holder in state 4. LIVE lacks the raw route-choice
               workspaces and projects source-valid long route $00D7 so the first
               command owns an exact target instead of fabricated gates. */
            scene->live_foundation.play_state.actor_state[receiver] != 4U ||
            scene->live_foundation.play_state
                    .action_state_046e[receiver] != 0x18U ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00D7U ||
            scene->live_foundation.last_step_offset[receiver] != 0x00D7U) {
            LIVE_FAIL("LIVE CPU pass catch/control handoff failed");
        }
        /* The genuine B24F catch above installs the former selected passer at
           `$0B63`; do not park that cursor. Exercise its next two eligible
           ordinary dispatches (opcode 2 -> opcode 8) against all four exact
           held-ball/orientation edges. Non-primary actors remain otherwise
           exactly as produced by the catch transaction. */
        postcatch_scene = *scene;
        {
            TecmoGameplayLiveFoundation exact_refresh =
                postcatch_scene.live_foundation;
            TecmoGameplayLiveFoundation nearby_refresh = exact_refresh;
            TecmoGameplayLiveFoundation opcode8_refresh = exact_refresh;
            TecmoGameplayLiveFoundation wrong_state_refresh = exact_refresh;
            if (!tecmo_gameplay_live_foundation_refresh_formation(
                    &scene->cpu_steering_assets,
                    postcatch_scene.live_foundation.actor_position,
                    &exact_refresh) ||
                exact_refresh.play_state.stream_offset[passer] != 0x0B63U) {
                LIVE_FAIL("LIVE exact post-catch cursor did not survive refresh");
            }
            nearby_refresh.play_state.stream_offset[passer] = 0x0B6DU;
            nearby_refresh.last_step_offset[passer] = 0x0B6DU;
            if (!tecmo_gameplay_live_foundation_refresh_formation(
                    &scene->cpu_steering_assets,
                    postcatch_scene.live_foundation.actor_position,
                    &nearby_refresh) ||
                nearby_refresh.play_state.stream_offset[passer] == 0x0B6DU) {
                LIVE_FAIL("LIVE nearby non-catch cursor bypassed refresh");
            }
            opcode8_refresh.play_state.stream_offset[passer] = 0x0B68U;
            opcode8_refresh.last_step_offset[passer] = 0x0B68U;
            if (!tecmo_gameplay_live_foundation_refresh_formation(
                    &scene->cpu_steering_assets,
                    postcatch_scene.live_foundation.actor_position,
                    &opcode8_refresh) ||
                opcode8_refresh.play_state.stream_offset[passer] == 0x0B68U) {
                LIVE_FAIL("LIVE opcode-8 cursor bypassed ordinary refresh");
            }
            wrong_state_refresh.play_state.actor_state[passer] = 0x06U;
            wrong_state_refresh.play_state.wait_counter[passer] = 1U;
            if (!tecmo_gameplay_live_foundation_refresh_formation(
                    &scene->cpu_steering_assets,
                    postcatch_scene.live_foundation.actor_position,
                    &wrong_state_refresh) ||
                wrong_state_refresh.play_state.stream_offset[passer] ==
                    0x0B63U ||
                wrong_state_refresh.play_state.actor_state[passer] != 0x04U) {
                LIVE_FAIL("LIVE wrong-state catch cursor bypassed refresh");
            }
        }
        {
            TecmoGameplayScene natural_scene = postcatch_scene;
            uint16_t natural_ball_x;
            bool natural_redirect;
            memset(&no_shot, 0, sizeof(no_shot));
            if (!scene_update_ai(&natural_scene, &no_shot) ||
                natural_scene.live_foundation.play_state
                        .stream_offset[passer] != 0x0B68U ||
                natural_scene.live_foundation.last_step_offset[passer] !=
                    0x0B68U ||
                natural_scene.live_foundation.deferred[passer] ||
                !scene_attach_ball(&natural_scene)) {
                LIVE_FAIL("LIVE organic post-catch opcode-2 predecessor failed");
            }
            natural_ball_x =
                (uint16_t)(natural_scene.ball_position.x_q8 / 256);
            natural_redirect = natural_scene.live_foundation.orientation == 0U
                ? natural_ball_x < 0x0140U
                : natural_ball_x >= 0x01C0U;
            memset(&no_shot, 0, sizeof(no_shot));
            if (!scene_update_ai(&natural_scene, &no_shot) ||
                natural_scene.live_foundation.play_state
                        .stream_offset[passer] !=
                    (natural_redirect ? 0x025DU : 0x0B6DU) ||
                natural_scene.live_foundation.play_state
                        .actor_state[passer] != 0x04U ||
                natural_scene.live_foundation.deferred[passer]) {
                char failure[320];
                (void)snprintf(failure, sizeof(failure),
                    "LIVE natural post-catch opcode-8 orient=%u ball=%u redirect=%u stream=%04X last=%04X state=%u defer=%u reason=%u failed",
                    (unsigned)natural_scene.live_foundation.orientation,
                    (unsigned)natural_ball_x,
                    natural_redirect ? 1U : 0U,
                    (unsigned)natural_scene.live_foundation.play_state
                        .stream_offset[passer],
                    (unsigned)natural_scene.live_foundation
                        .last_step_offset[passer],
                    (unsigned)natural_scene.live_foundation.play_state
                        .actor_state[passer],
                    natural_scene.live_foundation.deferred[passer] ? 1U : 0U,
                    (unsigned)natural_scene.live_foundation
                        .deferred_reason[passer]);
                LIVE_FAIL(failure);
            }
        }
        /* The chosen source-valid long route $00D7 begins with exact opcode 2 and absolute target
           ($00B4,$0096) for orientation 0. The catch tail makes the holder
           immediately eligible; prove that selected-primary dispatch owns
           that target and TGMO begins moving toward it rather than leaving
           the receiver inert. */
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || scene_pass_active(scene) ||
            scene->live_foundation.play_state.actor_state[receiver] != 4U ||
            scene->live_foundation.play_state.wait_counter[receiver] != 0U ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00DCU ||
            scene->live_foundation.last_step_offset[receiver] != 0x00DCU ||
            !scene->live_foundation.source_target_valid[receiver] ||
            scene->live_foundation.play_state.target_x[receiver] !=
                expected_catch_target_x ||
            scene->live_foundation.play_state.target_depth[receiver] != 0x0096 ||
            !scene->cpu_actors[receiver].target_valid ||
            scene->cpu_actors[receiver].target_position.x !=
                expected_catch_target_x ||
            scene->cpu_actors[receiver].target_position.y != 0x0096) {
            char failure[512];
            (void)snprintf(
                failure, sizeof(failure),
                "LIVE CPU catch route target failed: holder=%u receiver=%u primary=%u poss=%u controllers=(%u,%u) legacy=%u orient=%u state=%u wait=%u stream=%04X last=%04X source=%u target=(%d,%d) cpu=%u cpu_target=(%d,%d) expected_x=%d defer=%u reason=%u",
                (unsigned)scene->ball_holder, (unsigned)receiver,
                (unsigned)scene->live_foundation.primary_actor,
                (unsigned)scene->state.possession,
                (unsigned)scene->launch.controller_team[0U],
                (unsigned)scene->launch.controller_team[1U],
                scene->legacy_direct_launch ? 1U : 0U,
                (unsigned)scene->orientation_state.attack_direction,
                (unsigned)scene->live_foundation.play_state.actor_state[receiver],
                (unsigned)scene->live_foundation.play_state.wait_counter[receiver],
                (unsigned)scene->live_foundation.play_state.stream_offset[receiver],
                (unsigned)scene->live_foundation.last_step_offset[receiver],
                scene->live_foundation.source_target_valid[receiver] ? 1U : 0U,
                (int)scene->live_foundation.play_state.target_x[receiver],
                (int)scene->live_foundation.play_state.target_depth[receiver],
                scene->cpu_actors[receiver].target_valid ? 1U : 0U,
                (int)scene->cpu_actors[receiver].target_position.x,
                (int)scene->cpu_actors[receiver].target_position.y,
                (int)expected_catch_target_x,
                scene->live_foundation.deferred[receiver] ? 1U : 0U,
                (unsigned)scene->live_foundation.deferred_reason[receiver]);
            LIVE_FAIL(failure);
        }
        primary_stream_before = (uint16_t)scene->actors[receiver].position.x;
        /* $00DC opcode 21 owns exact typed shot/game clocks plus the fixed
           $F07E-$F0B9 bit-1 predicate. Select its +5 branch here so the
           natural long route reaches $00E1 action $17. */
        scene->state.shot_clock = 3U;
        scene->state.clock_minutes = 1U;
        scene->state.clock_seconds = 30U;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00E1U ||
            scene->live_foundation.last_step_offset[receiver] != 0x00E1U ||
            scene->live_foundation.deferred[receiver] ||
            !scene->live_foundation.source_target_valid[receiver] ||
            !scene->cpu_actors[receiver].target_valid) {
            LIVE_FAIL("LIVE CPU catch opcode-21 +5 branch failed");
        }
        if (scene->actors[receiver].position.x ==
                (int16_t)primary_stream_before ||
            !scene->live_foundation.source_target_valid[receiver]) {
            LIVE_FAIL("LIVE CPU catch holder remained inert");
        }
        /* $00E1 opcode 9 writes state 0/action $17. Bank05 dispatches it to
           $8ACE without a human pad; the typed autonomous seam must launch
           real far/jump playback and end held-ball ownership. */
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || !no_shot.requested ||
            no_shot.deferred || !no_shot.playback_supported ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
            scene->shot_actor != receiver ||
            scene->shot_controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            !scene->jump_playback_active ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00E6U) {
            LIVE_FAIL("LIVE CPU catch natural action-17 shot launch failed");
        }
        before = *scene;
        if (scene_begin_cpu_pass_from_action21(scene, passer) ||
            memcmp(scene, &before, sizeof(before)) != 0) {
            LIVE_FAIL("LIVE consumed CPU pass restarted without action 21");
        }

        /* Existing human A transport shares the actor-neutral kernel but
           retains its controller handoff at the same Bank05 $B24F seam. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &bound) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE human pass regression setup rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        /* Typed human ownership takes neither automatic branch at
           $8374-$83F3. The selected human primary must not consume the exact
           opcode-9 cursor through either selected or ordinary dispatch. */
        scene->live_foundation.play_state.actor_state[passer] = 0x04U;
        scene->live_foundation.play_state.wait_counter[passer] = 0U;
        scene->live_foundation.play_state.stream_offset[passer] =
            action21_offset;
        scene->live_foundation.last_step_offset[passer] = action21_offset;
        scene->live_foundation.play_state.action_state_046e[passer] = 0U;
        before = *scene;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || scene_pass_active(scene) ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                action21_offset ||
            scene->live_foundation.last_step_offset[passer] !=
                action21_offset ||
            scene->live_foundation.play_state
                    .action_state_046e[passer] != 0U) {
            LIVE_FAIL("LIVE human selected primary consumed CPU opcode-9");
        }
        receiver = scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.offense_side];
        if (!scene_begin_pass(scene, 0U, receiver)) {
            LIVE_FAIL("LIVE human pass regression gather rejected");
        }
        for (pass_updates = 0U;
             scene_pass_active(scene) && pass_updates < 32U;
             ++pass_updates) {
            if (!scene_update_pass(scene)) {
                LIVE_FAIL("LIVE human pass regression update rejected");
            }
        }
        if (scene_pass_active(scene) || scene->ball_holder != receiver ||
            scene->controlled_actor[0U] != receiver ||
            scene->live_foundation.play_state.actor_state[receiver] != 0U ||
            scene->live_foundation.play_state
                    .action_state_046e[receiver] != 0U) {
            LIVE_FAIL("LIVE human pass regression catch/control failed");
        }

        /* The nonlegacy Bank07 restart transport reaches the same typed
           $B24F catch transaction as an ordinary pass. Prove an automatic
           inbound receiver exits through $96B6 in state 4/$00D7 instead of
           remaining the selected, ordinary-loop-skipped state-0 holder. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene) ||
            !scene_begin_inbound(scene, TECMO_GAMEPLAY_TEAM_AWAY)) {
            LIVE_FAIL("LIVE automatic inbound fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        receiver = scene->inbound_state.receiver;
        for (pass_updates = 0U; scene_inbound_active(scene) &&
             pass_updates < 80U; ++pass_updates) {
            if (!scene_update_inbound(scene)) {
                LIVE_FAIL("LIVE automatic inbound transport rejected");
            }
        }
        if (scene_inbound_active(scene) || scene->ball_holder != receiver ||
            scene->live_foundation.primary_actor != receiver ||
            scene->live_foundation.play_state.actor_state[receiver] != 4U ||
            scene->live_foundation.play_state
                    .action_state_046e[receiver] != 0x18U ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00D7U ||
            scene->live_foundation.last_step_offset[receiver] != 0x00D7U) {
            LIVE_FAIL("LIVE automatic inbound catch lifecycle failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.stream_offset[receiver] !=
                0x00DCU ||
            !scene->live_foundation.source_target_valid[receiver] ||
            !scene->cpu_actors[receiver].target_valid) {
            LIVE_FAIL("LIVE automatic inbound holder remained inert");
        }

        /* Bank06 state index 6 dispatches through $82B6/$82C4 to
           $9053-$905D for selected and ordinary actors alike. Exercise it
           independently of the chosen $00D7 catch projection: decrement once
           per update, do not fetch on the zero transition, and fetch exactly
           once on the following update. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE selected-primary state-6 fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->live_foundation.play_state.actor_state[actor] = 6U;
            scene->live_foundation.play_state.wait_counter[actor] = 10U;
        }
        scene->live_foundation.play_state.wait_counter[passer] = 2U;
        /* $007D opcode 3 has already advanced to $0082 when it installs the
           exact eight-count state-6 wait. Seed that coherent continuation. */
        primary_stream_before = 0x0082U;
        scene->live_foundation.play_state.stream_offset[passer] =
            primary_stream_before;
        scene->live_foundation.last_step_offset[passer] =
            primary_stream_before;
        ordinary_stream_before = scene->live_foundation.play_state
            .stream_offset[ordinary_wait_actor];
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE selected-primary state-6 foundation invalid");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.actor_state[passer] != 6U ||
            scene->live_foundation.play_state.wait_counter[passer] != 1U ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                primary_stream_before ||
            scene->live_foundation.last_step_offset[passer] !=
                primary_stream_before ||
            scene->live_foundation.play_state
                    .actor_state[ordinary_wait_actor] != 6U ||
            scene->live_foundation.play_state
                    .wait_counter[ordinary_wait_actor] != 9U ||
            scene->live_foundation.play_state
                    .stream_offset[ordinary_wait_actor] !=
                ordinary_stream_before ||
            scene->live_foundation.last_step_offset[ordinary_wait_actor] !=
                ordinary_stream_before) {
            LIVE_FAIL("LIVE selected-primary state-6 first decrement failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.actor_state[passer] != 4U ||
            scene->live_foundation.play_state.wait_counter[passer] != 0U ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                primary_stream_before ||
            scene->live_foundation.last_step_offset[passer] !=
                primary_stream_before) {
            LIVE_FAIL("LIVE selected-primary state-6 zero transition fetched");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                0x0087U ||
            scene->live_foundation.last_step_offset[passer] !=
                scene->live_foundation.play_state.stream_offset[passer]) {
            LIVE_FAIL("LIVE selected-primary post-wait fetch failed");
        }

        /* Canonical DEC wraps a zero $04E7 entry to $FF. Selected-primary
           admission must therefore key on state 6 rather than a nonzero wait
           byte; this tick retains the next command without fetching it. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE selected-primary state-6 wrap fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->live_foundation.play_state.actor_state[actor] = 6U;
            scene->live_foundation.play_state.wait_counter[actor] = 10U;
        }
        scene->live_foundation.play_state.wait_counter[passer] = 0U;
        scene->live_foundation.play_state.stream_offset[passer] = 0x009BU;
        scene->live_foundation.last_step_offset[passer] = 0x009BU;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.actor_state[passer] != 6U ||
            scene->live_foundation.play_state.wait_counter[passer] != 0xFFU ||
            scene->live_foundation.play_state.stream_offset[passer] != 0x009BU ||
            scene->live_foundation.last_step_offset[passer] != 0x009BU) {
            LIVE_FAIL("LIVE selected-primary state-6 zero wrap failed");
        }

        /* A stale nonzero wait byte in state 4 cannot suppress command
           dispatch. Exact opcode 3 at $0096 seeds 10/state 6 and advances to
           $009B; the following state-6 update decrements without fetching. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE selected-primary opcode-3 fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->live_foundation.play_state.actor_state[actor] = 6U;
            scene->live_foundation.play_state.wait_counter[actor] = 10U;
        }
        scene->live_foundation.play_state.actor_state[passer] = 4U;
        scene->live_foundation.play_state.wait_counter[passer] = 0xA5U;
        scene->live_foundation.play_state.stream_offset[passer] = 0x0096U;
        scene->live_foundation.last_step_offset[passer] = 0x0096U;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.actor_state[passer] != 6U ||
            scene->live_foundation.play_state.wait_counter[passer] != 10U ||
            scene->live_foundation.play_state.stream_offset[passer] != 0x009BU ||
            scene->live_foundation.last_step_offset[passer] != 0x009BU) {
            LIVE_FAIL("LIVE selected-primary opcode-3 wait seed failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.actor_state[passer] != 6U ||
            scene->live_foundation.play_state.wait_counter[passer] != 9U ||
            scene->live_foundation.play_state.stream_offset[passer] != 0x009BU ||
            scene->live_foundation.last_step_offset[passer] != 0x009BU) {
            LIVE_FAIL("LIVE selected-primary opcode-3 retained wait failed");
        }

        /* Ordinary actors use the same state table after the selected-primary
           call. Exact opcode 3 at $062C seeds 30 and advances to $0631; its
           next ordinary state-6 dispatch decrements and retains that cursor. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE ordinary opcode-3 fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->live_foundation.play_state.actor_state[actor] = 6U;
            scene->live_foundation.play_state.wait_counter[actor] = 10U;
        }
        scene->live_foundation.play_state.actor_state[ordinary_wait_actor] = 4U;
        scene->live_foundation.play_state
            .wait_counter[ordinary_wait_actor] = 0xA5U;
        scene->live_foundation.play_state
            .stream_offset[ordinary_wait_actor] = 0x062CU;
        scene->live_foundation.last_step_offset[ordinary_wait_actor] = 0x062CU;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state
                    .actor_state[ordinary_wait_actor] != 6U ||
            scene->live_foundation.play_state
                    .wait_counter[ordinary_wait_actor] != 30U ||
            scene->live_foundation.play_state
                    .stream_offset[ordinary_wait_actor] != 0x0631U ||
            scene->live_foundation.last_step_offset[ordinary_wait_actor] !=
                0x0631U) {
            LIVE_FAIL("LIVE ordinary opcode-3 wait seed failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state
                    .actor_state[ordinary_wait_actor] != 6U ||
            scene->live_foundation.play_state
                    .wait_counter[ordinary_wait_actor] != 29U ||
            scene->live_foundation.play_state
                    .stream_offset[ordinary_wait_actor] != 0x0631U ||
            scene->live_foundation.last_step_offset[ordinary_wait_actor] !=
                0x0631U) {
            LIVE_FAIL("LIVE ordinary opcode-3 retained wait failed");
        }

        /* Discriminate opcode 21's exact +10 clock branch. The accepted
           typed clocks satisfy $058A>=4,$0357=0,$0358>=4 and fixed
           $F07E-$F0B9 supplies the exact clear bit-1 result. */
        scene->state.shot_clock = 4U;
        scene->state.clock_minutes = 0U;
        scene->state.clock_seconds = 4U;
        scene->live_foundation.play_state.actor_state[passer] = 4U;
        scene->live_foundation.play_state.wait_counter[passer] = 0U;
        scene->live_foundation.play_state.stream_offset[passer] = 0x00DCU;
        scene->live_foundation.last_step_offset[passer] = 0x00DCU;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) ||
            scene->live_foundation.play_state.stream_offset[passer] !=
                0x00E6U ||
            scene->live_foundation.last_step_offset[passer] != 0x00E6U ||
            scene->live_foundation.deferred[passer]) {
            LIVE_FAIL("LIVE selected-primary opcode-21 +10 branch failed");
        }

        /* Bank05 action $17 is a selected shot dispatcher, not an idle state.
           A source opcode-9 record that writes state 0/action $17 reaches the
           bounded native close-shot admission adapter in the same update;
           the accepted phase-table assets remain source-backed. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE action-17 close-shot fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        scene->actors[passer].position.x = (int16_t)(
            scene->orientation_state.offensive_hoop.x + 8);
        scene->actors[passer].position.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
        scene->actors[passer].anchor = scene->actors[passer].position;
        scene->live_foundation.play_state.actor_state[passer] = 4U;
        scene->live_foundation.play_state.action_state_046e[passer] = 0U;
        scene->live_foundation.play_state.stream_offset[passer] = 0x008CU;
        scene->live_foundation.last_step_offset[passer] = 0x008CU;
        if (!scene_attach_ball(scene) ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE action-17 close-shot setup invalid");
        }
        before = *scene;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || !no_shot.requested ||
            no_shot.actor_index != passer ||
            !no_shot.playback_supported || no_shot.deferred ||
            scene->action_serial != before.action_serial + 1U ||
            memcmp(scene->cpu_actors, before.cpu_actors,
                   sizeof(scene->cpu_actors)) != 0 ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != passer ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            !scene->live_foundation.last_shot_request ||
            !scene->live_foundation.last_shot_playback_supported ||
            scene->live_foundation.last_shot_deferred) {
            LIVE_FAIL("LIVE action-17 close-shot dispatch failed");
        }

        /* A non-$17 state-0 action remains outside this shot seam. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE action-17 nontrigger fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        scene->live_foundation.play_state.actor_state[passer] = 0U;
        scene->live_foundation.play_state.action_state_046e[passer] = 0x18U;
        primary_stream_before = (uint16_t)scene->action_serial;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            no_shot.deferred || scene->shot_kind !=
                TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
            scene->action_serial != primary_stream_before ||
            scene->live_foundation.play_state.actor_state[passer] != 0U ||
            scene->live_foundation.play_state
                    .action_state_046e[passer] != 0x18U) {
            LIVE_FAIL("LIVE non-action-17 state changed shot ownership");
        }

        /* A far selected-primary action $17 uses the same source-backed jump
           geometry/profile/playback without borrowing Home's controller. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE action-17 automatic jump fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        scene->actors[passer].position.x = 320;
        scene->actors[passer].position.y = 144;
        scene->actors[passer].anchor = scene->actors[passer].position;
        scene->live_foundation.play_state.actor_state[passer] = 4U;
        scene->live_foundation.play_state.action_state_046e[passer] = 0U;
        scene->live_foundation.play_state.stream_offset[passer] = 0x008CU;
        scene->live_foundation.last_step_offset[passer] = 0x008CU;
        if (!scene_attach_ball(scene) || !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE action-17 automatic jump attachment rejected");
        }
        before = *scene;
        memset(&neutral_one, 0, sizeof(neutral_one));
        memset(&neutral_two, 0, sizeof(neutral_two));
        /* Exercise the complete outer update so its supported-playback
           classification cannot regress to accepting close shots only. */
        if (!tecmo_gameplay_scene_update(
                scene, &neutral_one, &neutral_two) ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
            scene->shot_actor != passer ||
            scene->shot_controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->action_serial != before.action_serial + 1U ||
            !scene->jump_playback_active ||
            !scene->live_foundation.last_shot_request ||
            scene->live_foundation.last_shot_deferred ||
            !scene->live_foundation.last_shot_playback_supported ||
            !scene_ownership_valid(scene)) {
            LIVE_FAIL("LIVE action-17 automatic jump dispatch failed");
        }
        /* A controllerless jump consumes no human B state. NULL playback
           advances the same owned lifecycle and keeps the ball released. */
        primary_stream_before = scene->shot_frame;
        if (!scene_update_shot(scene, NULL) ||
            scene->shot_frame != primary_stream_before + 1U ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            !scene_ownership_valid(scene)) {
            LIVE_FAIL("LIVE action-17 automatic jump playback failed");
        }
        pass_updates = 1U;
        while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               pass_updates < 160U) {
            if (!scene_update_shot(scene, NULL)) {
                LIVE_FAIL("LIVE action-17 automatic jump lifecycle rejected");
            }
            ++pass_updates;
        }
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
            scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            !scene->loose_ball_state.active ||
            !scene_ownership_valid(scene)) {
            LIVE_FAIL("LIVE action-17 automatic jump did not retain loose ball");
        }

        /* The typed automatic entry rejects a human-owned offense without
           mutating even one scene byte; public controller admission remains
           the only human far/jump entry. */
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &bound) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, passer) ||
            !scene_attach_ball(scene)) {
            LIVE_FAIL("LIVE action-17 human ownership fixture rejected");
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        scene->actors[passer].position.x = 320;
        scene->actors[passer].position.y = 144;
        scene->actors[passer].anchor = scene->actors[passer].position;
        if (!scene_attach_ball(scene)) {
            LIVE_FAIL("LIVE action-17 human attachment rejected");
        }
        before = *scene;
        if (scene_start_automatic_cpu_shot_actor(scene, passer) ||
            memcmp(scene, &before, sizeof(before)) != 0) {
            LIVE_FAIL("LIVE automatic shot weakened human admission");
        }
        if (scene_start_automatic_cpu_shot_actor(scene, captured_receiver) ||
            memcmp(scene, &before, sizeof(before)) != 0) {
            LIVE_FAIL("LIVE automatic nonholder shot was not transactional");
        }
        if (!scene_start_shot_actor(scene, 0U, passer) ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
            scene->shot_controller != 0U ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
            LIVE_FAIL("LIVE human jump admission changed");
        }
    }

    if (!scene_test_pretip_cpu_common_tail_handoff(
            scene, &bound, message, message_size)) {
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        return false;
    }

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
        scene->live_foundation.formation_index != 32U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        LIVE_FAIL("LIVE initializer seeds or formation identity failed");
    }
    for (actor = 0U; actor < 10U; ++actor) {
        if (scene->live_foundation.formation_start_offset[actor] !=
                scene->cpu_steering_assets.formation_stream_offsets[32U][actor] ||
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
    memset(&possession_trace, 0, sizeof(possession_trace));
    if (!tecmo_gameplay_scene_possession_trace_snapshot(
            scene, &possession_trace) ||
        possession_trace.contract_tag !=
            TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG ||
        possession_trace.raw_0308_primary_actor != 0U ||
        possession_trace.raw_0309_defender_actor != 5U ||
        possession_trace.raw_030a_offense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        possession_trace.raw_030b_defense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        possession_trace.raw_030c_030d_control_mode[
            TECMO_GAMEPLAY_TEAM_AWAY] != 0U ||
        possession_trace.raw_030c_030d_control_mode[
            TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        !possession_trace.semantic_live_synchronized) {
        LIVE_FAIL("LIVE possession trace snapshot did not mirror typed state");
    }
    malformed_scene = *scene;
    malformed_scene.live_foundation.actor_selector_flags[0U] = 0x20U;
    snapshot = malformed_scene;
    memset(&possession_trace, 0xA5, sizeof(possession_trace));
    if (tecmo_gameplay_scene_possession_trace_snapshot(
            &malformed_scene, &possession_trace) ||
        possession_trace.contract_tag != 0xA5A5A5A5U ||
        memcmp(&malformed_scene, &snapshot, sizeof(malformed_scene)) != 0) {
        LIVE_FAIL("LIVE malformed possession trace did not fail closed");
    }
    /* Made-score $8FB9-$9042 swaps the selected pair, clears both selected
       actor lifecycles, toggles all role bits, and resets the leading $9621
       aggregation/global fields. A stale ordinary barrier cursor on the old
       holder must not become the new side's selected lifecycle. */
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.actor_state[0U] = 0x0BU;
    candidate_foundation.play_state.stream_offset[0U] = 0x0627U;
    candidate_foundation.last_step_offset[0U] = 0x0627U;
    candidate_foundation.play_state.action_state_046e[0U] = 0x18U;
    candidate_foundation.play_state.wait_counter[0U] = 0x1EU;
    candidate_foundation.play_state.actor_state[5U] = 0x0BU;
    candidate_foundation.play_state.aggregation_06df = 3U;
    candidate_foundation.play_state.aggregation_06e1 = 0x21U;
    candidate_foundation.play_state.global_0790 = 0x80U;
    memcpy(selector_flags_before, candidate_foundation.actor_selector_flags,
           sizeof(selector_flags_before));
    if (!tecmo_gameplay_live_foundation_score_restart_transition(
            &scene->cpu_steering_assets, TECMO_GAMEPLAY_TEAM_HOME,
            &candidate_foundation) ||
        candidate_foundation.primary_actor != 5U ||
        candidate_foundation.defender_actor != 0U ||
        candidate_foundation.offense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        candidate_foundation.defense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        candidate_foundation.play_state.actor_state[0U] != 0U ||
        candidate_foundation.play_state.actor_state[5U] != 0U ||
        candidate_foundation.play_state.action_state_046e[0U] != 0U ||
        candidate_foundation.play_state.action_state_046e[5U] != 0U ||
        candidate_foundation.play_state.action[0U] != 0x30U ||
        candidate_foundation.play_state.action[5U] != 0x30U ||
        candidate_foundation.play_state.wait_counter[0U] != 0U ||
        candidate_foundation.play_state.aggregation_06df != 0U ||
        candidate_foundation.play_state.aggregation_06e1 != 0U ||
        candidate_foundation.play_state.global_0790 != 0U ||
        candidate_foundation.actor_selector_flags[0U] !=
            (uint8_t)(selector_flags_before[0U] ^ 0x10U) ||
        candidate_foundation.actor_selector_flags[9U] !=
            (uint8_t)(selector_flags_before[9U] ^ 0x10U) ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE scored restart selected-pair reset diverged");
    }
    /* Bank05 `$901F` state 1 reaches Bank06 `$8661-$8727`, then the shared
       `$8728` refresh. The isolated helper keeps its large rollback snapshots
       out of this already-large regression stack frame. */
    if (!scene_test_score_restart_auto_pass_selector(
            scene, &candidate_foundation)) {
        LIVE_FAIL("LIVE natural auto-pass selector matrix failed");
    }
    candidate_foundation = foundation_before;
    snapshot.live_foundation = candidate_foundation;
    if (tecmo_gameplay_live_foundation_score_restart_transition(
            &scene->cpu_steering_assets, TECMO_GAMEPLAY_TEAM_AWAY,
            &candidate_foundation) ||
        memcmp(&candidate_foundation, &snapshot.live_foundation,
               sizeof(candidate_foundation)) != 0) {
        LIVE_FAIL("LIVE scored restart rejection was not transactional");
    }

    /* A no-source miss promotion is deliberately not a claimant transaction.
       Automatic play receives only the bounded selected-stream normalization;
       human ownership rejects without mutation. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    candidate_foundation.play_state.actor_state[0U] = 0x0BU;
    candidate_foundation.play_state.stream_offset[0U] = 0x0488U;
    candidate_foundation.last_step_offset[0U] = 0x0488U;
    candidate_foundation.play_state.wait_counter[0U] = 0x1EU;
    candidate_foundation.play_state.target_x[0U] = 200;
    candidate_foundation.play_state.target_depth[0U] = 144;
    candidate_foundation.source_target_valid[0U] = true;
    candidate_foundation.deferred[0U] = true;
    candidate_foundation.deferred_reason[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE21_GATE_INPUTS;
    if (!tecmo_gameplay_live_foundation_normalize_automatic_primary(
            &scene->cpu_steering_assets, 0U, &candidate_foundation) ||
        candidate_foundation.play_state.stream_offset[0U] != 0x007DU ||
        candidate_foundation.last_step_offset[0U] != 0x007DU ||
        candidate_foundation.play_state.actor_state[0U] != 0x04U ||
        candidate_foundation.play_state.action_state_046e[0U] != 0x18U ||
        candidate_foundation.play_state.wait_counter[0U] != 0U ||
        candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.deferred[0U] ||
        candidate_foundation.deferred_reason[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE) {
        LIVE_FAIL("LIVE automatic miss-primary normalization diverged");
    }
    candidate_foundation = foundation_before;
    snapshot.live_foundation = candidate_foundation;
    if (tecmo_gameplay_live_foundation_normalize_automatic_primary(
            &scene->cpu_steering_assets, 0U, &candidate_foundation) ||
        memcmp(&candidate_foundation, &snapshot.live_foundation,
               sizeof(candidate_foundation)) != 0) {
        LIVE_FAIL("LIVE human miss-primary normalization was not rejected");
    }
    /* Bank05 $B24F-$B32B / Bank06 $81F7-$82D: Mark Jackson slot 0
       passes to John Starks slot 1; the old holder resumes ordinary command
       state 4/$0B63 and automatic defense selects by descending eligibility
       plus configured `$06CB` link, not receiver+5 arithmetic. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    candidate_foundation.dynamic_link[9U] = 1U;
    candidate_foundation.defender_eligible[9U] = false;
    candidate_foundation.dynamic_link[8U] = 0U;
    candidate_foundation.defender_eligible[8U] = true;
    candidate_foundation.dynamic_link[7U] = 0U;
    candidate_foundation.defender_eligible[7U] = true;
    candidate_foundation.dynamic_link[6U] = 1U;
    candidate_foundation.defender_eligible[6U] = true;
    if (!tecmo_gameplay_live_foundation_pass_handoff(
            &scene->cpu_steering_assets, 1U, &candidate_foundation)) {
        LIVE_FAIL("LIVE B24F/B27B/B317 handoff rejected");
    }
    if (
        candidate_foundation.primary_actor != 1U ||
        candidate_foundation.prior_selected_actor != 0U ||
        candidate_foundation.play_state.actor_state[0U] != 4U ||
        candidate_foundation.play_state.stream_offset[0U] != 0x0B63U ||
        candidate_foundation.last_step_offset[0U] != 0x0B63U ||
        candidate_foundation.defender_actor != 6U ||
        candidate_foundation.prior_defender_actor != 5U) {
        LIVE_FAIL("LIVE B24F/B27B/B317 descending pass handoff failed");
    }
    /* Highest eligible matching X wins; slot 7 deliberately proves the
       result is not receiver+5 (which would be slot 6). */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    candidate_foundation.dynamic_link[9U] = 1U;
    candidate_foundation.dynamic_link[8U] = 1U;
    candidate_foundation.defender_eligible[9U] = true;
    candidate_foundation.defender_eligible[8U] = true;
    if (!tecmo_gameplay_live_foundation_pass_handoff(
            &scene->cpu_steering_assets, 1U, &candidate_foundation) ||
        candidate_foundation.defender_actor != 9U) {
        LIVE_FAIL("LIVE B317 highest eligible linked defender did not win");
    }
    /* $030C[$030B]==0 bypasses B317 and preserves the human-selected
       defender, while still performing the B24F/B27B offensive handoff. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 0U;
    candidate_foundation.dynamic_link[9U] = 1U;
    if (!tecmo_gameplay_live_foundation_pass_handoff(
            &scene->cpu_steering_assets, 1U, &candidate_foundation) ||
        candidate_foundation.defender_actor != 5U ||
        candidate_foundation.prior_defender_actor != 9U) {
        LIVE_FAIL("LIVE human opponent incorrectly ran B317 reselection");
    }
    /* The bounded native choice for raw-loop underflow is fail-closed: no
       match means no partial selected actor, command, or defender mutation. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    for (actor = 0U; actor < 10U; ++actor) {
        candidate_foundation.defender_eligible[actor] = false;
    }
    snapshot.live_foundation = candidate_foundation;
    if (tecmo_gameplay_live_foundation_pass_handoff(
            &scene->cpu_steering_assets, 1U, &candidate_foundation) ||
        memcmp(&candidate_foundation, &snapshot.live_foundation,
               sizeof(candidate_foundation)) != 0) {
        LIVE_FAIL("LIVE B317 no-match failure was not transactional");
    }

    /* Bank05 $B87C-$B98A is a claimant settlement, separate from the B24F
       pass helper above. Same-side claimant 1 proves the exact no-swap branch:
       $030A/$030B and every $04B0 bit-$10 selector stay put while the automatic
       defender scan walks 9..0 and chooses the first dynamic-link match. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    candidate_foundation.dynamic_link[9U] = 1U;
    candidate_foundation.dynamic_link[8U] = 1U;
    memcpy(selector_flags_before, candidate_foundation.actor_selector_flags,
           sizeof(selector_flags_before));
    if (!tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 1U, TECMO_GAMEPLAY_TEAM_AWAY,
            &candidate_foundation, &claimant_settlement) ||
        claimant_settlement.contract_tag !=
            TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG ||
        claimant_settlement.raw_0308_before != 0U ||
        claimant_settlement.raw_0309_before != 5U ||
        claimant_settlement.raw_030a_before != TECMO_GAMEPLAY_TEAM_AWAY ||
        claimant_settlement.raw_030b_before != TECMO_GAMEPLAY_TEAM_HOME ||
        claimant_settlement.raw_0308_after != 1U ||
        claimant_settlement.raw_0309_after != 9U ||
        claimant_settlement.side_context_swapped ||
        claimant_settlement.raw_04b0_bit10_toggled ||
        claimant_settlement.raw_035a_save_and_toggle_observed ||
        !claimant_settlement.automatic_defender_scan_ran ||
        !claimant_settlement.automatic_defender_match_found ||
        candidate_foundation.last_possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        candidate_foundation.offense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        candidate_foundation.defense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        candidate_foundation.primary_actor != 1U ||
        candidate_foundation.defender_actor != 9U ||
        candidate_foundation.selected_actor_by_side[
            TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        candidate_foundation.candidate_actor_by_side[
            TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        candidate_foundation.selected_actor_by_side[
            TECMO_GAMEPLAY_TEAM_HOME] != 9U ||
        candidate_foundation.play_state.stream_offset[1U] != 0x007DU ||
        candidate_foundation.play_state.actor_state[1U] != 0x04U ||
        candidate_foundation.last_step_offset[1U] != 0x007DU ||
        memcmp(selector_flags_before,
               candidate_foundation.actor_selector_flags,
               sizeof(selector_flags_before)) != 0 ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE B87C same-side claimant transaction diverged");
    }

    /* On the exact $B8C1 predicate, other-side claimant 6 first replaces
       $0309 with old $0308, swaps $030A/$030B, then $9042 EORs bit-$10 for
       every slot. After that swap the automatic-defense scan again descends
       9..0, so away slot 4 wins over slot 3. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    candidate_foundation.dynamic_link[4U] = 6U;
    candidate_foundation.dynamic_link[3U] = 6U;
    memcpy(selector_flags_before, candidate_foundation.actor_selector_flags,
           sizeof(selector_flags_before));
    if (!tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 6U, TECMO_GAMEPLAY_TEAM_HOME,
            &candidate_foundation, &claimant_settlement) ||
        !claimant_settlement.candidate_replaced_primary ||
        !claimant_settlement.side_context_swapped ||
        !claimant_settlement.raw_04b0_bit10_toggled ||
        !claimant_settlement.raw_035a_save_and_toggle_observed ||
        !claimant_settlement.automatic_defender_scan_ran ||
        !claimant_settlement.automatic_defender_match_found ||
        claimant_settlement.raw_0308_after != 6U ||
        claimant_settlement.raw_0309_after != 4U ||
        claimant_settlement.raw_030a_after != TECMO_GAMEPLAY_TEAM_HOME ||
        claimant_settlement.raw_030b_after != TECMO_GAMEPLAY_TEAM_AWAY ||
        candidate_foundation.last_possession != TECMO_GAMEPLAY_TEAM_HOME ||
        candidate_foundation.offense_side != TECMO_GAMEPLAY_TEAM_HOME ||
        candidate_foundation.defense_side != TECMO_GAMEPLAY_TEAM_AWAY ||
        candidate_foundation.primary_actor != 6U ||
        candidate_foundation.defender_actor != 4U ||
        candidate_foundation.selected_actor_by_side[
            TECMO_GAMEPLAY_TEAM_HOME] != 6U ||
        candidate_foundation.candidate_actor_by_side[
            TECMO_GAMEPLAY_TEAM_HOME] != 7U ||
        candidate_foundation.selected_actor_by_side[
            TECMO_GAMEPLAY_TEAM_AWAY] != 4U ||
        candidate_foundation.play_state.stream_offset[6U] != 0x007DU ||
        candidate_foundation.play_state.actor_state[6U] != 0x04U ||
        candidate_foundation.actor_selector_flags[0U] !=
            (uint8_t)(selector_flags_before[0U] ^ 0x10U) ||
        candidate_foundation.actor_selector_flags[9U] !=
            (uint8_t)(selector_flags_before[9U] ^ 0x10U) ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE B87C cross-side claimant transaction diverged");
    }

    /* An automatic scan with no exact $04B0/$06CB match retains the source
       branch's already-selected defender (old $0308 after a side crossing),
       rather than failing closed as the older pass-only helper does. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
    for (actor = 0U; actor < 10U; ++actor) {
        candidate_foundation.dynamic_link[actor] = 0U;
    }
    if (!tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 6U, TECMO_GAMEPLAY_TEAM_HOME,
            &candidate_foundation, &claimant_settlement) ||
        !claimant_settlement.automatic_defender_scan_ran ||
        claimant_settlement.automatic_defender_match_found ||
        candidate_foundation.defender_actor != 0U ||
        candidate_foundation.selected_actor_by_side[
            TECMO_GAMEPLAY_TEAM_AWAY] != 0U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE B87C no-match defender fallback diverged");
    }

    /* Candidate equal to $0308 takes the $B8C1 BEQ: it cannot swap side
       context or toggle selector flags. Human defense further bypasses the
       $B8F6 descending scan without fabricating a defender change. */
    candidate_foundation = foundation_before;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 0U;
    candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 0U;
    memcpy(selector_flags_before, candidate_foundation.actor_selector_flags,
           sizeof(selector_flags_before));
    if (!tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 0U, TECMO_GAMEPLAY_TEAM_AWAY,
            &candidate_foundation, &claimant_settlement) ||
        claimant_settlement.candidate_replaced_primary ||
        claimant_settlement.side_context_swapped ||
        claimant_settlement.raw_04b0_bit10_toggled ||
        claimant_settlement.automatic_defender_scan_ran ||
        candidate_foundation.primary_actor != 0U ||
        candidate_foundation.defender_actor != 5U ||
        memcmp(selector_flags_before,
               candidate_foundation.actor_selector_flags,
               sizeof(selector_flags_before)) != 0 ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE B87C same-primary no-toggle branch diverged");
    }

    /* Exercise every $B98B byte through the typed transaction. Human defense
       bypasses the optional $B8F6 scan so this isolates the exact remap table
       for both same-side and side-crossing claimants. */
    for (actor = 0U; actor < 10U; ++actor) {
        TecmoGameplayTeam claimant_team = actor < 5U
            ? TECMO_GAMEPLAY_TEAM_AWAY : TECMO_GAMEPLAY_TEAM_HOME;
        candidate_foundation = foundation_before;
        candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 0U;
        candidate_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 0U;
        if (!tecmo_gameplay_live_foundation_claimant_settlement(
                &scene->cpu_steering_assets, (uint8_t)actor,
                (uint8_t)claimant_team, &candidate_foundation,
                &claimant_settlement) ||
            candidate_foundation.candidate_actor_by_side[claimant_team] !=
                exact_b87c_candidate_remap[actor]) {
            LIVE_FAIL("LIVE B98B claimant remap table diverged");
        }
    }

    /* Malformed selector input and a claimant/possession disagreement both
       reject without touching the caller's foundation or result output. */
    candidate_foundation = foundation_before;
    candidate_foundation.actor_selector_flags[6U] = 0x20U;
    snapshot.live_foundation = candidate_foundation;
    memset(&claimant_settlement, 0xA5, sizeof(claimant_settlement));
    if (tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 6U, TECMO_GAMEPLAY_TEAM_HOME,
            &candidate_foundation, &claimant_settlement) ||
        memcmp(&candidate_foundation, &snapshot.live_foundation,
               sizeof(candidate_foundation)) != 0 ||
        claimant_settlement.contract_tag != 0xA5A5A5A5U) {
        LIVE_FAIL("LIVE B87C malformed selector rollback failed");
    }
    candidate_foundation = foundation_before;
    snapshot.live_foundation = candidate_foundation;
    memset(&claimant_settlement, 0xA5, sizeof(claimant_settlement));
    if (tecmo_gameplay_live_foundation_claimant_settlement(
            &scene->cpu_steering_assets, 6U, TECMO_GAMEPLAY_TEAM_AWAY,
            &candidate_foundation, &claimant_settlement) ||
        memcmp(&candidate_foundation, &snapshot.live_foundation,
               sizeof(candidate_foundation)) != 0 ||
        claimant_settlement.contract_tag != 0xA5A5A5A5U) {
        LIVE_FAIL("LIVE B87C claimant/possession rollback failed");
    }
    /* LIVE foundation invariants fail closed independently, rather than
       relying on the caller to preserve aligned stream/matchup metadata. */
    candidate_foundation = foundation_before;
    candidate_foundation.last_step_offset[0U] ^= 1U;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE stream offset alignment negative was accepted");
    }
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.fixed_link_target[0U] =
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
    candidate_foundation.fixed_link_projection_active = false;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE fixed-link projection classification negative was accepted");
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
    candidate_foundation.play_state.target_object[0U] = 5U;
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
        candidate_foundation.play_state.target_object[0U] !=
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
    {
        TecmoGameplaySceneClaimantSettlementTrace trace_before =
            scene->claimant_settlement_trace;
        if (!scene_handoff_possession(scene, TECMO_GAMEPLAY_TEAM_HOME, 5U) ||
            !scene_sync_live_foundation(scene) ||
            scene->live_foundation.primary_actor != 5U ||
            !scene_handoff_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene) ||
            scene->live_foundation.primary_actor != 0U ||
            memcmp(&scene->claimant_settlement_trace, &trace_before,
                   sizeof(trace_before)) != 0) {
            LIVE_FAIL("LIVE pass/switch/handoff synchronization failed");
        }
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
    scene_test_enable_captured_play_inputs(&play_input);
    play_input.actor = 0U;
    play_input.step_budget = 2U;
    play_input.orientation_035a = 0U;
    memcpy(play_input.actor_position, positions, sizeof(positions));
    play_input.ball_position = positions[0U];
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
        scene_test_enable_captured_play_inputs(&play_input);
        play_input.actor = (uint8_t)actor;
        play_input.step_budget = 1U;
        memcpy(play_input.actor_position, positions, sizeof(positions));
        play_input.ball_position = positions[0U];
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

    /* Bank06 $9441-$946E reloads only ordinary state-4 actors: $944D skips
       $0308 and $9452 skips bit-$10 slots. A selected CPU ball-handler must
       therefore retain the production-supported opcode-4 source target when
       it crosses a 64-pixel formation bucket. */
    candidate_foundation = foundation_before;
    memset(&play_input, 0, sizeof(play_input));
    play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    scene_test_enable_captured_play_inputs(&play_input);
    play_input.actor = candidate_foundation.primary_actor;
    play_input.step_budget = 1U;
    play_input.orientation_035a = candidate_foundation.orientation;
    memcpy(play_input.actor_position, candidate_foundation.actor_position,
           sizeof(play_input.actor_position));
    play_input.ball_position = candidate_foundation.actor_position[0U];
    for (offset = 0U;
         offset < scene->cpu_steering_assets.command_record_count * 5U;
         offset = (uint16_t)(offset + 5U)) {
        if (!tecmo_gameplay_cpu_steering_decode_command(
                &scene->cpu_steering_assets, offset, &command) ||
            command.opcode != 4U) {
            continue;
        }
        candidate_foundation.play_state.stream_offset[
            candidate_foundation.primary_actor] = offset;
        candidate_foundation.last_step_offset[
            candidate_foundation.primary_actor] = offset;
        if (tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &play_result) &&
            !play_result.deferred &&
            candidate_foundation.source_target_valid[
                candidate_foundation.primary_actor]) {
            found_absolute_target = true;
            break;
        }
        candidate_foundation = foundation_before;
    }
    if (!found_absolute_target) {
        LIVE_FAIL("LIVE Bank04 object-target fixture missing");
    }
    preserved_target_x = candidate_foundation.play_state.target_x[
        candidate_foundation.primary_actor];
    preserved_target_depth = candidate_foundation.play_state.target_depth[
        candidate_foundation.primary_actor];
    /* Seed an ordinary actor target too, proving reloads discard only the
       metadata belonging to the replaced command stream. */
    candidate_foundation.play_state.target_object[1U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[1U] = positions[1U].x;
    candidate_foundation.play_state.target_depth[1U] = positions[1U].y;
    candidate_foundation.source_target_valid[1U] = true;
    memcpy(refresh_positions, candidate_foundation.actor_position,
           sizeof(refresh_positions));
    if (refresh_positions[candidate_foundation.primary_actor].x >= 64) {
        refresh_positions[candidate_foundation.primary_actor].x =
            (int16_t)(refresh_positions[
                candidate_foundation.primary_actor].x - 64);
    } else {
        refresh_positions[candidate_foundation.primary_actor].x =
            (int16_t)(refresh_positions[
                candidate_foundation.primary_actor].x + 64);
    }
    advanced_offset = candidate_foundation.play_state.stream_offset[
        candidate_foundation.primary_actor];
    if (!tecmo_gameplay_live_foundation_refresh_formation(
            &scene->cpu_steering_assets, refresh_positions,
            &candidate_foundation) ||
        candidate_foundation.play_state.stream_offset[
            candidate_foundation.primary_actor] != advanced_offset ||
        !candidate_foundation.source_target_valid[
            candidate_foundation.primary_actor] ||
        candidate_foundation.play_state.target_x[
            candidate_foundation.primary_actor] != preserved_target_x ||
        candidate_foundation.play_state.target_depth[
            candidate_foundation.primary_actor] != preserved_target_depth ||
        candidate_foundation.source_target_valid[1U] ||
        candidate_foundation.play_state.target_object[1U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        candidate_foundation.play_state.target_x[1U] != 0 ||
        candidate_foundation.play_state.target_depth[1U] != 0 ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE $944D selected-target preservation regressed");
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
    scene_test_enable_captured_play_inputs(&play_input);
    play_input.step_budget = 1U;
    memcpy(play_input.actor_position, positions, sizeof(positions));
    play_input.ball_position = positions[0U];
    /* This direct foundation fixture is a negative LIVE diagnostic test, not
       a gameplay proof: it places an already decoded canonical opcode-15
       record at the normal play-step boundary and proves that unavailable
       raw owners are recorded without a defender/stream/pose mutation. */
    {
        TecmoGameplayLiveFoundation opcode15_before = foundation_before;
        const uint8_t opcode15_actor = 0U;
        opcode15_before.play_state.stream_offset[opcode15_actor] = 0x0037U;
        opcode15_before.last_step_offset[opcode15_actor] = 0x0037U;
        opcode15_before.play_state.actor_state[opcode15_actor] = 0x0BU;
        opcode15_before.play_state.action_state_046e[opcode15_actor] = 0xC3U;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &opcode15_before)) {
            LIVE_FAIL("LIVE opcode-15 negative fixture was invalid");
        }
        candidate_foundation = opcode15_before;
        play_input.actor = opcode15_actor;
        if (!tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &play_result) ||
            play_result.command.opcode != 15U || !play_result.deferred ||
            play_result.deferred_reason !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE ||
            play_result.advanced || play_result.next_offset != 0x0037U ||
            candidate_foundation.primary_actor !=
                opcode15_before.primary_actor ||
            candidate_foundation.defender_actor !=
                opcode15_before.defender_actor ||
            candidate_foundation.play_state.stream_offset[opcode15_actor] !=
                opcode15_before.play_state.stream_offset[opcode15_actor] ||
            candidate_foundation.play_state.actor_state[opcode15_actor] !=
                opcode15_before.play_state.actor_state[opcode15_actor] ||
            candidate_foundation.play_state.action_state_046e[opcode15_actor] !=
                opcode15_before.play_state.action_state_046e[opcode15_actor] ||
            candidate_foundation.deferred_reason[opcode15_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE ||
            !candidate_foundation.opcode15_trace.observed ||
            candidate_foundation.opcode15_trace.branch !=
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW ||
            candidate_foundation.opcode15_trace.command_record_offset !=
                0x0037U ||
            candidate_foundation.opcode15_trace.missing_raw_mask !=
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK ||
            candidate_foundation.opcode15_trace.raw_0499_available ||
            candidate_foundation.opcode15_trace.raw_04b0_available ||
            candidate_foundation.opcode15_trace.raw_007e_available ||
            candidate_foundation.opcode15_trace.raw_06d5_06d6_available ||
            candidate_foundation.opcode15_trace.raw_0479_available ||
            candidate_foundation.opcode15_trace.raw_0442_044d_available ||
            candidate_foundation.opcode15_trace.raw_059e_available ||
            candidate_foundation.opcode15_trace.raw_actor_lifecycle_available ||
            candidate_foundation.opcode15_trace.raw_0308_before !=
                candidate_foundation.opcode15_trace.raw_0308_after ||
            candidate_foundation.opcode15_trace.raw_0309_before !=
                candidate_foundation.opcode15_trace.raw_0309_after ||
            candidate_foundation.opcode15_trace.actor_stream_before !=
                candidate_foundation.opcode15_trace.actor_stream_after ||
            candidate_foundation.opcode15_trace.actor_state_before !=
                candidate_foundation.opcode15_trace.actor_state_after ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            LIVE_FAIL("LIVE opcode-15 missing-raw diagnostic mutated state");
        }
    }
    /* Positive production-owner fixture for Bank06 `$9172-$9216` and its
       `$9248-$926F` state-7 follow-up. It proves the automatic `$0037`
       primary/formation replacement, then the automatic `$004B` defender
       replacement and persistent `$059E` consumer. */
    if (!scene_test_opcode15_live_owner(&scene->cpu_steering_assets,
                                         &foundation_before,
                                         &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-15 replacement/state-7 owner failed");
    }
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
    candidate_foundation.play_state.target_object[deferred_actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[deferred_actor] = 0;
    candidate_foundation.play_state.target_depth[deferred_actor] = 0;
    candidate_foundation.source_target_valid[deferred_actor] = false;
    play_input.actor = deferred_actor;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input, &candidate_foundation,
            &play_result) || !play_result.deferred ||
        play_result.deferred_reason ==
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
        candidate_foundation.source_target_valid[deferred_actor] ||
        candidate_foundation.deferred_reason[deferred_actor] !=
            play_result.deferred_reason ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE deferred target became an unproven movement target");
    }
    /* Intentional executor/foundation fixture: raw high-byte latch evidence
       is retained without being advertised as an in-court source target.
       Its exact signed delta still publishes the source direction used by the
       bounded direction adapter. Production never enables this input. */
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.stream_offset[0U] = 0x002DU;
    candidate_foundation.last_step_offset[0U] = 0x002DU;
    candidate_foundation.play_state.actor_state[0U] = 0x04U;
    candidate_foundation.play_state.wait_counter[0U] = 0U;
    candidate_foundation.play_state.target_object[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[0U] = 0;
    candidate_foundation.play_state.target_depth[0U] = 0;
    candidate_foundation.source_target_valid[0U] = false;
    play_input.actor = 0U;
    play_input.global_target_available = true;
    play_input.global_target.x = 0xFF00U;
    play_input.global_target.depth = 0x0100U;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        play_result.command.opcode != 13U || play_result.deferred ||
        !play_result.advanced ||
        !play_result.raw_target_valid ||
        play_result.raw_target_x != 0xFF00U ||
        play_result.raw_target_depth != 0x0100U ||
        candidate_foundation.source_target_valid[0U] ||
        !candidate_foundation.source_raw_target_valid[0U] ||
        !candidate_foundation.source_direction_valid[0U] ||
        candidate_foundation.play_state.target_object[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        (uint16_t)candidate_foundation.play_state.target_x[0U] != 0xFF00U ||
        (uint16_t)candidate_foundation.play_state.target_depth[0U] !=
            0x0100U ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-13 raw target validation regressed");
    }
    /* Opcode 20 preserves target storage bits but makes their former meaning
       inactive. Prove both semantic and raw provenance commit successfully,
       become mutually-exclusive stale storage, and cannot publish a target. */
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.stream_offset[0U] = 0x000FU;
    candidate_foundation.last_step_offset[0U] = 0x000FU;
    candidate_foundation.play_state.actor_state[0U] = 0x04U;
    candidate_foundation.play_state.wait_counter[0U] = 0U;
    candidate_foundation.play_state.target_object[0U] = 5U;
    candidate_foundation.play_state.target_x[0U] = positions[5U].x;
    candidate_foundation.play_state.target_depth[0U] = positions[5U].y;
    candidate_foundation.source_target_valid[0U] = true;
    candidate_foundation.source_raw_target_valid[0U] = false;
    candidate_foundation.source_inactive_target_storage[0U] = false;
    play_input.global_target.x =
        (uint16_t)play_input.actor_position[0U].x;
    play_input.global_target.depth =
        (uint16_t)(uint8_t)play_input.actor_position[0U].y;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        play_result.command.opcode != 20U || play_result.deferred ||
        candidate_foundation.play_state.target_object[0U] != 5U ||
        candidate_foundation.play_state.target_x[0U] != positions[5U].x ||
        candidate_foundation.play_state.target_depth[0U] != positions[5U].y ||
        candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.source_raw_target_valid[0U] ||
        !candidate_foundation.source_inactive_target_storage[0U] ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-20 semantic target invalidation failed");
    }
    candidate_foundation.source_target_valid[0U] = true;
    if (tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-20 inactive target provenance overlapped");
    }

    candidate_foundation = foundation_before;
    candidate_foundation.play_state.stream_offset[0U] = 0x0019U;
    candidate_foundation.last_step_offset[0U] = 0x0019U;
    candidate_foundation.play_state.actor_state[0U] = 0x04U;
    candidate_foundation.play_state.wait_counter[0U] = 0U;
    candidate_foundation.play_state.target_object[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.play_state.target_x[0U] = (int16_t)0xFF00U;
    candidate_foundation.play_state.target_depth[0U] = (int16_t)0x0100U;
    candidate_foundation.source_target_valid[0U] = false;
    candidate_foundation.source_raw_target_valid[0U] = true;
    candidate_foundation.source_inactive_target_storage[0U] = false;
    play_input.global_target.x = 0xFF10U;
    play_input.global_target.depth = 0xFF20U;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        play_result.command.opcode != 20U || play_result.deferred ||
        (uint16_t)candidate_foundation.play_state.target_x[0U] != 0xFF00U ||
        (uint16_t)candidate_foundation.play_state.target_depth[0U] != 0x0100U ||
        candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.source_raw_target_valid[0U] ||
        !candidate_foundation.source_inactive_target_storage[0U] ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-20 raw target invalidation failed");
    }
    /* A subsequent real opcode-2 target write replaces the inactive bits and
       restores semantic provenance. */
    candidate_foundation.play_state.stream_offset[0U] = 0x00D7U;
    candidate_foundation.last_step_offset[0U] = 0x00D7U;
    play_input.common_tail_ba_available = true;
    play_input.flags_ba = 0U;
    if (!tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &play_result) ||
        play_result.command.opcode != 2U || play_result.deferred ||
        !candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.source_raw_target_valid[0U] ||
        candidate_foundation.source_inactive_target_storage[0U] ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate_foundation)) {
        LIVE_FAIL("LIVE opcode-20 stale storage replacement failed");
    }
    candidate_foundation.source_inactive_target_storage[0U] = true;
    tecmo_gameplay_live_foundation_init(&candidate_foundation);
    if (candidate_foundation.source_inactive_target_storage[0U] ||
        candidate_foundation.source_target_valid[0U] ||
        candidate_foundation.source_raw_target_valid[0U] ||
        candidate_foundation.play_state.target_x[0U] != 0 ||
        candidate_foundation.play_state.target_depth[0U] != 0) {
        LIVE_FAIL("LIVE opcode-20 inactive storage survived reset");
    }
    play_input.global_target_available = false;
    candidate_foundation = foundation_before;
    candidate_foundation.play_state.target_object[0U] = 5U;
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

    /* Lock the canonical ordinary iteration independently of command effects:
       selected-primary prepass is covered above, then Bank06 $8284 LDX #$09 /
       $82A4 DEX must expose actors 9..0 with no extra element. */
    for (actor = 0U; actor < 10U; ++actor) {
        if (scene_bank06_ordinary_actor_at(actor) != (uint8_t)(9U - actor)) {
            LIVE_FAIL("LIVE Bank06 descending actor order regressed");
        }
    }
    if (scene_bank06_ordinary_actor_at(10U) !=
            TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
        LIVE_FAIL("LIVE Bank06 actor-order sentinel regressed");
    }

    /* Exact selected-primary opcode-7 zero subset. At this prepass the
       ordinary selector seam proves slot 10 `$0478==0`, so each canonical
       record advances and returns state 4. An ordinary actor parked on the
       same handler in the later 9..0 traversal must still defer: opcode 6 can
       change `$0478` between those source phases. */
    {
        static const uint16_t selected_offset[2U] = {0x013BU, 0x0172U};
        static const uint16_t selected_next[2U] = {0x0140U, 0x0177U};
        for (size_t item = 0U; item < 2U; ++item) {
            TecmoGameplaySceneCpuShotRequest no_shot;
            uint8_t primary;
            uint8_t ordinary_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
                !scene_handoff_possession(
                    scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
                !scene_sync_live_foundation(scene)) {
                LIVE_FAIL("LIVE opcode-7 selected-primary setup rejected");
            }
            candidate_foundation = scene->live_foundation;
            primary = candidate_foundation.primary_actor;
            for (int scan = 9; scan >= 0; --scan) {
                if ((uint8_t)scan != primary &&
                    (uint8_t)scan != candidate_foundation.defender_actor) {
                    ordinary_actor = (uint8_t)scan;
                    break;
                }
            }
            if (primary != scene->ball_holder || ordinary_actor >= 10U) {
                LIVE_FAIL("LIVE opcode-7 selected-primary actors unavailable");
            }
            for (actor = 0U; actor < 10U; ++actor) {
                candidate_foundation.play_state.actor_state[actor] = 0x06U;
                candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
                candidate_foundation.deferred[actor] = false;
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            }
            candidate_foundation.play_state.actor_state[primary] = 0x04U;
            candidate_foundation.play_state.wait_counter[primary] = 0U;
            candidate_foundation.play_state.stream_offset[primary] =
                selected_offset[item];
            candidate_foundation.last_step_offset[primary] =
                selected_offset[item];
            candidate_foundation.play_state.actor_state[ordinary_actor] =
                0x04U;
            candidate_foundation.play_state.wait_counter[ordinary_actor] = 0U;
            candidate_foundation.play_state.stream_offset[ordinary_actor] =
                0x0172U;
            candidate_foundation.last_step_offset[ordinary_actor] = 0x0172U;
            if (!tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &candidate_foundation)) {
                LIVE_FAIL("LIVE opcode-7 selected-primary fixture rejected");
            }
            scene->live_foundation = candidate_foundation;
            if (item == 1U) {
                TecmoGameplayScene failed = *scene;
                TecmoGameplayScene before_failed;
                failed.cpu_actors[ordinary_actor].decision_serial = UINT32_MAX;
                before_failed = failed;
                memset(&no_shot, 0, sizeof(no_shot));
                if (scene_update_ai(&failed, &no_shot) ||
                    memcmp(&failed, &before_failed, sizeof(failed)) != 0) {
                    LIVE_FAIL("LIVE opcode-7 late failure was not atomic");
                }
            }
            memset(&no_shot, 0, sizeof(no_shot));
            if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
                scene->live_foundation.play_state.stream_offset[primary] !=
                    selected_next[item] ||
                scene->live_foundation.play_state.actor_state[primary] !=
                    0x04U ||
                scene->live_foundation.deferred[primary] ||
                scene->live_foundation.play_state
                        .stream_offset[ordinary_actor] != 0x0172U ||
                !scene->live_foundation.deferred[ordinary_actor] ||
                scene->live_foundation.deferred_reason[ordinary_actor] !=
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE) {
                LIVE_FAIL("LIVE opcode-7 selected probe leaked to ordinary actor");
            }
        }
    }

    /* Synthetic traversal ordering: an earlier ordinary opcode 6 is the
       source writer that can make `$0478=$13`; a later opcode 7 therefore
       remains unavailable rather than reusing the selected-primary zero. */
    {
        TecmoGameplaySceneCpuShotRequest no_shot;
        uint8_t earlier_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t later_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-6/7 traversal setup rejected");
        }
        candidate_foundation = scene->live_foundation;
        for (int scan = 9; scan >= 0; --scan) {
            if ((uint8_t)scan == candidate_foundation.primary_actor ||
                (uint8_t)scan == candidate_foundation.defender_actor) {
                continue;
            }
            if (earlier_actor >= 10U) earlier_actor = (uint8_t)scan;
            else {
                later_actor = (uint8_t)scan;
                break;
            }
        }
        if (earlier_actor >= 10U || later_actor >= 10U ||
            earlier_actor <= later_actor) {
            LIVE_FAIL("LIVE opcode-6/7 traversal actors unavailable");
        }
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.play_state.actor_state[actor] = 0x06U;
            candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
            candidate_foundation.deferred[actor] = false;
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        candidate_foundation.play_state.actor_state[earlier_actor] = 0x04U;
        candidate_foundation.play_state.wait_counter[earlier_actor] = 0U;
        candidate_foundation.play_state.stream_offset[earlier_actor] = 0x0190U;
        candidate_foundation.last_step_offset[earlier_actor] = 0x0190U;
        candidate_foundation.play_state.actor_state[later_actor] = 0x04U;
        candidate_foundation.play_state.wait_counter[later_actor] = 0U;
        candidate_foundation.play_state.stream_offset[later_actor] = 0x013BU;
        candidate_foundation.last_step_offset[later_actor] = 0x013BU;
        scene->live_foundation = candidate_foundation;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            !scene->live_foundation.deferred[earlier_actor] ||
            scene->live_foundation.play_state.stream_offset[earlier_actor] !=
                0x0190U ||
            !scene->live_foundation.deferred[later_actor] ||
            scene->live_foundation.deferred_reason[later_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE ||
            scene->live_foundation.play_state.stream_offset[later_actor] !=
                0x013BU) {
            LIVE_FAIL("LIVE opcode-6/7 ordinary traversal leaked probe");
        }
    }

    /* Full source stream timing, parked explicitly because the play-selection
       route into offset $017C is not yet proven: opcode 5 -> opcode 9 ->
       opcode 3's six-count wait -> opcode 23 -> retained opcode 6 -> next-
       update action-$10 pass gather. One handler is fetched per update. */
    {
        TecmoGameplaySceneCpuShotRequest no_shot;
        uint8_t primary;
        uint8_t receiver;
        uint8_t ordinary_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-23/6 pass setup rejected");
        }
        candidate_foundation = scene->live_foundation;
        primary = candidate_foundation.primary_actor;
        receiver = candidate_foundation.candidate_actor_by_side[
            candidate_foundation.offense_side];
        for (int scan = 9; scan >= 0; --scan) {
            if ((uint8_t)scan != primary &&
                (uint8_t)scan != candidate_foundation.defender_actor &&
                (uint8_t)scan != receiver) {
                ordinary_actor = (uint8_t)scan;
                break;
            }
        }
        if (primary != scene->ball_holder || receiver >= 10U ||
            receiver == primary || ordinary_actor >= 10U ||
            scene->actors[receiver].team != scene->state.possession) {
            LIVE_FAIL("LIVE opcode-23/6 pass actors unavailable");
        }
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.play_state.actor_state[actor] = 0x06U;
            candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
            candidate_foundation.deferred[actor] = false;
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        candidate_foundation.play_state.actor_state[primary] = 0x04U;
        candidate_foundation.play_state.wait_counter[primary] = 0U;
        candidate_foundation.play_state.action_state_046e[primary] = 0U;
        candidate_foundation.play_state.stream_offset[primary] = 0x017CU;
        candidate_foundation.last_step_offset[primary] = 0x017CU;
        /* Opcode 5 owns direction/action, not the target planes. Seed a
           deliberately noncoherent but valid prior target and prove that its
           raw storage survives without remaining an active movement claim. */
        candidate_foundation.play_state.target_object[primary] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        candidate_foundation.play_state.target_x[primary] = 133;
        candidate_foundation.play_state.target_depth[primary] = 150;
        candidate_foundation.source_target_valid[primary] = true;
        candidate_foundation.source_raw_target_valid[primary] = false;
        candidate_foundation.source_inactive_target_storage[primary] = false;
        candidate_foundation.play_state.actor_state[ordinary_actor] = 0x04U;
        candidate_foundation.play_state.wait_counter[ordinary_actor] = 0U;
        candidate_foundation.play_state.stream_offset[ordinary_actor] =
            0x013BU;
        candidate_foundation.last_step_offset[ordinary_actor] = 0x013BU;
        scene->live_foundation = candidate_foundation;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0181U ||
            scene->live_foundation.play_state.actor_state[primary] != 0x04U ||
            scene->live_foundation.play_state.action_state_046e[primary] !=
                0x18U ||
            !scene->live_foundation.source_direction_valid[primary] ||
            scene->live_foundation.source_direction[primary] != 2U ||
            scene->live_foundation.source_target_valid[primary] ||
            !scene->live_foundation
                 .source_inactive_target_storage[primary] ||
            scene->live_foundation.play_state.target_x[primary] != 133 ||
            scene->live_foundation.play_state.target_depth[primary] != 150 ||
            !scene->cpu_actors[primary].target_valid ||
            scene->cpu_actors[primary].direction != 2U ||
            (scene->cpu_actors[primary].target_position.x == 133 &&
             scene->cpu_actors[primary].target_position.y == 150) ||
            scene_pass_active(scene)) {
            LIVE_FAIL("LIVE opcode-5 pass-stream entry failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0186U ||
            scene->live_foundation.play_state.actor_state[primary] != 0x04U ||
            scene->live_foundation.play_state.action_state_046e[primary] !=
                0x19U || scene_pass_active(scene)) {
            LIVE_FAIL("LIVE opcode-9 pass-stream cadence failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x018BU ||
            scene->live_foundation.play_state.actor_state[primary] != 0x06U ||
            scene->live_foundation.play_state.wait_counter[primary] != 6U ||
            scene_pass_active(scene)) {
            LIVE_FAIL("LIVE opcode-3 pass-stream wait seed failed");
        }
        for (uint8_t wait = 5U;; --wait) {
            memset(&no_shot, 0, sizeof(no_shot));
            if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
                scene->live_foundation.play_state.stream_offset[primary] !=
                    0x018BU ||
                scene->live_foundation.play_state.wait_counter[primary] !=
                    wait ||
                scene->live_foundation.play_state.actor_state[primary] !=
                    (wait == 0U ? 0x04U : 0x06U) ||
                scene_pass_active(scene)) {
                LIVE_FAIL("LIVE pass-stream wait cadence failed");
            }
            if (wait == 0U) break;
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0190U ||
            scene->live_foundation.play_state.action_state_046e[primary] !=
                0x19U || scene_pass_active(scene)) {
            LIVE_FAIL("LIVE opcode-23 predecessor timing failed");
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0190U ||
            scene->live_foundation.play_state.actor_state[primary] != 0x04U ||
            scene->live_foundation.play_state.action_state_046e[primary] !=
                0x10U || scene_pass_active(scene) ||
            !scene->live_foundation.deferred[ordinary_actor] ||
            scene->live_foundation.deferred_reason[ordinary_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE ||
            scene->live_foundation.play_state.stream_offset[ordinary_actor] !=
                0x013BU) {
            LIVE_FAIL("LIVE opcode-6 pending pass timing failed");
        }
        {
            TecmoGameplayScene failed = *scene;
            TecmoGameplayScene before_failed;
            failed.actors[receiver].team =
                (uint8_t)(1U - failed.state.possession);
            before_failed = failed;
            memset(&no_shot, 0, sizeof(no_shot));
            if (scene_update_ai(&failed, &no_shot) ||
                memcmp(&failed, &before_failed, sizeof(failed)) != 0) {
                LIVE_FAIL("LIVE opcode-6 malformed receiver was not atomic");
            }
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_GATHER ||
            scene->pass_state.packed_animation_state != 0x32U ||
            scene->pass_state.passer != primary ||
            scene->pass_state.receiver != receiver ||
            scene->pass_state.controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->live_foundation.play_state
                    .action_state_046e[primary] != 0x0FU ||
            scene->live_foundation.play_state.actor_state[receiver] != 0x0CU ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0190U) {
            LIVE_FAIL("LIVE opcode-6 automatic pass gather failed");
        }

        /* The same parked record is unavailable when the possessing side has
           a human controller; it cannot manufacture an automatic pending. */
        if (!tecmo_gameplay_scene_launch(scene, &bound) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-6 human exclusion setup rejected");
        }
        primary = scene->live_foundation.primary_actor;
        scene->live_foundation.play_state.actor_state[primary] = 0x04U;
        scene->live_foundation.play_state.wait_counter[primary] = 0U;
        scene->live_foundation.play_state.action_state_046e[primary] = 0U;
        scene->live_foundation.play_state.stream_offset[primary] = 0x0190U;
        scene->live_foundation.last_step_offset[primary] = 0x0190U;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            scene->live_foundation.play_state.stream_offset[primary] !=
                0x0190U ||
            scene->live_foundation.play_state.action_state_046e[primary] ==
                0x10U || scene_pass_active(scene)) {
            LIVE_FAIL("LIVE opcode-6 human controller was admitted");
        }
    }

    /* Exact ordinary-LIVE opcode-10 ownership. Preview TGBC with the current
       ball snapshot: the stored state is deliberately zero at the boundary,
       yet the selector must see the just-established frontcourt bit without
       mutating or adjudicating the authoritative state. */
    {
        TecmoGameplayCourtCoordinate boundary_ball;
        TecmoGameplayCpuSteeringPlayInput selector_play_input;
        TecmoGameplaySceneOpcode10FrameContext opcode10_context;
        TecmoGameplaySceneOpcode10Projection opcode10_projection;
        TecmoGameplayCpuSteeringPlayResult opcode10_play_result;
        TecmoGameplaySceneOpcode10FrameContext bound_before;
        TecmoGameplayScene single_use_scene;
        uint8_t candidate_timer;
        uint8_t rate;
        broken = *scene;
        broken.backcourt_state.frontcourt_established = 0U;
        boundary_ball.x = broken.orientation_state.attack_direction == 0U
            ? (int16_t)(broken.backcourt_assets
                            .orientation_zero_frontcourt_x - 16U)
            : (int16_t)(broken.backcourt_assets
                            .orientation_one_frontcourt_x + 16U);
        boundary_ball.y = 100;
        if (!tecmo_gameplay_court_coordinate_to_q8(
                &boundary_ball, &broken.ball_position)) {
            LIVE_FAIL("LIVE opcode-10 boundary Q8 setup rejected");
        }
        candidate_foundation = broken.live_foundation;
        candidate_foundation.primary_actor = 0U;
        candidate_foundation.defender_actor = 8U;
        memset(candidate_foundation.actor_selector_flags, 0,
               sizeof(candidate_foundation.actor_selector_flags));
        for (actor = 0U; actor < 10U; ++actor) {
            positions[actor].x = boundary_ball.x;
            positions[actor].y = 100;
            candidate_foundation.dynamic_link[actor] =
                actor < 5U ? (uint8_t)(actor + 5U)
                           : (uint8_t)(actor - 5U);
        }
        positions[5U].x = (int16_t)(boundary_ball.x + 40);
        positions[7U].x = (int16_t)(boundary_ball.x + 20);
        candidate_foundation.actor_selector_flags[5U] = 0x10U;
        candidate_foundation.actor_selector_flags[7U] = 0x10U;
        memset(&selector_play_input, 0, sizeof(selector_play_input));
        selector_play_input.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
        selector_play_input.orientation_035a =
            broken.orientation_state.attack_direction;
        memcpy(selector_play_input.actor_position, positions,
               sizeof(selector_play_input.actor_position));
        if (!scene_cpu_opcode10_selector_project(
                &broken, positions, &candidate_foundation,
                &selector_play_input) ||
            broken.backcourt_state.frontcourt_established != 0U ||
            !selector_play_input.special_actor_07df_available ||
            selector_play_input.special_actor_07df != 7U ||
            !selector_play_input.linked_actor_branch_context_available) {
            LIVE_FAIL("LIVE opcode-10 current-boundary selector regressed");
        }

        /* Actor 6 resolves through fixed `$06CB[6]=1` and therefore
           owns the non-primary workspace. The exceptional actor 7 resolves
           to primary 0; `$07DF` is dynamic and distinct from `$06CB`. */
        memset(&opcode10_context, 0, sizeof(opcode10_context));
        if (!scene_cpu_opcode10_workspace_project(
                6U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection) ||
            !selector_play_input.linked_actor_resolved_valid ||
            selector_play_input.linked_actor != 1U ||
            !selector_play_input.linked_relative_valid) {
            LIVE_FAIL("LIVE opcode-10 non-primary workspace regressed");
        }
        if (!scene_cpu_opcode10_workspace_project(
                7U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection) ||
            !selector_play_input.linked_actor_resolved_valid ||
            selector_play_input.linked_actor != 0U ||
            selector_play_input.linked_relative_valid) {
            LIVE_FAIL("LIVE opcode-10 primary workspace did not defer");
        }
        /* A valid runtime frame context enables the primary link. Projection
           is non-mutating; only a fetched, nondeferred opcode-10 result may
           serially commit the pending timer. */
        memset(&opcode10_context, 0, sizeof(opcode10_context));
        opcode10_context.contract_tag =
            TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG;
        opcode10_context.available = true;
        opcode10_context.sample_6a = 1U;
        opcode10_context.timer_0798 = 2U;
        opcode10_context.rate_index_075f = 2U;
        if (!tecmo_gameplay_scene_bind_opcode10_frame_context(
                &broken, &opcode10_context)) {
            LIVE_FAIL("LIVE opcode-10 valid frame bind rejected");
        }
        bound_before = broken.opcode10_frame_context;
        opcode10_context.sample_6a = 0U;
        if (tecmo_gameplay_scene_bind_opcode10_frame_context(
                &broken, &opcode10_context) ||
            memcmp(&broken.opcode10_frame_context, &bound_before,
                   sizeof(bound_before)) != 0) {
            LIVE_FAIL("LIVE opcode-10 malformed bind was not transactional");
        }
        single_use_scene = broken;
        if (!tecmo_gameplay_scene_update(&single_use_scene, NULL, NULL) ||
            single_use_scene.opcode10_frame_context.available) {
            LIVE_FAIL("LIVE opcode-10 frame context was not single-use");
        }
        opcode10_context = bound_before;
        if (!scene_cpu_opcode10_workspace_project(
                7U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection) ||
            !selector_play_input.linked_relative_valid ||
            !opcode10_projection.primary_timer_pending ||
            opcode10_projection.timer_before != 2U ||
            opcode10_projection.timer_after != 1U ||
            opcode10_context.timer_0798 != 2U) {
            LIVE_FAIL("LIVE opcode-10 primary timer projection regressed");
        }
        memset(&opcode10_play_result, 0, sizeof(opcode10_play_result));
        opcode10_play_result.fetched = true;
        opcode10_play_result.command.opcode = 9U;
        candidate_timer = 2U;
        if (!scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &opcode10_play_result,
                &candidate_timer) || candidate_timer != 2U) {
            LIVE_FAIL("LIVE opcode-10 non-command changed timer");
        }
        opcode10_play_result.command.opcode = 10U;
        if (!scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &opcode10_play_result,
                &candidate_timer) || candidate_timer != 1U) {
            LIVE_FAIL("LIVE opcode-10 first serial timer commit regressed");
        }
        opcode10_context.timer_0798 = candidate_timer;
        if (!scene_cpu_opcode10_workspace_project(
                7U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection) ||
            opcode10_projection.timer_before != 1U ||
            opcode10_projection.timer_after != 0U ||
            !scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &opcode10_play_result,
                &candidate_timer) || candidate_timer != 0U) {
            LIVE_FAIL("LIVE opcode-10 second serial timer commit regressed");
        }
        /* Threshold/reload vectors for all launch rate indices. Rate 0 can
           never reload with the proven nonzero sample. Rates 1 and 2 use
           their exact reload constants when sample <= threshold. */
        for (rate = 0U; rate < 3U; ++rate) {
            opcode10_context.timer_0798 = 0U;
            opcode10_context.rate_index_075f = rate;
            opcode10_context.sample_6a = rate == 0U ? 1U : rate;
            opcode10_context.timer_bias_0760 = rate == 2U ? 2U : 0U;
            if (!scene_cpu_opcode10_workspace_project(
                    7U, &candidate_foundation, &opcode10_context,
                    &selector_play_input, &opcode10_projection) ||
                !selector_play_input.linked_relative_valid ||
                opcode10_projection.timer_after !=
                    (rate == 0U ? 0U :
                     (rate == 1U ? 0x1DU : 0x33U))) {
                LIVE_FAIL("LIVE opcode-10 launch-rate branch regressed");
            }
        }
        opcode10_context.timer_0798 = 0U;
        opcode10_context.rate_index_075f = 1U;
        opcode10_context.sample_6a = 2U;
        opcode10_context.timer_bias_0760 = 0U;
        if (!scene_cpu_opcode10_workspace_project(
                7U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection) ||
            opcode10_projection.timer_after != 0U) {
            LIVE_FAIL("LIVE opcode-10 rate threshold bypass regressed");
        }
        opcode10_context.timer_0798 = 2U;
        opcode10_context.rate_index_075f = 2U;
        opcode10_context.sample_6a = 1U;
        if (!scene_cpu_opcode10_workspace_project(
                7U, &candidate_foundation, &opcode10_context,
                &selector_play_input, &opcode10_projection)) {
            LIVE_FAIL("LIVE opcode-10 deferred commit setup rejected");
        }
        opcode10_play_result.deferred = true;
        candidate_timer = 2U;
        if (!scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &opcode10_play_result,
                &candidate_timer) || candidate_timer != 2U) {
            LIVE_FAIL("LIVE opcode-10 deferred command changed timer");
        }

        /* With only the initial actor qualified, $BFD1 takes its no-store
           return. The artificial prior sentinel must never become a LIVE
           owner. */
        candidate_foundation.actor_selector_flags[7U] = 0U;
        if (!scene_cpu_opcode10_selector_project(
                &broken, positions, &candidate_foundation,
                &selector_play_input) ||
            selector_play_input.special_actor_07df_available ||
            selector_play_input.linked_actor_branch_context_available) {
            LIVE_FAIL("LIVE opcode-10 retained selector was projected");
        }

        /* Before frontcourt, the sole bit-$10 producer is clear and Bank02
           explicitly stores $FF without reading candidate coordinates. */
        positions[broken.ball_holder].x =
            broken.orientation_state.attack_direction == 0U
                ? (int16_t)(broken.backcourt_assets
                                .orientation_zero_frontcourt_x + 16U)
                : (int16_t)(broken.backcourt_assets
                                .orientation_one_frontcourt_x - 16U);
        if (!scene_cpu_opcode10_selector_project(
                &broken, positions, &candidate_foundation,
                &selector_play_input) ||
            !selector_play_input.special_actor_07df_available ||
            selector_play_input.special_actor_07df !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
            !selector_play_input.linked_actor_branch_context_available) {
            LIVE_FAIL("LIVE opcode-10 explicit-$FF projection regressed");
        }
    }

    /* End-to-end order regression: move the controlled holder across the
       frontcourt boundary while retaining the old scene ball Q8, exactly as
       scene_update_live_action_ordered does before scene_update_ai. The LIVE
       selector must follow the new held-ball/dribble projection, not that
       stale stored coordinate, and the subsequent AI pass must remain valid. */
    {
        TecmoControlFrame crossing_controls;
        TecmoGameplayCourtCoordinateQ8 stale_ball;
        TecmoGameplayBallDribbleFrame current_ball;
        TecmoGameplayCpuSteeringPlayInput selector_play_input;
        TecmoGameplaySceneCpuShotRequest no_shot;
        uint8_t holder;
        size_t holder_controller = TECMO_GAMEPLAY_CONTROLLER_COUNT;
        uint8_t initial_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t candidate_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint16_t opcode10_offset = 0U;
        bool found_opcode10 = false;
        int scan;
        if (!tecmo_gameplay_scene_launch(scene, &bound) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-10 controlled crossing launch rejected");
        }
        holder = scene->ball_holder;
        for (actor = 0U; actor < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++actor) {
            if (scene->controlled_actor[actor] == holder) {
                holder_controller = actor;
                break;
            }
        }
        if (holder_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT) {
            LIVE_FAIL("LIVE opcode-10 controlled holder was not routed");
        }
        scene->backcourt_state.frontcourt_established = 0U;
        scene->actors[holder].position.x =
            scene->orientation_state.attack_direction == 0U
                ? (int16_t)scene->backcourt_assets
                      .orientation_zero_frontcourt_x
                : (int16_t)(scene->backcourt_assets
                                .orientation_one_frontcourt_x - 1U);
        scene->actors[holder].position.y = 100;
        scene->actors[holder].anchor = scene->actors[holder].position;
        if (!scene_ball_position_for_actors(
                scene, scene->actors, holder, &scene->ball_position)) {
            LIVE_FAIL("LIVE opcode-10 stale held-ball setup rejected");
        }
        stale_ball = scene->ball_position;
        memset(&crossing_controls, 0, sizeof(crossing_controls));
        if (scene->orientation_state.attack_direction == 0U) {
            crossing_controls.held.left = true;
        } else {
            crossing_controls.held.right = true;
        }
        for (scan = 0; scan < 64; ++scan) {
            if (!scene_move_controlled_actor(
                    scene, holder_controller, &crossing_controls) ||
                !scene_live_ball_frame_for_actors(
                    scene, scene->actors, holder, &current_ball)) {
                LIVE_FAIL("LIVE opcode-10 controlled crossing move rejected");
            }
            if ((scene->orientation_state.attack_direction == 0U &&
                 current_ball.visible_position.x <
                     (int16_t)scene->backcourt_assets
                         .orientation_zero_frontcourt_x) ||
                (scene->orientation_state.attack_direction == 1U &&
                 current_ball.visible_position.x >=
                     (int16_t)scene->backcourt_assets
                         .orientation_one_frontcourt_x)) {
                break;
            }
        }
        if (scan == 64 ||
            scene->ball_position.x_q8 != stale_ball.x_q8 ||
            scene->ball_position.y_q8 != stale_ball.y_q8) {
            LIVE_FAIL("LIVE opcode-10 crossing did not retain stale scene ball");
        }
        for (actor = 0U; actor < 10U; ++actor) {
            positions[actor] = scene->actors[actor].position;
        }
        candidate_foundation = scene->live_foundation;
        candidate_foundation.primary_actor = holder;
        candidate_foundation.defender_actor = (uint8_t)((holder + 1U) % 10U);
        memset(candidate_foundation.actor_selector_flags, 0,
               sizeof(candidate_foundation.actor_selector_flags));
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.dynamic_link[actor] =
                (uint8_t)((holder + 2U) % 10U);
        }
        for (scan = 9; scan >= 0; --scan) {
            if ((uint8_t)scan != holder &&
                (uint8_t)scan != candidate_foundation.defender_actor &&
                (uint8_t)scan != scene->cpu_actors[holder].linked_actor) {
                if (initial_actor == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
                    initial_actor = (uint8_t)scan;
                } else {
                    candidate_actor = (uint8_t)scan;
                    break;
                }
            }
        }
        if (initial_actor == TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            candidate_actor == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
            LIVE_FAIL("LIVE opcode-10 crossing selector actors unavailable");
        }
        candidate_foundation.actor_selector_flags[initial_actor] = 0x10U;
        candidate_foundation.dynamic_link[initial_actor] = holder;
        candidate_foundation.actor_selector_flags[candidate_actor] = 0x10U;
        positions[initial_actor].x = (int16_t)(
            positions[holder].x +
            (positions[holder].x <=
                    TECMO_GAMEPLAY_COURT_WORLD_MAX_X - 40
                ? 40 : -40));
        positions[initial_actor].y = positions[holder].y;
        positions[candidate_actor].x = (int16_t)(
            positions[holder].x +
            (positions[holder].x <=
                    TECMO_GAMEPLAY_COURT_WORLD_MAX_X - 20
                ? 20 : -20));
        positions[candidate_actor].y = positions[holder].y;
        memset(&selector_play_input, 0, sizeof(selector_play_input));
        selector_play_input.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
        selector_play_input.orientation_035a =
            scene->orientation_state.attack_direction;
        memcpy(selector_play_input.actor_position, positions,
               sizeof(selector_play_input.actor_position));
        if (!scene_cpu_opcode10_selector_project(
                scene, positions, &candidate_foundation,
                &selector_play_input) ||
            !selector_play_input.special_actor_07df_available ||
            selector_play_input.special_actor_07df != candidate_actor ||
            selector_play_input.ball_position.x !=
                current_ball.visible_position.x ||
            selector_play_input.ball_position.y !=
                current_ball.visible_position.y) {
            LIVE_FAIL("LIVE opcode-10 crossing used stale ball projection");
        }

        /* Production proof: park the canonical opcode-10 record on the
           actual selector candidate whose fixed `$06CB` link resolves primary.
           The real scene dispatcher must fetch it, publish its source target,
           compose that target through TGMO, and consume exactly one pending
           timer result. */
        {
            TecmoGameplayScene opcode_scene = *scene;
            TecmoGameplayScene opcode_before;
            TecmoGameplayScene absent_scene;
            TecmoGameplayScene failed_scene;
            TecmoGameplayScene failed_before;
            TecmoGameplaySceneOpcode10FrameContext frame_context;
            TecmoGameplaySceneActor actor_before;
            TecmoGameplaySceneCpuShotRequest opcode_shot;
            uint8_t primary;
            uint8_t initial = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            uint8_t selected = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            uint8_t late_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            int16_t spacing;

            for (offset = 0U;
                 offset < scene->cpu_steering_assets.command_record_count * 5U;
                 offset = (uint16_t)(offset + 5U)) {
                if (tecmo_gameplay_cpu_steering_decode_command(
                        &scene->cpu_steering_assets, offset, &command) &&
                    command.opcode == 10U) {
                    opcode10_offset = offset;
                    found_opcode10 = true;
                    break;
                }
            }
            primary = opcode_scene.ball_holder;
            for (actor = 0U; actor < 10U; ++actor) {
                if (opcode_scene.actors[actor].team !=
                        opcode_scene.state.possession &&
                    exact_links[actor] == primary) {
                    initial = (uint8_t)actor;
                    break;
                }
            }
            for (scan = 9; scan >= 0; --scan) {
                if ((uint8_t)scan != initial &&
                    (uint8_t)scan !=
                        opcode_scene.live_foundation.defender_actor &&
                    opcode_scene.actors[scan].team !=
                        opcode_scene.state.possession) {
                    selected = (uint8_t)scan;
                    break;
                }
            }
            spacing = opcode_scene.actors[primary].position.x <=
                    TECMO_GAMEPLAY_COURT_WORLD_MAX_X - 40
                ? 1 : -1;
            if (!found_opcode10 || initial >= 10U || selected >= 10U) {
                LIVE_FAIL("LIVE canonical opcode-10 fixture unavailable");
            }
            for (actor = 0U; actor < 10U; ++actor) {
                if (opcode_scene.actors[actor].team !=
                        opcode_scene.state.possession) {
                    opcode_scene.actors[actor].position.y = 220;
                    opcode_scene.actors[actor].anchor =
                        opcode_scene.actors[actor].position;
                }
            }
            opcode_scene.actors[initial].position.x = (int16_t)(
                opcode_scene.actors[primary].position.x + spacing * 40);
            opcode_scene.actors[initial].position.y =
                opcode_scene.actors[primary].position.y;
            opcode_scene.actors[initial].anchor =
                opcode_scene.actors[initial].position;
            opcode_scene.actors[selected].position.x = (int16_t)(
                opcode_scene.actors[primary].position.x + spacing * 20);
            opcode_scene.actors[selected].position.y =
                opcode_scene.actors[primary].position.y;
            opcode_scene.actors[selected].anchor =
                opcode_scene.actors[selected].position;
            if (!scene_sync_live_foundation(&opcode_scene)) {
                LIVE_FAIL("LIVE canonical opcode-10 foundation sync rejected");
            }
            if (selected == opcode_scene.live_foundation.defender_actor) {
                opcode_scene.actors[selected].position.y = 220;
                opcode_scene.actors[selected].anchor =
                    opcode_scene.actors[selected].position;
                selected = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                for (scan = 9; scan >= 0; --scan) {
                    if ((uint8_t)scan != initial &&
                        (uint8_t)scan !=
                            opcode_scene.live_foundation.defender_actor &&
                        opcode_scene.actors[scan].team !=
                            opcode_scene.state.possession) {
                        selected = (uint8_t)scan;
                        break;
                    }
                }
                if (selected >= 10U) {
                    LIVE_FAIL("LIVE opcode-10 ordinary selector unavailable");
                }
                opcode_scene.actors[selected].position.x = (int16_t)(
                    opcode_scene.actors[primary].position.x + spacing * 20);
                opcode_scene.actors[selected].position.y =
                    opcode_scene.actors[primary].position.y;
                opcode_scene.actors[selected].anchor =
                    opcode_scene.actors[selected].position;
                if (!scene_sync_live_foundation(&opcode_scene)) {
                    LIVE_FAIL("LIVE opcode-10 revised selector sync rejected");
                }
            }
            candidate_foundation = opcode_scene.live_foundation;
            /* This fixture isolates ordinary Bank06 command dispatch. The
               separately covered selected-defender handoff would otherwise
               take exclusive target ownership before the opcode-10 actor. */
            candidate_foundation.selected_defender_handoff_active = false;
            for (actor = 0U; actor < 10U; ++actor) {
                candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
                candidate_foundation.play_state.actor_state[actor] = 0x06U;
                candidate_foundation.play_state.target_object[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
                candidate_foundation.play_state.target_x[actor] = 0;
                candidate_foundation.play_state.target_depth[actor] = 0;
                candidate_foundation.source_target_valid[actor] = false;
                candidate_foundation.source_direction_valid[actor] = false;
                candidate_foundation.deferred[actor] = false;
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
                memset(&candidate_foundation.play_state.route_motion[actor],
                       0, sizeof(candidate_foundation.play_state
                                     .route_motion[actor]));
                candidate_foundation.play_state.route_motion[actor]
                    .contract_tag =
                    TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
            }
            candidate_foundation.play_state.wait_counter[selected] = 0U;
            candidate_foundation.play_state.actor_state[selected] = 0x04U;
            candidate_foundation.play_state.stream_offset[selected] =
                opcode10_offset;
            candidate_foundation.last_step_offset[selected] = opcode10_offset;
            if (!tecmo_gameplay_live_foundation_valid(
                    &opcode_scene.cpu_steering_assets,
                    &candidate_foundation)) {
                LIVE_FAIL("LIVE canonical opcode-10 foundation rejected");
            }
            opcode_scene.live_foundation = candidate_foundation;
            memset(&frame_context, 0, sizeof(frame_context));
            frame_context.contract_tag =
                TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG;
            frame_context.available = true;
            frame_context.sample_6a = 1U;
            frame_context.timer_0798 = 2U;
            frame_context.rate_index_075f = 2U;
            opcode_scene.opcode10_frame_context = frame_context;
            actor_before = opcode_scene.actors[selected];
            opcode_before = opcode_scene;

            absent_scene = opcode_before;
            absent_scene.opcode10_frame_context.available = false;
            memset(&opcode_shot, 0, sizeof(opcode_shot));
            if (!scene_update_ai(&absent_scene, &opcode_shot) ||
                !absent_scene.live_foundation.deferred[selected] ||
                absent_scene.live_foundation.deferred_reason[selected] !=
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE ||
                absent_scene.live_foundation.source_target_valid[selected] ||
                absent_scene.opcode10_frame_context.timer_0798 != 2U) {
                LIVE_FAIL("LIVE opcode-10 absent frame context did not defer");
            }

            memset(&opcode_shot, 0, sizeof(opcode_shot));
            if (!scene_update_ai(&opcode_scene, &opcode_shot) ||
                opcode_shot.requested ||
                opcode_scene.live_foundation.deferred[selected] ||
                !opcode_scene.live_foundation.source_target_valid[selected] ||
                opcode_scene.live_foundation.last_effect[selected] !=
                    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_PROXIMITY ||
                opcode_scene.live_foundation.play_state
                        .target_object[selected] != primary ||
                !opcode_scene.cpu_actors[selected].target_valid ||
                opcode_scene.cpu_actors[selected].target_position.x !=
                    opcode_scene.actors[primary].position.x ||
                opcode_scene.cpu_actors[selected].target_position.y !=
                    opcode_scene.actors[primary].position.y ||
                opcode_scene.opcode10_frame_context.timer_0798 != 1U ||
                (opcode_scene.actors[selected].position.x ==
                     actor_before.position.x &&
                 opcode_scene.actors[selected].position.y ==
                     actor_before.position.y &&
                 opcode_scene.actors[selected].movement_direction ==
                     actor_before.movement_direction)) {
                (void)snprintf(message, message_size,
                    "LIVE opcode10 step failed sel=%u req=%u def=%u reason=%u src=%u eff=%u obj=%u primary=%u cpu=%u tx=%d/%d/px%d ty=%d/%d/py%d timer=%u pos=%d,%d->%d,%d dir=%u->%u",
                    (unsigned)selected, (unsigned)opcode_shot.requested,
                    (unsigned)opcode_scene.live_foundation.deferred[selected],
                    (unsigned)opcode_scene.live_foundation
                        .deferred_reason[selected],
                    (unsigned)opcode_scene.live_foundation
                        .source_target_valid[selected],
                    (unsigned)opcode_scene.live_foundation.last_effect[selected],
                    (unsigned)opcode_scene.live_foundation.play_state
                        .target_object[selected], (unsigned)primary,
                    (unsigned)opcode_scene.cpu_actors[selected].target_valid,
                    opcode_scene.cpu_actors[selected].target_position.x,
                    opcode_scene.live_foundation.play_state.target_x[selected],
                    opcode_scene.actors[primary].position.x,
                    opcode_scene.cpu_actors[selected].target_position.y,
                    opcode_scene.live_foundation.play_state
                        .target_depth[selected],
                    opcode_scene.actors[primary].position.y,
                    (unsigned)opcode_scene.opcode10_frame_context.timer_0798,
                    actor_before.position.x, actor_before.position.y,
                    opcode_scene.actors[selected].position.x,
                    opcode_scene.actors[selected].position.y,
                    (unsigned)actor_before.movement_direction,
                    (unsigned)opcode_scene.actors[selected]
                        .movement_direction);
                tecmo_gameplay_scene_test_set_skip_pretip(false);
                return false;
            }

            /* Reuse the complete pre-dispatch fixture. A lower actor is
               visited after the opcode-10 candidate in descending order and
               fails only at its explicit serial guard, after the command and
               timer result were staged. */
            failed_scene = opcode_before;
            for (scan = (int)selected - 1; scan >= 0; --scan) {
                if ((uint8_t)scan != primary &&
                    (uint8_t)scan != failed_scene.live_foundation.defender_actor) {
                    late_actor = (uint8_t)scan;
                    break;
                }
            }
            if (late_actor >= 10U) {
                LIVE_FAIL("LIVE opcode-10 late rollback actor unavailable");
            }
            failed_scene.cpu_actors[late_actor].decision_serial = UINT32_MAX;
            failed_before = failed_scene;
            memset(&opcode_shot, 0, sizeof(opcode_shot));
            if (scene_update_ai(&failed_scene, &opcode_shot) ||
                memcmp(&failed_scene, &failed_before,
                       sizeof(failed_scene)) != 0) {
                LIVE_FAIL("LIVE opcode-10 late failure did not roll back");
            }
        }
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot)) {
            LIVE_FAIL("LIVE opcode-10 post-crossing scene AI rejected");
        }
        /* Restore the CPU-only fixture expected by the production opcode-4
           proof below. */
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-10 post-crossing fixture restore rejected");
        }
    }

    /* Bounded LIVE opcode-12 proof. No imported formation owns `$006E`, so
       this explicitly parks the sole canonical record while retaining the
       real scene selector/workspace, automatic-offense admission, descending
       traversal, foundation publication, TGMO composition, and timer commit. */
    {
        static TecmoGameplayScene opcode12_scene;
        static TecmoGameplayScene absent_scene;
        static TecmoGameplayScene failed_scene;
        static TecmoGameplayScene failed_before;
        TecmoGameplayLiveFoundation opcode12_foundation;
        TecmoGameplaySceneOpcode10FrameContext frame_context;
        TecmoGameplaySceneCpuShotRequest no_shot;
        TecmoGameplaySceneActor actor_before;
        TecmoGameplayCourtCoordinate backcourt_ball;
        uint8_t target_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t late_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t primary;
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-12 bounded scene setup rejected");
        }
        primary = scene->live_foundation.primary_actor;
        for (int scan = 9; scan >= 0; --scan) {
            uint8_t item = (uint8_t)scan;
            if (item != primary &&
                item != scene->live_foundation.defender_actor &&
                scene->live_foundation.actor_team[item] ==
                    scene->live_foundation.offense_side) {
                target_actor = item;
                break;
            }
        }
        if (target_actor >= 10U ||
            scene->live_foundation.control_mode[
                scene->live_foundation.offense_side] == 0U) {
            LIVE_FAIL("LIVE opcode-12 automatic offense actor unavailable");
        }
        backcourt_ball.x = scene->orientation_state.attack_direction == 0U
            ? (int16_t)(scene->backcourt_assets
                            .orientation_zero_frontcourt_x + 16U)
            : (int16_t)(scene->backcourt_assets
                            .orientation_one_frontcourt_x - 16U);
        backcourt_ball.y = scene->actors[primary].position.y;
        scene->actors[primary].position = backcourt_ball;
        scene->actors[primary].anchor = backcourt_ball;
        scene->backcourt_state.frontcourt_established = 0U;
        if (!scene_attach_ball(scene) || !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-12 explicit-FF selector setup rejected");
        }
        opcode12_foundation = scene->live_foundation;
        for (actor = 0U; actor < 10U; ++actor) {
            opcode12_foundation.play_state.actor_state[actor] = 0x06U;
            opcode12_foundation.play_state.wait_counter[actor] = 0xFFU;
            opcode12_foundation.play_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            opcode12_foundation.play_state.target_x[actor] = 0;
            opcode12_foundation.play_state.target_depth[actor] = 0;
            opcode12_foundation.play_state.direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            opcode12_foundation.source_target_valid[actor] = false;
            opcode12_foundation.source_raw_target_valid[actor] = false;
            opcode12_foundation.source_inactive_target_storage[actor] = false;
            opcode12_foundation.source_direction_valid[actor] = false;
            opcode12_foundation.source_direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            opcode12_foundation.deferred[actor] = false;
            opcode12_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            memset(&opcode12_foundation.play_state.route_motion[actor], 0,
                   sizeof(opcode12_foundation.play_state
                              .route_motion[actor]));
            opcode12_foundation.play_state.route_motion[actor].contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
        }
        opcode12_foundation.play_state.actor_state[target_actor] = 0x04U;
        opcode12_foundation.play_state.wait_counter[target_actor] = 0U;
        opcode12_foundation.play_state.stream_offset[target_actor] = 0x006EU;
        opcode12_foundation.last_step_offset[target_actor] = 0x006EU;
        opcode12_foundation.dynamic_link[target_actor] = primary;
        scene->actors[target_actor].position.x = (int16_t)(
            scene->actors[primary].position.x +
            (scene->actors[primary].position.x < 384 ? 80 : -80));
        scene->actors[target_actor].position.y = (int16_t)(
            scene->actors[primary].position.y < 160
                ? scene->actors[primary].position.y + 40
                : scene->actors[primary].position.y - 40);
        scene->actors[target_actor].anchor =
            scene->actors[target_actor].position;
        opcode12_foundation.actor_position[target_actor] =
            scene->actors[target_actor].position;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &opcode12_foundation)) {
            LIVE_FAIL("LIVE opcode-12 bounded foundation rejected");
        }
        scene->live_foundation = opcode12_foundation;
        memset(&frame_context, 0, sizeof(frame_context));
        frame_context.contract_tag =
            TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG;
        frame_context.available = true;
        frame_context.sample_6a = 1U;
        frame_context.timer_0798 = 2U;
        frame_context.rate_index_075f = 2U;
        if (!tecmo_gameplay_scene_bind_opcode10_frame_context(
                scene, &frame_context)) {
            LIVE_FAIL("LIVE opcode-12 frame context bind rejected");
        }
        opcode12_scene = *scene;
        absent_scene = opcode12_scene;
        absent_scene.opcode10_frame_context.available = false;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(&absent_scene, &no_shot) ||
            !absent_scene.live_foundation.deferred[target_actor] ||
            absent_scene.live_foundation.deferred_reason[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE ||
            absent_scene.live_foundation.play_state
                    .stream_offset[target_actor] != 0x006EU ||
            absent_scene.opcode10_frame_context.timer_0798 != 2U) {
            LIVE_FAIL("LIVE opcode-12 missing workspace did not defer");
        }
        actor_before = opcode12_scene.actors[target_actor];
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(&opcode12_scene, &no_shot) ||
            opcode12_scene.live_foundation.deferred[target_actor] ||
            !opcode12_scene.live_foundation.source_target_valid[target_actor] ||
            opcode12_scene.live_foundation.play_state
                    .stream_offset[target_actor] != 0x0078U ||
            opcode12_scene.live_foundation.play_state
                    .target_object[target_actor] != primary ||
            !opcode12_scene.cpu_actors[target_actor].target_valid ||
            opcode12_scene.opcode10_frame_context.timer_0798 != 1U ||
            (opcode12_scene.actors[target_actor].position.x ==
                 actor_before.position.x &&
             opcode12_scene.actors[target_actor].position.y ==
                 actor_before.position.y &&
             opcode12_scene.actors[target_actor].movement_direction ==
                 actor_before.movement_direction)) {
            LIVE_FAIL("LIVE opcode-12 exact scene execution failed");
        }
        failed_scene = *scene;
        for (int scan = (int)target_actor - 1; scan >= 0; --scan) {
            if ((uint8_t)scan != primary &&
                (uint8_t)scan != failed_scene.live_foundation.defender_actor) {
                late_actor = (uint8_t)scan;
                break;
            }
        }
        if (late_actor >= 10U) {
            LIVE_FAIL("LIVE opcode-12 rollback actor unavailable");
        }
        failed_scene.cpu_actors[late_actor].decision_serial = UINT32_MAX;
        failed_before = failed_scene;
        memset(&no_shot, 0, sizeof(no_shot));
        if (scene_update_ai(&failed_scene, &no_shot) ||
            memcmp(&failed_scene, &failed_before, sizeof(failed_scene)) != 0) {
            LIVE_FAIL("LIVE opcode-12 late failure did not roll back");
        }
    }

    /* Production opcode-13 boundary: scene_cpu_build_play_input has no owner
       for persistent $038D-$0390 and must not substitute the current ball.
       Park the exact record on an eligible ordinary actor and observe the
       typed missing-global-target defer with no stream/target mutation. */
    {
        TecmoGameplaySceneCpuShotRequest no_shot;
        TecmoGameplaySceneA023LatchFrameContext latch_context;
        TecmoGameplayCourtCoordinate opcode21_positions[
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
        uint8_t opcode21_flags = 0U;
        uint8_t target_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t unrelated_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t late_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-13 production fixture setup rejected");
        }
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            opcode21_positions[actor] = scene->actors[actor].position;
        }
        /* Fixed $F07E-$F0B9 exact bit-1 boundary vectors. */
        broken = *scene;
        broken.orientation_state.attack_direction = 0U;
        opcode21_positions[broken.live_foundation.primary_actor].x = 0x00F7;
        opcode21_positions[broken.live_foundation.primary_actor].y = 0x007B;
        if (!scene_cpu_opcode21_flags_007e(
                &broken, opcode21_positions, &broken.live_foundation,
                &opcode21_flags) || opcode21_flags != 0x02U) {
            LIVE_FAIL("LIVE opcode-21 orientation-0 set boundary failed");
        }
        opcode21_positions[broken.live_foundation.primary_actor].x = 0x00F8;
        if (!scene_cpu_opcode21_flags_007e(
                &broken, opcode21_positions, &broken.live_foundation,
                &opcode21_flags) || opcode21_flags != 0U) {
            LIVE_FAIL("LIVE opcode-21 orientation-0 clear boundary failed");
        }
        broken.orientation_state.attack_direction = 1U;
        opcode21_positions[broken.live_foundation.primary_actor].x = 0x0208;
        opcode21_positions[broken.live_foundation.primary_actor].y = 0x00AE;
        if (!scene_cpu_opcode21_flags_007e(
                &broken, opcode21_positions, &broken.live_foundation,
                &opcode21_flags) || opcode21_flags != 0x02U) {
            LIVE_FAIL("LIVE opcode-21 orientation-1 set boundary failed");
        }
        opcode21_positions[broken.live_foundation.primary_actor].x = 0x0207;
        if (!scene_cpu_opcode21_flags_007e(
                &broken, opcode21_positions, &broken.live_foundation,
                &opcode21_flags) || opcode21_flags != 0U) {
            LIVE_FAIL("LIVE opcode-21 orientation-1 clear boundary failed");
        }
        opcode21_positions[broken.live_foundation.primary_actor].x = 0x0208;
        opcode21_positions[broken.live_foundation.primary_actor].y = 0x00AF;
        if (!scene_cpu_opcode21_flags_007e(
                &broken, opcode21_positions, &broken.live_foundation,
                &opcode21_flags) || opcode21_flags != 0U) {
            LIVE_FAIL("LIVE opcode-21 depth clear boundary failed");
        }
        candidate_foundation = scene->live_foundation;
        for (int scan = 9; scan >= 0; --scan) {
            if ((uint8_t)scan != candidate_foundation.primary_actor &&
                (uint8_t)scan != candidate_foundation.defender_actor) {
                target_actor = (uint8_t)scan;
                break;
            }
        }
        if (target_actor >= 10U) {
            LIVE_FAIL("LIVE opcode-13 ordinary actor unavailable");
        }
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
            candidate_foundation.play_state.actor_state[actor] = 0x06U;
            candidate_foundation.play_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            candidate_foundation.play_state.target_x[actor] = 0;
            candidate_foundation.play_state.target_depth[actor] = 0;
            candidate_foundation.source_target_valid[actor] = false;
            candidate_foundation.deferred[actor] = false;
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            memset(&candidate_foundation.play_state.route_motion[actor], 0,
                   sizeof(candidate_foundation.play_state
                              .route_motion[actor]));
            candidate_foundation.play_state.route_motion[actor].contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
        }
        candidate_foundation.play_state.wait_counter[target_actor] = 0U;
        candidate_foundation.play_state.actor_state[target_actor] = 0x04U;
        candidate_foundation.play_state.stream_offset[target_actor] = 0x002DU;
        candidate_foundation.last_step_offset[target_actor] = 0x002DU;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            LIVE_FAIL("LIVE opcode-13 production foundation rejected");
        }
        scene->live_foundation = candidate_foundation;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            !scene->live_foundation.deferred[target_actor] ||
            scene->live_foundation.deferred_reason[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET ||
            scene->live_foundation.play_state.stream_offset[target_actor] !=
                0x002DU ||
            scene->live_foundation.source_target_valid[target_actor] ||
            scene->live_foundation.source_raw_target_valid[target_actor] ||
            scene->cpu_actors[target_actor].target_valid) {
            LIVE_FAIL("LIVE opcode-13 production input was fabricated");
        }
        /* Opcode 20 shares only the unavailable raw latch. Production must
           not substitute the current held/loose ball coordinate for it. */
        scene->live_foundation.play_state.stream_offset[target_actor] =
            0x000FU;
        scene->live_foundation.last_step_offset[target_actor] = 0x000FU;
        scene->live_foundation.deferred[target_actor] = false;
        scene->live_foundation.deferred_reason[target_actor] =
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(scene, &no_shot) || no_shot.requested ||
            !scene->live_foundation.deferred[target_actor] ||
            scene->live_foundation.deferred_reason[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET ||
            scene->live_foundation.play_state.stream_offset[target_actor] !=
                0x000FU ||
            scene->live_foundation.source_target_valid[target_actor] ||
            scene->live_foundation.source_raw_target_valid[target_actor] ||
            scene->cpu_actors[target_actor].target_valid) {
            LIVE_FAIL("LIVE opcode-20 production input was fabricated");
        }

        /* Intentional exact-event attachment fixture. Production owns no
           A214 caller yet, so construct only the already-proven B721 result:
           one raw depth byte, exact producer provenance, and the actor mask
           written to `$0019` by that same assignment. */
        scene->live_foundation.play_state.stream_offset[target_actor] =
            0x0019U;
        scene->live_foundation.last_step_offset[target_actor] = 0x0019U;
        scene->live_foundation.deferred[target_actor] = false;
        scene->live_foundation.deferred_reason[target_actor] =
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        memset(&latch_context, 0, sizeof(latch_context));
        latch_context.contract_tag =
            TECMO_GAMEPLAY_SCENE_A023_LATCH_FRAME_CONTEXT_TAG;
        latch_context.available = true;
        latch_context.latch.contract_tag =
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_TAG;
        latch_context.latch.valid = true;
        latch_context.latch.producer_kind =
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721;
        latch_context.latch.target.x = (uint16_t)(
            scene->actors[target_actor].position.x + 24);
        latch_context.latch.target.depth = (uint16_t)(uint8_t)
            scene->actors[target_actor].position.y;
        latch_context.latch.immediate_opcode20_actor_mask =
            (uint16_t)(1U << target_actor);
        broken = *scene;
        if (!tecmo_gameplay_scene_bind_a023_latch_frame_context(
                &broken, &latch_context) ||
            !scene_update_ai(&broken, &no_shot) ||
            broken.live_foundation.deferred[target_actor] ||
            broken.live_foundation.play_state
                    .stream_offset[target_actor] != 0x001EU ||
            broken.live_foundation.source_target_valid[target_actor] ||
            broken.live_foundation.source_raw_target_valid[target_actor] ||
            !broken.live_foundation
                 .source_inactive_target_storage[target_actor] ||
            broken.a023_latch_frame_context.available) {
            LIVE_FAIL("LIVE same-frame B721 opcode-20 consume failed");
        }
        /* The same exact offset on an actor absent from the assignment mask
           is cursor coincidence, not authorization. */
        for (actor = 0U; actor < 10U; ++actor) {
            if (actor != target_actor &&
                actor != scene->live_foundation.primary_actor &&
                actor != scene->live_foundation.defender_actor) {
                unrelated_actor = (uint8_t)actor;
                break;
            }
        }
        if (unrelated_actor >= 10U) {
            LIVE_FAIL("LIVE same-frame latch unrelated actor unavailable");
        }
        broken = *scene;
        broken.live_foundation.play_state
            .stream_offset[unrelated_actor] = 0x0019U;
        broken.live_foundation.last_step_offset[unrelated_actor] =
            0x0019U;
        broken.live_foundation.play_state.actor_state[unrelated_actor] =
            0x04U;
        broken.live_foundation.play_state.wait_counter[unrelated_actor] = 0U;
        broken.live_foundation.play_state.actor_state[target_actor] =
            0x06U;
        broken.live_foundation.play_state.wait_counter[target_actor] =
            0xFFU;
        if (!tecmo_gameplay_scene_bind_a023_latch_frame_context(
                &broken, &latch_context) ||
            !scene_update_ai(&broken, &no_shot) ||
            !broken.live_foundation.deferred[unrelated_actor] ||
            broken.live_foundation.deferred_reason[unrelated_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET ||
            broken.live_foundation.play_state
                    .stream_offset[unrelated_actor] != 0x0019U ||
            broken.a023_latch_frame_context.available) {
            LIVE_FAIL("LIVE same-frame latch cursor coincidence admitted");
        }
        broken = *scene;
        broken.live_foundation.play_state.stream_offset[target_actor] =
            0x000AU;
        broken.live_foundation.last_step_offset[target_actor] = 0x000AU;
        if (!tecmo_gameplay_scene_bind_a023_latch_frame_context(
                &broken, &latch_context) ||
            !scene_update_ai(&broken, &no_shot) ||
            broken.live_foundation.play_state
                    .stream_offset[target_actor] != 0x000FU ||
            broken.a023_latch_frame_context.available) {
            LIVE_FAIL("LIVE same-frame latch leaked into delayed opcode-20");
        }
        broken = *scene;
        broken.live_foundation.play_state
            .stream_offset[target_actor] = 0x002DU;
        broken.live_foundation.last_step_offset[target_actor] = 0x002DU;
        if (!tecmo_gameplay_scene_bind_a023_latch_frame_context(
                &broken, &latch_context) ||
            !scene_update_ai(&broken, &no_shot) ||
            !broken.live_foundation.deferred[target_actor] ||
            broken.live_foundation.play_state
                    .stream_offset[target_actor] != 0x002DU) {
            LIVE_FAIL("LIVE same-frame latch leaked into opcode-13");
        }
        broken = *scene;
        for (actor = 0U; actor < target_actor; ++actor) {
            if (actor != broken.live_foundation.primary_actor &&
                actor != broken.live_foundation.defender_actor) {
                late_actor = (uint8_t)actor;
                break;
            }
        }
        if (late_actor >= 10U) {
            LIVE_FAIL("LIVE same-frame latch rollback actor unavailable");
        }
        broken.cpu_actors[late_actor].decision_serial = UINT32_MAX;
        if (!tecmo_gameplay_scene_bind_a023_latch_frame_context(
                &broken, &latch_context)) {
            LIVE_FAIL("LIVE same-frame latch rollback bind rejected");
        }
        snapshot = broken;
        if (scene_update_ai(&broken, &no_shot) ||
            memcmp(&broken, &snapshot, sizeof(broken)) != 0) {
            LIVE_FAIL("LIVE same-frame latch late failure did not roll back");
        }
        latch_context.latch.immediate_opcode20_actor_mask = 0U;
        snapshot = *scene;
        if (tecmo_gameplay_scene_bind_a023_latch_frame_context(
                scene, &latch_context) ||
            memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
            LIVE_FAIL("LIVE malformed same-frame latch bind mutated scene");
        }
    }

    /* Bank05 $81F2 opcode-16 producer: capture primary position before one
       controlled move, then prove both canonical records share that immutable
       workspace in the later Bank06 descending traversal. */
    {
        TecmoGameplaySceneOpcode16FrameContext pre_context;
        TecmoGameplaySceneOpcode16FrameContext post_context;
        TecmoGameplayScene opcode16_scene;
        TecmoGameplayScene absent_scene;
        TecmoGameplayScene malformed_context_scene;
        TecmoGameplayScene malformed_before;
        TecmoGameplayLiveFoundation opcode16_foundation;
        TecmoGameplaySceneCpuShotRequest no_shot;
        TecmoControlFrame move_controls;
        uint16_t opcode16_offsets[2U] = {0U, 0U};
        uint8_t opcode16_actors[2U] = {
            TECMO_GAMEPLAY_SCENE_NO_ACTOR,
            TECMO_GAMEPLAY_SCENE_NO_ACTOR
        };
        uint8_t primary;
        uint8_t defender;
        size_t opcode16_count = 0U;
        size_t controller = TECMO_GAMEPLAY_CONTROLLER_COUNT;
        int16_t hoop_x;
        int16_t pre_x;
        int16_t expected_target_x;
        int16_t expected_target_y;

        for (offset = 0U;
             offset < scene->cpu_steering_assets.command_record_count * 5U;
             offset = (uint16_t)(offset + 5U)) {
            if (!tecmo_gameplay_cpu_steering_decode_command(
                    &scene->cpu_steering_assets, offset, &command)) {
                continue;
            }
            if (command.opcode == 16U && command.arguments[0U] == 0x09U &&
                command.arguments[1U] == 0x03U && opcode16_count < 2U) {
                opcode16_offsets[opcode16_count++] = offset;
            }
        }
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (opcode16_count != 2U ||
            opcode16_offsets[0U] != 0x0D07U ||
            opcode16_offsets[1U] != 0x0D16U ||
            !tecmo_gameplay_scene_launch(scene, &bound) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-16 canonical record fixture rejected");
        }
        primary = scene->live_foundation.primary_actor;
        defender = scene->live_foundation.defender_actor;
        for (actor = 0U; actor < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++actor) {
            if (scene->controlled_actor[actor] == primary) {
                controller = actor;
                break;
            }
        }
        for (int scan = 9; scan >= 0 && opcode16_count > 0U; --scan) {
            uint8_t item = (uint8_t)scan;
            if (item == primary || item == defender ||
                item == scene->controlled_actor[0U] ||
                item == scene->controlled_actor[1U]) {
                continue;
            }
            if (opcode16_actors[0U] == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
                opcode16_actors[0U] = item;
            } else {
                opcode16_actors[1U] = item;
                break;
            }
        }
        if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT || primary >= 10U ||
            defender >= 10U || opcode16_actors[0U] >= 10U ||
            opcode16_actors[1U] >= 10U) {
            LIVE_FAIL("LIVE opcode-16 actor routing unavailable");
        }
        hoop_x = scene->orientation_state.attack_direction == 0U
            ? TECMO_GAMEPLAY_COURT_LEFT_HOOP_X
            : TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X;
        scene->actors[primary].position.x = (int16_t)(hoop_x +
            (scene->orientation_state.attack_direction == 0U ? 48 : -48));
        scene->actors[primary].position.y = 100;
        scene->actors[primary].anchor = scene->actors[primary].position;
        scene->actors[primary].movement_fractional_accumulator = 15U;
        scene->actors[primary].movement_action_state =
            scene->orientation_state.attack_direction == 0U
                ? TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT
                : TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
        if (!scene_attach_ball(scene) || !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-16 comparison-boundary setup rejected");
        }
        opcode16_foundation = scene->live_foundation;
        for (actor = 0U; actor < 10U; ++actor) {
            opcode16_foundation.play_state.wait_counter[actor] = 0xFFU;
            opcode16_foundation.play_state.actor_state[actor] = 0x06U;
            opcode16_foundation.play_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            opcode16_foundation.play_state.target_x[actor] = 0;
            opcode16_foundation.play_state.target_depth[actor] = 0;
            opcode16_foundation.source_target_valid[actor] = false;
            opcode16_foundation.source_direction_valid[actor] = false;
            opcode16_foundation.deferred[actor] = false;
            opcode16_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            memset(&opcode16_foundation.play_state.route_motion[actor], 0,
                   sizeof(opcode16_foundation.play_state
                              .route_motion[actor]));
            opcode16_foundation.play_state.route_motion[actor].contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
        }
        for (actor = 0U; actor < 2U; ++actor) {
            uint8_t item = opcode16_actors[actor];
            opcode16_foundation.play_state.wait_counter[item] = 0U;
            opcode16_foundation.play_state.actor_state[item] = 0x04U;
            opcode16_foundation.play_state.stream_offset[item] =
                opcode16_offsets[actor];
            opcode16_foundation.last_step_offset[item] =
                opcode16_offsets[actor];
        }
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &opcode16_foundation)) {
            LIVE_FAIL("LIVE opcode-16 dual-record foundation rejected");
        }
        scene->live_foundation = opcode16_foundation;
        if (!scene_cpu_opcode16_workspace_capture(scene, &pre_context) ||
            pre_context.workspace_036e != 48U ||
            pre_context.workspace_0370 != 48U ||
            !tecmo_gameplay_scene_bind_opcode16_frame_context(
                scene, &pre_context)) {
            LIVE_FAIL("LIVE opcode-16 pre-motion capture rejected");
        }
        pre_x = scene->actors[primary].position.x;
        memset(&move_controls, 0, sizeof(move_controls));
        if (scene->orientation_state.attack_direction == 0U) {
            move_controls.held.left = true;
        } else {
            move_controls.held.right = true;
        }
        if (!scene_move_controlled_actor(scene, controller, &move_controls) ||
            scene->actors[primary].position.x == pre_x ||
            (uint16_t)(scene->actors[primary].position.x > hoop_x
                ? scene->actors[primary].position.x - hoop_x
                : hoop_x - scene->actors[primary].position.x) >= 48U) {
            LIVE_FAIL("LIVE opcode-16 primary did not cross comparison boundary");
        }
        opcode16_scene = *scene;
        expected_target_x = opcode16_scene.actors[defender].position.x;
        expected_target_y = (int16_t)(
            opcode16_scene.actors[defender].position.y >=
                    TECMO_GAMEPLAY_COURT_HOOP_Y
                ? opcode16_scene.actors[defender].position.y - 10
                : opcode16_scene.actors[defender].position.y + 10);

        absent_scene = opcode16_scene;
        absent_scene.opcode16_frame_context.available = false;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(&absent_scene, &no_shot)) {
            LIVE_FAIL("LIVE opcode-16 absent-context update rejected");
        }
        for (actor = 0U; actor < 2U; ++actor) {
            uint8_t item = opcode16_actors[actor];
            if (!absent_scene.live_foundation.deferred[item] ||
                absent_scene.live_foundation.deferred_reason[item] !=
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE ||
                absent_scene.live_foundation.source_target_valid[item]) {
                LIVE_FAIL("LIVE opcode-16 absent context did not defer");
            }
        }

        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(&opcode16_scene, &no_shot) ||
            no_shot.requested) {
            LIVE_FAIL("LIVE opcode-16 pre-motion scene dispatch rejected");
        }
        for (actor = 0U; actor < 2U; ++actor) {
            uint8_t item = opcode16_actors[actor];
            if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                    &opcode16_scene.cpu_steering_assets,
                    (int16_t)(scene->actors[defender].position.x -
                              scene->actors[item].position.x),
                    (int16_t)(scene->actors[defender].position.y -
                              scene->actors[item].position.y),
                    &expected_direction) ||
                opcode16_scene.live_foundation.deferred[item] ||
                !opcode16_scene.live_foundation.source_target_valid[item] ||
                opcode16_scene.live_foundation.last_effect[item] !=
                    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_POINTER_ACTOR_TARGET ||
                opcode16_scene.live_foundation.play_state
                        .target_object[item] != defender ||
                opcode16_scene.live_foundation.play_state.target_x[item] !=
                    expected_target_x ||
                opcode16_scene.live_foundation.play_state
                        .target_depth[item] != expected_target_y) {
                LIVE_FAIL("LIVE opcode-16 shared pre-motion workspace regressed");
            }
            if (!opcode16_scene.cpu_actors[item].target_valid ||
                opcode16_scene.cpu_actors[item].target_position.x !=
                    scene->actors[defender].position.x ||
                opcode16_scene.cpu_actors[item].target_position.y !=
                    scene->actors[defender].position.y ||
                opcode16_scene.cpu_actors[item].direction !=
                    expected_direction) {
                LIVE_FAIL("LIVE opcode-16 pre-motion TGMO direction regressed");
            }
        }
        if (opcode16_scene.live_foundation.source_target_valid[primary] ||
            opcode16_scene.live_foundation.source_target_valid[defender]) {
            LIVE_FAIL("LIVE opcode-16 primary/defender exclusion regressed");
        }

        malformed_context_scene = *scene;
        malformed_context_scene.opcode16_frame_context.workspace_036e ^= 1U;
        malformed_before = malformed_context_scene;
        memset(&no_shot, 0, sizeof(no_shot));
        if (scene_update_ai(&malformed_context_scene, &no_shot) ||
            memcmp(&malformed_context_scene, &malformed_before,
                   sizeof(malformed_context_scene)) != 0) {
            LIVE_FAIL("LIVE opcode-16 malformed context did not roll back");
        }

        /* Re-capturing after movement flips to the horizontal branch. This
           proves the preceding dual-record result came from the retained
           pre-motion snapshot rather than a current-position recompute. */
        opcode16_scene = *scene;
        opcode16_scene.live_foundation = opcode16_foundation;
        if (!scene_cpu_opcode16_workspace_capture(
                &opcode16_scene, &post_context) ||
            post_context.workspace_036e >= post_context.workspace_0370 ||
            !tecmo_gameplay_scene_bind_opcode16_frame_context(
                &opcode16_scene, &post_context)) {
            LIVE_FAIL("LIVE opcode-16 post-motion capture rejected");
        }
        expected_target_x = (int16_t)(
            opcode16_scene.actors[defender].position.x +
            (opcode16_scene.orientation_state.attack_direction == 0U
                ? 16 : -16));
        expected_target_y = opcode16_scene.actors[defender].position.y;
        memset(&no_shot, 0, sizeof(no_shot));
        if (!scene_update_ai(&opcode16_scene, &no_shot)) {
            LIVE_FAIL("LIVE opcode-16 horizontal scene dispatch rejected");
        }
        for (actor = 0U; actor < 2U; ++actor) {
            uint8_t item = opcode16_actors[actor];
            if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                    &opcode16_scene.cpu_steering_assets,
                    (int16_t)(scene->actors[defender].position.x -
                              scene->actors[item].position.x),
                    (int16_t)(scene->actors[defender].position.y -
                              scene->actors[item].position.y),
                    &expected_direction) ||
                !opcode16_scene.live_foundation.source_target_valid[item] ||
                opcode16_scene.live_foundation.play_state.target_x[item] !=
                    expected_target_x ||
                opcode16_scene.live_foundation.play_state
                        .target_depth[item] != expected_target_y ||
                !opcode16_scene.cpu_actors[item].target_valid ||
                opcode16_scene.cpu_actors[item].direction !=
                    expected_direction) {
                LIVE_FAIL("LIVE opcode-16 horizontal workspace branch regressed");
            }
        }

        if (!tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE opcode-16 fixture restore rejected");
        }
    }

    /* Production path proof for the strict Bank04 $9F2E canonical opcode-4
       record. The fixture only chooses an already imported record for an
       ordinary CPU player; scene_update_ai still builds its immutable player
       and Q8 ball snapshot, launches TGAI-3 state 5 without a TGMO move on
       the launch tick, then advances the frozen target on the next tick. */
    {
        const uint8_t target_actor = 1U;
        uint16_t opcode4_offset = 0U;
        uint16_t opcode0_offset = 0U;
        bool found_opcode4 = false;
        bool found_opcode0 = false;
        TecmoGameplayCourtCoordinate expected_ball;
        TecmoGameplaySceneActor route_start_actor;
        TecmoGameplayCpuSteeringRouteMotionState expected_route_state;
        TecmoGameplayCpuSteeringRouteStepResult expected_route_step;
        for (offset = 0U;
             offset < scene->cpu_steering_assets.command_record_count * 5U;
             offset = (uint16_t)(offset + 5U)) {
            if (!tecmo_gameplay_cpu_steering_decode_command(
                    &scene->cpu_steering_assets, offset, &command)) {
                continue;
            }
            if (command.opcode == 4U &&
                command.arguments[0U] ==
                    TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT) {
                opcode4_offset = offset;
                found_opcode4 = true;
            } else if (command.opcode == 0U && !found_opcode0) {
                opcode0_offset = offset;
                found_opcode0 = true;
            }
            if (found_opcode4 && found_opcode0) {
                break;
            }
        }
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!found_opcode4 || !found_opcode0 ||
            !tecmo_gameplay_scene_launch(scene, &cpu_only) ||
            !scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene) ||
            !scene_attach_ball(scene) ||
            !tecmo_gameplay_court_coordinate_q8_floor(
                &scene->ball_position, &expected_ball)) {
            LIVE_FAIL("LIVE opcode-4 ball target fixture setup rejected");
        }
        candidate_foundation = scene->live_foundation;
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.play_state.target_object[actor] =
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
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            if (actor != target_actor) {
                candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
                candidate_foundation.play_state.actor_state[actor] = 0x06U;
            }
        }
        candidate_foundation.play_state.wait_counter[target_actor] = 0U;
        candidate_foundation.play_state.actor_state[target_actor] = 0x04U;
        candidate_foundation.play_state.stream_offset[target_actor] =
            opcode4_offset;
        candidate_foundation.last_step_offset[target_actor] = opcode4_offset;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            LIVE_FAIL("LIVE opcode-4 ball target foundation rejected");
        }
        scene->live_foundation = candidate_foundation;
        route_start_actor = scene->actors[target_actor];
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) ||
            shot_request.requested || shot_request.playback_supported ||
            shot_request.deferred ||
            !scene->live_foundation.source_target_valid[target_actor] ||
            scene->live_foundation.deferred[target_actor] ||
            scene->live_foundation.play_state.target_object[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
            scene->live_foundation.play_state.target_x[target_actor] !=
                expected_ball.x ||
            scene->live_foundation.play_state.target_depth[target_actor] !=
                expected_ball.y ||
            scene->live_foundation.last_effect[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_TARGET ||
            scene->live_foundation.last_step_offset[target_actor] !=
                (uint16_t)(opcode4_offset + 5U) ||
            scene->live_foundation.play_state.actor_state[target_actor] !=
                0x05U ||
            !scene->live_foundation.play_state.route_motion[target_actor]
                .active ||
            scene->live_foundation.play_state.route_motion[target_actor]
                .remaining_timer == 0U ||
            scene->actors[target_actor].position.x !=
                route_start_actor.position.x ||
            scene->actors[target_actor].position.y !=
                route_start_actor.position.y ||
            scene->actors[target_actor].movement_action_state !=
                route_start_actor.movement_action_state ||
            scene->actors[target_actor].movement_direction !=
                route_start_actor.movement_direction ||
            !scene->cpu_actors[target_actor].target_valid ||
            scene->cpu_actors[target_actor].target_kind !=
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET ||
            scene->cpu_actors[target_actor].target_position.x !=
                expected_ball.x ||
            scene->cpu_actors[target_actor].target_position.y !=
                expected_ball.y ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE canonical opcode-4 C8 route was not launched");
        }
        if (!tecmo_gameplay_cpu_steering_route_step(
                target_actor,
                (uint8_t)(scene->state.clock_divider & 1U),
                &scene->live_foundation.play_state.route_motion[target_actor],
                &expected_route_state, &expected_route_step) ||
            !scene_update_ai(scene, &shot_request) ||
            scene->actors[target_actor].position.x !=
                (int16_t)expected_route_step.horizontal_position ||
            scene->actors[target_actor].position.y !=
                (int16_t)expected_route_step.depth_position ||
            memcmp(&scene->live_foundation.play_state
                        .route_motion[target_actor],
                   &expected_route_state, sizeof(expected_route_state)) != 0 ||
            scene->live_foundation.play_state.target_x[target_actor] !=
                expected_ball.x ||
            scene->live_foundation.play_state.target_depth[target_actor] !=
                expected_ball.y ||
            scene->cpu_actors[target_actor].target_position.x !=
                expected_ball.x ||
            scene->cpu_actors[target_actor].target_position.y !=
                expected_ball.y) {
            LIVE_FAIL("LIVE opcode-4 state-5 route step/frozen target regressed");
        }

        /* The outer scene frame decrements/reloads the typed $0359 clock
           divider before Bank06 AI. Starting at 2 must therefore feed bit 1
           to a low actor's decrement-to-zero completion, not stale bit 0. */
        if (!scene_sync_live_foundation(scene)) {
            LIVE_FAIL("LIVE route clock-order fixture sync rejected");
        }
        broken = *scene;
        broken.state.clock_divider = 2U;
        broken.live_foundation.play_state.route_motion[target_actor]
            .remaining_timer = 1U;
        if (!tecmo_gameplay_scene_update(
                &broken, &neutral_one, &neutral_two) ||
            broken.state.clock_divider != 1U ||
            broken.live_foundation.play_state.route_motion[target_actor]
                .active ||
            broken.live_foundation.play_state.actor_state[target_actor] !=
                0x04U) {
            LIVE_FAIL("LIVE $0359 pre-AI completion order regressed");
        }

        /* Raw state-5 publishes with no clamp. A Q6 step beyond the accepted
           court must reject the complete AI transaction without exposing the
           integrated actor, route timer, metadata, or stream changes. */
        broken = *scene;
        broken.actors[target_actor].position.x =
            TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
        scene_clamp_actor_world(&broken.actors[target_actor]);
        if (!scene_actor_world_position_valid(
                &broken.actors[target_actor])) {
            LIVE_FAIL("LIVE route edge fixture was not initially valid");
        }
        broken.live_foundation.actor_position[target_actor] =
            broken.actors[target_actor].position;
        broken.live_foundation.play_state.route_motion[target_actor]
            .horizontal_accumulator_q6 = (uint16_t)(
                (uint16_t)broken.actors[target_actor].position.x << 6U);
        broken.live_foundation.play_state.route_motion[target_actor]
            .horizontal_velocity_q6 = 64;
        broken.live_foundation.play_state.route_motion[target_actor]
            .depth_accumulator_q6 = (uint16_t)(
                (uint16_t)broken.actors[target_actor].position.y << 6U);
        broken.live_foundation.play_state.route_motion[target_actor]
            .depth_velocity_q6 = 0;
        broken.live_foundation.play_state.route_motion[target_actor]
            .remaining_timer = 2U;
        snapshot = broken;
        memset(&shot_request, 0, sizeof(shot_request));
        if (scene_update_ai(&broken, &shot_request) ||
            memcmp(&broken, &snapshot, sizeof(broken)) != 0) {
            LIVE_FAIL("LIVE out-of-court route rejection was not transactional");
        }

        /* Holder/orientation/controller/formation changes invalidate a
           command-derived target. The state-5/Q6 transaction must be
           cancelled with that frozen target so it cannot resume later. */
        candidate_foundation = scene->live_foundation;
        for (actor = 0U; actor < 10U; ++actor) {
            positions[actor] = scene->actors[actor].position;
            actor_team[actor] = scene->actors[actor].team;
        }
        if (!tecmo_gameplay_live_foundation_synchronize(
                &scene->cpu_steering_assets, positions,
                (uint8_t)(scene->orientation_state.attack_direction ^ 1U),
                (uint8_t)scene->state.possession, scene->ball_holder,
                actor_team, scene->launch.controller_team,
                scene->controlled_actor, &candidate_foundation) ||
            candidate_foundation.play_state.route_motion[target_actor]
                .active ||
            candidate_foundation.play_state.actor_state[target_actor] !=
                0x04U ||
            candidate_foundation.source_target_valid[target_actor] ||
            candidate_foundation.play_state.target_object[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
            candidate_foundation.play_state.target_x[target_actor] != 0 ||
            candidate_foundation.play_state.target_depth[target_actor] != 0 ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            LIVE_FAIL("LIVE role transition retained stale route state");
        }

        /* A route-owning actor can become $0309 through a handoff. Bank06's
           ordinary loop excludes that selected defender, so LIVE must cancel
           the stale route and let the holder +/-16 adapter own this tick. */
        {
            uint8_t defender = scene->live_foundation.defender_actor;
            TecmoGameplayCourtCoordinate defender_before;
            TecmoGameplayCourtCoordinate defender_target;
            if (defender < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
                defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                !scene->live_foundation.selected_defender_handoff_active) {
                LIVE_FAIL("LIVE selected-defender route fixture lacked ownership");
            }
            candidate_foundation = scene->live_foundation;
            for (actor = 0U; actor < 10U; ++actor) {
                memset(&candidate_foundation.play_state.route_motion[actor],
                       0, sizeof(candidate_foundation.play_state
                                     .route_motion[actor]));
                candidate_foundation.play_state.route_motion[actor]
                    .contract_tag =
                        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
                candidate_foundation.play_state.actor_state[actor] = 0x06U;
                candidate_foundation.play_state.wait_counter[actor] = 0xFFU;
                candidate_foundation.play_state.target_object[actor] =
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
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            }
            defender_before = scene->actors[defender].position;
            defender_target = scene->actors[scene->ball_holder].position;
            defender_target.x = (int16_t)(defender_target.x +
                (scene->orientation_state.attack_direction == 0U
                    ? 16 : -16));
            candidate_foundation.play_state.actor_state[defender] = 0x05U;
            candidate_foundation.play_state.wait_counter[defender] = 0U;
            candidate_foundation.play_state.target_object[defender] =
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
            candidate_foundation.play_state.target_x[defender] =
                expected_ball.x;
            candidate_foundation.play_state.target_depth[defender] =
                expected_ball.y;
            candidate_foundation.source_target_valid[defender] = true;
            candidate_foundation.play_state.route_motion[defender]
                .horizontal_accumulator_q6 = (uint16_t)(
                    (uint16_t)defender_before.x << 6U);
            candidate_foundation.play_state.route_motion[defender]
                .depth_accumulator_q6 = (uint16_t)(
                    (uint16_t)defender_before.y << 6U);
            candidate_foundation.play_state.route_motion[defender]
                .horizontal_velocity_q6 = 4096;
            candidate_foundation.play_state.route_motion[defender]
                .remaining_timer = 3U;
            candidate_foundation.play_state.route_motion[defender].active =
                true;
            if (!tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &candidate_foundation)) {
                LIVE_FAIL("LIVE selected-defender stale-route fixture rejected");
            }
            scene->live_foundation = candidate_foundation;
            memset(&shot_request, 0, sizeof(shot_request));
            if (!scene_update_ai(scene, &shot_request) ||
                scene->live_foundation.play_state.route_motion[defender]
                    .active ||
                scene->live_foundation.play_state.actor_state[defender] !=
                    0x04U ||
                scene->live_foundation.source_target_valid[defender] ||
                scene->live_foundation.play_state.target_object[defender] !=
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
                scene->live_foundation.play_state.target_x[defender] != 0 ||
                scene->live_foundation.play_state.target_depth[defender] != 0 ||
                scene->actors[defender].position.x ==
                    (int16_t)(defender_before.x + 64) ||
                !scene->cpu_actors[defender].target_valid ||
                scene->cpu_actors[defender].target_position.x !=
                    defender_target.x ||
                scene->cpu_actors[defender].target_position.y !=
                    defender_target.y ||
                !tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &scene->live_foundation)) {
                LIVE_FAIL("LIVE selected defender resumed stale state-5 route");
            }
        }

        /* Ordinary LIVE owns only Bank06 $92CA's zero branch: Bank05
           $86DD-$8798 reserves nonzero low bits for transient shot/recovery,
           while $8FAD requires ($BA & 3)==0 for ordinary handoff. Install an
           imported opcode-0 record and prove the typed zero advances it;
           this is not a raw-$BA or frame-counter substitute. */
        candidate_foundation = scene->live_foundation;
        candidate_foundation.play_state.route_motion[target_actor].active =
            false;
        candidate_foundation.play_state.actor_state[target_actor] = 0x04U;
        for (actor = 0U; actor < 10U; ++actor) {
            candidate_foundation.play_state.wait_counter[actor] =
                actor == target_actor ? 0U : 1U;
            candidate_foundation.deferred[actor] = false;
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        candidate_foundation.play_state.stream_offset[target_actor] =
            opcode0_offset;
        candidate_foundation.last_step_offset[target_actor] = opcode0_offset;
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate_foundation)) {
            LIVE_FAIL("LIVE opcode-0 typed-zero foundation rejected");
        }
        scene->live_foundation = candidate_foundation;
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) ||
            scene->live_foundation.deferred[target_actor] ||
            scene->live_foundation.deferred_reason[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
            !scene->live_foundation.source_target_valid[target_actor] ||
            scene->live_foundation.last_effect[target_actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_RELATIVE_TARGET ||
            scene->live_foundation.play_state.stream_offset[target_actor] !=
                (uint16_t)(opcode0_offset + 5U) ||
            scene->live_foundation.last_step_offset[target_actor] !=
                (uint16_t)(opcode0_offset + 5U) ||
            !tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)) {
            LIVE_FAIL("LIVE production builder did not advance typed-zero $BA");
        }
    }

    /* Bound LIVE still owns the human TGMO path. Exercise its one-update
       latency, offensive A pass, maintained defensive candidate switch, and a swapped
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
            !scene_pass_active(scene) || scene->ball_holder != 0U ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[1U].team != TECMO_GAMEPLAY_TEAM_AWAY) {
            LIVE_FAIL("LIVE bound human offensive A pass failed");
        }
        for (size_t pass_guard = 0U;
             scene_pass_active(scene) && pass_guard < 40U; ++pass_guard) {
            memset(&human_p1, 0, sizeof(human_p1));
            if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2))
                LIVE_FAIL("LIVE bound human pass update failed");
        }
        if (scene_pass_active(scene) || scene->ball_holder != 1U ||
            scene->controlled_actor[0U] != 1U)
            LIVE_FAIL("LIVE bound human pass catch failed");
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
        switched_actor = scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.defense_side];
        memset(&human_p1, 0, sizeof(human_p1));
        human_p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->controlled_actor[0U] != switched_actor ||
            scene->actors[scene->controlled_actor[0U]].team !=
                TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->controlled_actor[1U] != 5U) {
            LIVE_FAIL("LIVE bound human defensive A candidate-switch failed");
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
            !scene_pass_active(scene) || scene->ball_holder != 0U ||
            scene->controlled_actor[1U] != 0U) {
            LIVE_FAIL("LIVE swapped controller offensive A pass failed");
        }
        for (size_t pass_guard = 0U;
             scene_pass_active(scene) && pass_guard < 40U; ++pass_guard) {
            memset(&human_p2, 0, sizeof(human_p2));
            if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2))
                LIVE_FAIL("LIVE swapped controller pass update failed");
        }
        if (scene_pass_active(scene) || scene->ball_holder != 1U ||
            scene->controlled_actor[1U] != 1U)
            LIVE_FAIL("LIVE swapped controller pass catch failed");
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
        switched_actor = scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.defense_side];
        memset(&human_p2, 0, sizeof(human_p2));
        human_p2.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &human_p1, &human_p2) ||
            scene->controlled_actor[1U] != switched_actor ||
            scene->actors[switched_actor].team != TECMO_GAMEPLAY_TEAM_AWAY) {
            LIVE_FAIL("LIVE swapped controller defensive candidate-switch failed");
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
            scene_test_enable_captured_play_inputs(&play_input);
            play_input.actor = edge_actor;
            play_input.step_budget = 1U;
            play_input.orientation_035a =
                scene->orientation_state.attack_direction;
            memcpy(play_input.actor_position, edge_positions,
                   sizeof(edge_positions));
            play_input.ball_position = edge_positions[0U];
            /* The accepted source executor currently leaves its direction
               sentinel false; this bounded fixture injects a validated
               direction metadata record and uses a known deferred source
               record to exercise only the owned TGMO composition branch. */
            candidate_foundation = scene->live_foundation;
            candidate_foundation.play_state.stream_offset[edge_actor] =
                deferred_offset;
            candidate_foundation.last_step_offset[edge_actor] =
                deferred_offset;
            candidate_foundation.play_state.target_object[edge_actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            candidate_foundation.play_state.target_x[edge_actor] = 0;
            candidate_foundation.play_state.target_depth[edge_actor] = 0;
            candidate_foundation.play_state.direction[edge_actor] =
                edge_direction;
            /* This fixture isolates the edge-direction inert adapter; the
               selected-defender spacing path is covered independently. */
            candidate_foundation.selected_defender_handoff_active = false;
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
                candidate_foundation.play_state.target_object[actor] =
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
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            }
            candidate_foundation.play_state.direction[edge_actor] =
                edge_direction;
            candidate_foundation.source_direction_valid[edge_actor] = true;
            candidate_foundation.source_direction[edge_actor] = edge_direction;
            candidate_foundation.selected_defender_handoff_active = false;
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
                scene->orientation_state.attack_direction;
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
                scene->orientation_state.attack_direction !=
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
                scene->live_foundation.play_state.target_object[edge_actor] !=
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
    candidate_foundation.play_state.target_object[1U] =
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
    candidate_foundation.play_state.target_object[0U] =
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
        candidate_foundation.play_state.target_object[far_holder] =
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
        if (!scene_update_ai(scene, &shot_request)) {
            LIVE_FAIL("LIVE supported close-shot scene_update_ai rejected");
        }
        if (!shot_request.requested ||
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
        (uint8_t)(scene->orientation_state.attack_direction ^ 1U);
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

static void scene_test_bind_live_foul_launch(
    TecmoGameplaySceneLaunch *launch)
{
    size_t side;
    size_t actor;
    if (launch == NULL) return;
    launch->starter_binding_bound = true;
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    for (side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
             ++actor) {
            launch->starter_roster_index[side][actor] = (uint8_t)actor;
        }
    }
}

/* The skip-pretip harness still preserves the transactional handoff frame.
 * Consume that frame before a test supplies B so the proof exercises the
 * ordinary LIVE action loop rather than a restart suppression boundary. */
static bool scene_test_live_foul_ready(TecmoGameplayScene *scene)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    return scene != NULL &&
           tecmo_gameplay_scene_update(scene, &p1, &p2) &&
           tecmo_gameplay_scene_update(scene, &p1, &p2) &&
           scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
           !tecmo_gameplay_scene_in_pretip(scene) &&
           !scene->legacy_direct_launch &&
           tecmo_gameplay_live_foundation_valid(
               &scene->cpu_steering_assets, &scene->live_foundation);
}

/* This is intentionally adjacent to the live bridge test rather than a
 * second classifier implementation.  It locks the strict TPNL source spans
 * and all classifier consequences that the scene bridge consumes: ordinary
 * defensive pushing, bonus attempts, the alternate blocking selector, and
 * offensive charging/turnover/counter behavior. */
static bool scene_test_live_foul_classifier_contract(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplayPenaltySourceSpan *commit_source;
    const TecmoGameplayPenaltySourceSpan *rules_source;
    TecmoGameplayPenaltyContext context;
    TecmoGameplayPenaltyResult result;

    if (scene == NULL || !scene->penalty_assets.available) return false;
    commit_source = tecmo_gameplay_penalties_find_source(
        &scene->penalty_assets, TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_COMMIT);
    rules_source = tecmo_gameplay_penalties_find_source(
        &scene->penalty_assets,
        TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_RULES_PRESENTATION);
    if (commit_source == NULL || commit_source->bank != 5U ||
        commit_source->fixed_bank || commit_source->cpu_start != 0x9571U ||
        commit_source->cpu_end != 0x9649U ||
        commit_source->byte_count != 217U ||
        commit_source->fingerprint != 0xC83877F7U ||
        rules_source == NULL || rules_source->bank != 2U ||
        rules_source->fixed_bank || rules_source->cpu_start != 0xB0F8U ||
        rules_source->cpu_end != 0xB398U ||
        rules_source->byte_count != 673U ||
        rules_source->fingerprint != 0xA06E397CU) {
        return false;
    }

    memset(&context, 0, sizeof(context));
    context.foul_actor = 5U;
    context.offensive_primary_actor = 0U;
    context.saved_route = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_SAVED_ROUTE;
    context.current_route = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CURRENT_ROUTE;
    context.contact_selector =
        TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CONTACT_SELECTOR;
    context.period_kind = TECMO_GAMEPLAY_PENALTY_PERIOD_REGULATION;
    if (!tecmo_gameplay_penalties_classify(
            &scene->penalty_assets, &context, &result) ||
        result.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        result.offensive_foul || result.turnover ||
        result.individual_foul_delta != 1U || result.team_foul_delta != 1U ||
        result.individual_fouls_after != 1U || result.team_fouls_after != 1U ||
        result.team_in_bonus || result.free_throw_attempts != 0U ||
        result.attempts_from_bonus) {
        return false;
    }
    context.individual_fouls = 5U;
    context.team_fouls = 4U;
    if (!tecmo_gameplay_penalties_classify(
            &scene->penalty_assets, &context, &result) ||
        result.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        result.individual_foul_delta != 1U || result.team_foul_delta != 1U ||
        result.individual_fouls_after != 6U || result.team_fouls_after != 5U ||
        !result.fouled_out || !result.team_in_bonus ||
        result.free_throw_attempts != 2U || !result.attempts_from_bonus) {
        return false;
    }
    context.individual_fouls = 0U;
    context.team_fouls = 0U;
    context.current_route = 0U;
    context.contact_selector = 4U;
    if (!tecmo_gameplay_penalties_classify(
            &scene->penalty_assets, &context, &result) ||
        result.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING ||
        result.offensive_foul || result.turnover ||
        result.individual_foul_delta != 1U || result.team_foul_delta != 1U ||
        result.free_throw_attempts != 0U) {
        return false;
    }
    context.foul_actor = context.offensive_primary_actor;
    context.current_route = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CURRENT_ROUTE;
    context.individual_fouls = 5U;
    context.team_fouls = 4U;
    if (!tecmo_gameplay_penalties_classify(
            &scene->penalty_assets, &context, &result) ||
        result.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_CHARGING ||
        !result.offensive_foul || !result.turnover ||
        result.individual_foul_delta != 1U || result.team_foul_delta != 0U ||
        result.individual_fouls_after != 6U || result.team_fouls_after != 4U ||
        !result.fouled_out || result.free_throw_attempts != 0U) {
        return false;
    }
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
        scene->audio_player.pending_sfx_id !=
            TECMO_GAMEPLAY_SFX_EXPIRY_ID ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation expiry entry failed");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != TECMO_GAMEPLAY_SFX_EXPIRY_ID) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "music-off violation expiry buzzer consumption failed");
        return false;
    }
    for (frame = 0U; frame < 15U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.phase_frame != (uint16_t)(frame + 1U) ||
            scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "music-off violation queued SFX6 before frame 16");
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene->state.phase_frame != 16U ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 6U) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "music-off violation did not queue SFX6 at frame 16");
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 6U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off violation cue was not consumed");
        return false;
    }
    for (frame = 16U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.phase_frame != (uint16_t)(frame + 1U) ||
            scene->audio_player.sfx_pending) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "music-off violation cue repeated after frame 16");
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->audio_player.sfx_pending ||
        scene->audio_player.music == NULL ||
        scene->audio_player.music->playing ||
        scene->audio_player.music->track_pending) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "music-off restart queued neutral cue");
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    launch.game_music_enabled = true;

    scene_test_bind_live_foul_launch(&launch);
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_sync_live_foundation(scene) ||
        !scene_test_live_foul_ready(scene)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "live B05 contact-boundary launch rejected");
        return false;
    }
    scene->actors[scene->controlled_actor[1]].position.x =
        scene->actors[scene->ball_holder].position.x + 8;
    scene->actors[scene->controlled_actor[1]].position.y =
        scene->actors[scene->ball_holder].position.y;
    scene->actors[scene->controlled_actor[1]].anchor =
        scene->actors[scene->controlled_actor[1]].position;
    if (!scene_sync_live_foundation(scene) || !scene_ownership_valid(scene)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "B05 $9968 no-contact fixture did not synchronize");
        return false;
    }
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->action_serial != 2U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        scene->state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME][
            scene->actors[scene->controlled_actor[1]].roster_index] != 0U ||
        scene->ball_holder != scene->live_foundation.primary_actor) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "B05 $9968 close-contact boundary was not fail-closed");
        return false;
    }
    for (uint8_t side = 0U;
         side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT; ++side)
        for (uint8_t roster = 0U;
             roster < TECMO_PLAYER_STATS_ROSTER_COUNT; ++roster)
            for (uint8_t counter = TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT;
                 counter < TECMO_PLAYER_STATS_COUNTER_DIMENSION; ++counter)
                if (scene->player_stats.counters[side][roster][counter] != 0U) {
                    tecmo_gameplay_scene_test_message(
                        message, message_size,
                        "no-contact defense emitted unsupported player stats");
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
    uint8_t away_shooter_roster;
    uint8_t home_defender_roster;
    size_t frame;
    scene_test_bind_live_foul_launch(&launch);
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_sync_live_foundation(scene) ||
        !scene_test_live_foul_ready(scene) ||
        !scene_test_free_throw_lineup_unbound(scene) ||
        !scene_test_live_foul_classifier_contract(scene)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "strict live-foul bridge/source contract rejected");
        return false;
    }
    if (scene->live_foundation.primary_actor != scene->ball_holder ||
        scene->live_foundation.defender_actor != scene->controlled_actor[1U]) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "live foul test lacked the $0308/$0309-shaped selected pair");
        return false;
    }
    home_defender_roster =
        scene->actors[scene->controlled_actor[1U]].roster_index;
    scene->actors[scene->controlled_actor[1]].position.x =
        scene->actors[scene->ball_holder].position.x + 1;
    scene->actors[scene->controlled_actor[1]].position.y =
        scene->actors[scene->ball_holder].position.y;
    scene->actors[scene->controlled_actor[1]].anchor =
        scene->actors[scene->controlled_actor[1]].position;
    if (!scene_sync_live_foundation(scene) || !scene_ownership_valid(scene)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "ordinary live-foul contact fixture did not synchronize");
        return false;
    }
    /* Ordinary Bank05 fall-through ($07E3=0, $0478=$19) does not select a
       free throw before the bonus threshold.  This is a real defensive-B
       contact path; only the counters are fixture setup. */
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene->state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME]
            [home_defender_roster] != 1U ||
        scene->state.free_throws.attempts_remaining != 0U ||
        scene->action_serial != 2U ||
        !scene->foul_presentation.valid ||
        scene->foul_presentation.fouling_team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->foul_presentation.actor_index != scene->controlled_actor[1U] ||
        scene->foul_presentation.roster_index != home_defender_roster ||
        scene->foul_presentation.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        scene->foul_presentation.individual_foul_delta != 1U ||
        scene->foul_presentation.team_foul_delta != 1U ||
        scene->foul_presentation.individual_fouls_after != 1U ||
        scene->foul_presentation.team_fouls_after != 1U ||
        scene->foul_presentation.free_throw_attempts != 0U ||
        scene->foul_presentation.team_in_bonus ||
        scene->foul_presentation.fouled_out) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "ordinary live pushing foul did not preserve no-bonus consequences");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "ordinary live foul did not reach release handoff");
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.free_throws.attempts_remaining != 0U ||
        scene->ball_holder != scene->controlled_actor[0U] ||
        scene->action_serial != 2U || scene->foul_presentation.valid) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "ordinary live foul did not hand possession back without attempts");
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    /* Re-enter through the same live contact action one foul below bonus.  The
       strict TPNL result, rather than a scene-owned fixed count, must now
       produce exactly two attempts. */
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !scene_sync_live_foundation(scene) ||
        !scene_test_live_foul_ready(scene) ||
        !scene_test_free_throw_lineup_unbound(scene) ||
        scene->live_foundation.primary_actor != scene->ball_holder ||
        scene->live_foundation.defender_actor != scene->controlled_actor[1U]) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "bonus live-foul bridge launch rejected");
        return false;
    }
    home_defender_roster =
        scene->actors[scene->controlled_actor[1U]].roster_index;
    scene->actors[scene->controlled_actor[1]].position.x =
        scene->actors[scene->ball_holder].position.x + 1;
    scene->actors[scene->controlled_actor[1]].position.y =
        scene->actors[scene->ball_holder].position.y;
    scene->actors[scene->controlled_actor[1]].anchor =
        scene->actors[scene->controlled_actor[1]].position;
    if (!scene_sync_live_foundation(scene) || !scene_ownership_valid(scene)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "bonus live-foul contact fixture did not synchronize");
        return false;
    }
    scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] = 4U;
    scene->action_serial = 1U;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 5U ||
        scene->state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME]
            [home_defender_roster] != 1U ||
        scene->state.free_throws.attempts_remaining != 2U ||
        scene->action_serial != 2U ||
        !scene->foul_presentation.valid ||
        scene->foul_presentation.fouling_team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->foul_presentation.actor_index != scene->controlled_actor[1U] ||
        scene->foul_presentation.roster_index != home_defender_roster ||
        scene->foul_presentation.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        scene->foul_presentation.individual_foul_delta != 1U ||
        scene->foul_presentation.team_foul_delta != 1U ||
        scene->foul_presentation.individual_fouls_after != 1U ||
        scene->foul_presentation.team_fouls_after != 5U ||
        scene->foul_presentation.free_throw_attempts != 2U ||
        !scene->foul_presentation.team_in_bonus ||
        scene->foul_presentation.fouled_out) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "bonus live pushing foul did not use TPNL counters/attempts");
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "bonus live foul did not retain presentation lead-in");
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
        scene->foul_presentation.valid ||
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
    away_shooter_roster = scene->actors[scene->free_throw_shooter].roster_index;
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
        scene->free_throw_frame != 0U || scene->action_serial != 2U ||
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
            scene->free_throw_frame != 0U || scene->action_serial != 2U ||
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
        scene->free_throw_frame != 0U || scene->action_serial != 3U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_AWAY][
            away_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTA] != 1U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_AWAY][
            away_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTM] != 1U ||
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
        scene->free_throw_frame != 0U || scene->action_serial != 3U ||
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
        scene->free_throw_frame != 0U || scene->action_serial != 4U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_AWAY][
            away_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTA] != 2U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_AWAY][
            away_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTM] != 2U ||
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
    uint8_t home_shooter_roster;
    uint8_t cpu_shooter_roster;
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
    home_shooter_roster = scene->actors[scene->free_throw_shooter].roster_index;
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
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            home_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTA] != 1U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            home_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTM] != 1U ||
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
    cpu_shooter_roster = scene->actors[scene->free_throw_shooter].roster_index;
    if (scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            cpu_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTA] != 1U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            cpu_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTM] != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "CPU missed free-throw stats attribution failed");
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
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            cpu_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTA] != 2U ||
        scene->player_stats.counters[TECMO_GAMEPLAY_TEAM_HOME][
            cpu_shooter_roster][TECMO_PLAYER_STATS_COUNTER_FTM] != 0U ||
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
    TecmoPlayerStatsGameLedger expected_stats;
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
    if (!tecmo_player_stats_record_shot_attempt(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, 2U) ||
        !tecmo_player_stats_record_shot_make(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, 2U) ||
        !tecmo_player_stats_record_shot_attempt(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, 3U) ||
        !tecmo_player_stats_record_shot_make(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, 3U) ||
        !tecmo_player_stats_record_free_throw(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, false) ||
        !tecmo_player_stats_record_free_throw(
            &scene->player_stats, TECMO_GAMEPLAY_TEAM_AWAY, 0U, true)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "final player-stats setup failed");
        return false;
    }
    expected_stats = scene->player_stats;
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
        result.away_score != 4U || result.home_score != 2U ||
        memcmp(&result.player_stats, &expected_stats,
               sizeof(expected_stats)) != 0 ||
        !tecmo_player_stats_game_ledger_valid(&result.player_stats)) {
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

static unsigned scene_test_prepare_failure_stage;

static bool scene_test_prepare_owned_shot_fixture(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    int approach_distance_x,
    int16_t y,
    uint32_t frame_seed)
{
    TecmoGameplaySceneLaunch launch;
    TecmoGameplayCourtCoordinate hoop;
    TecmoGameplayCourtCoordinate position;
    size_t actor;
    bool launched = false;
    scene_test_prepare_failure_stage = 1U;
    if (scene == NULL || base_launch == NULL ||
        approach_distance_x < -8 || approach_distance_x > 320 ||
        y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y ||
        y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
        return false;
    }
    launch = *base_launch;
    launch.starter_binding_bound = true;
    for (size_t side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        for (size_t local = 0U;
             local < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT; ++local) {
            launch.starter_roster_index[side][local] = (uint8_t)local;
        }
    }
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        return false;
    }
    launched = true;
    scene_test_prepare_failure_stage = 2U;
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        if (launched) tecmo_gameplay_scene_end(scene);
        return false;
    }
    scene_test_prepare_failure_stage = 3U;
    hoop = scene->orientation_state.offensive_hoop;
    position.x = scene->orientation_state.attack_direction == 0U
        ? (int16_t)((int)hoop.x + approach_distance_x)
        : (int16_t)((int)hoop.x - approach_distance_x);
    position.y = y;
    if (!scene_actor_coordinate_valid(&position)) {
        if (launched) tecmo_gameplay_scene_end(scene);
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        scene->actors[actor].position.x = 500;
        scene->actors[actor].position.y = 230;
        scene->actors[actor].anchor = scene->actors[actor].position;
        scene->actors[actor].movement_boundary_latched = false;
    }
    scene->actors[0U].position = position;
    scene->actors[0U].anchor = position;
    scene->actors[0U].facing_right =
        scene->orientation_state.attack_direction == 0U
            ? position.x < hoop.x : position.x > hoop.x;
    scene->state.clock_minutes = 1U;
    scene->state.clock_seconds = 0U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 12U;
    scene->frame = frame_seed;
    if (!scene_attach_ball(scene)) {
        scene_test_prepare_failure_stage = 4U;
        if (launched) tecmo_gameplay_scene_end(scene);
        return false;
    }
    if (!scene_sync_live_foundation(scene)) {
        scene_test_prepare_failure_stage = 5U;
        if (launched) tecmo_gameplay_scene_end(scene);
        return false;
    }
    scene_test_prepare_failure_stage = 0U;
    return true;
}

static bool scene_test_find_close_route(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    uint8_t desired_selector)
{
    static const int approaches[] = {8, 16, 24, 32, 40, 48};
    static const int16_t heights[] = {95, 105, 120, 143, 160, 180, 200};
    size_t approach;
    size_t height;
    uint32_t frame;
    TecmoGameplayShotRimRouteKind expected_kind;
    if (scene == NULL || base_launch == NULL || desired_selector > 3U) {
        return false;
    }
    expected_kind = desired_selector == 1U
        ? TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9
        : desired_selector == 2U
            ? TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9
            : TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708;
    for (frame = 0U; frame < 256U; ++frame) {
        for (approach = 0U;
             approach < sizeof(approaches) / sizeof(approaches[0]);
             ++approach) {
            for (height = 0U;
                 height < sizeof(heights) / sizeof(heights[0]); ++height) {
                if (!scene_test_prepare_owned_shot_fixture(
                        scene, base_launch, approaches[approach],
                        heights[height], frame)) {
                    continue;
                }
                if (!scene_start_shot_actor(scene, 0U, 0U)) {
                    tecmo_gameplay_scene_end(scene);
                    continue;
                }
                if (scene_shot_is_close(scene->shot_kind) &&
                    scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS &&
                    scene->shot_rim_route.selector == desired_selector &&
                    scene->shot_rim_route.kind == expected_kind) {
                    return true;
                }
                tecmo_gameplay_scene_end(scene);
            }
        }
    }
    return false;
}

static bool scene_test_settlement_one_point_fields(
    TecmoGameplayScene *scene);

static bool scene_test_find_approx_make(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    uint8_t desired_points,
    int desired_selector)
{
    static const int approaches[] = {64, 96, 128, 160, 220, 300};
    static const int16_t heights[] = {95, 110, 125, 143, 160, 180, 200, 215};
    size_t approach;
    size_t height;
    uint32_t frame;
    if (scene == NULL || base_launch == NULL || desired_points > 3U ||
        desired_selector < -1 || desired_selector > 3) {
        return false;
    }
    for (frame = 0U; frame < 256U; ++frame) {
        for (approach = 0U;
             approach < sizeof(approaches) / sizeof(approaches[0]);
             ++approach) {
            for (height = 0U;
                 height < sizeof(heights) / sizeof(heights[0]); ++height) {
                if (!scene_test_prepare_owned_shot_fixture(
                        scene, base_launch, approaches[approach],
                        heights[height], frame)) {
                    continue;
                }
                if (!scene_start_shot_actor(scene, 0U, 0U)) {
                    tecmo_gameplay_scene_end(scene);
                    continue;
                }
                if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                    scene->predicted_make_route &&
                    scene->shot_schedule ==
                        TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION &&
                     scene->shot_outcome ==
                         TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                     (desired_selector < 0 ||
                      scene->shot_rim_route.selector ==
                          (uint8_t)desired_selector) &&
                    (desired_points == 0U || desired_points == 1U ||
                      scene->shot_points == desired_points)) {
                    if (desired_points == 1U) {
                        bool rebound = false;
                        for (uint32_t launch_frame = 0U;
                             launch_frame < 256U; ++launch_frame) {
                            TecmoGameplayScene one_point = *scene;
                            one_point.shot_launch_frame = launch_frame;
                            one_point.frame = launch_frame;
                            if (scene_test_settlement_one_point_fields(
                                    &one_point) &&
                                scene_ownership_valid(&one_point)) {
                                *scene = one_point;
                                rebound = true;
                                break;
                            }
                        }
                        if (!rebound) {
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                    }
                    return true;
                }
                tecmo_gameplay_scene_end(scene);
            }
        }
    }
    return false;
}

static unsigned scene_test_advance_failure;

static bool scene_test_advance_to_rim_tail(
    TecmoGameplayScene *scene)
{
    TecmoControlFrame neutral;
    unsigned updates = 0U;
    scene_test_advance_failure = 0U;
    if (scene == NULL || !scene_shot_is_close(scene->shot_kind) ||
        scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }
    memset(&neutral, 0, sizeof(neutral));
    while (!scene->shot_rim_tail_active &&
           scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           updates < 140U) {
        if (!scene_update_shot(scene, &neutral)) {
            scene_test_advance_failure = updates + 1U;
            return false;
        }
        ++updates;
    }
    if (!scene->shot_rim_tail_active || scene->shot_rim_tail_frame != 0U) {
        scene_test_advance_failure = 1000U + updates;
        return false;
    }
    return true;
}

static unsigned scene_test_route_failure;
static unsigned scene_test_terminal_failure;
static unsigned scene_test_terminal_detail;

static unsigned scene_test_production_matrix_failure;
static unsigned scene_test_matrix_setup_failure;
static unsigned scene_test_contact_failure;

static bool scene_test_prepare_matrix_fixture(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    TecmoGameplayTeam shooting_team,
    uint8_t shooter_roster,
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint32_t frame_seed,
    unsigned context_kind)
{
    TecmoGameplaySceneLaunch launch;
    TecmoGameplayCourtCoordinate hoop;
    TecmoGameplayCourtCoordinate position;
    uint8_t shooter;
    uint8_t controller;
    int16_t far_x;
    int16_t far_y;
    if (scene == NULL || base_launch == NULL ||
        (shooting_team != TECMO_GAMEPLAY_TEAM_AWAY &&
         shooting_team != TECMO_GAMEPLAY_TEAM_HOME) ||
        shooter_roster >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return false;
    }
    launch = *base_launch;
    launch.starter_binding_bound = true;
    for (size_t side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        for (size_t local = 0U;
             local < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT; ++local) {
            launch.starter_roster_index[side][local] = (uint8_t)local;
        }
    }
    if (shooting_team == TECMO_GAMEPLAY_TEAM_AWAY) {
        launch.starter_roster_index[0U][0U] = shooter_roster;
        controller = 0U;
        shooter = 0U;
    } else {
        launch.starter_roster_index[1U][0U] = shooter_roster;
        controller = 1U;
        shooter = 5U;
    }
    for (size_t side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        uint8_t next = 0U;
        if ((shooting_team == TECMO_GAMEPLAY_TEAM_AWAY && side == 0U) ||
            (shooting_team == TECMO_GAMEPLAY_TEAM_HOME && side == 1U)) {
            for (size_t local = 1U;
                 local < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT; ++local) {
                while (next == shooter_roster) ++next;
                launch.starter_roster_index[side][local] = next++;
            }
        }
    }
    launch.controller_team[0U] = shooting_team == TECMO_GAMEPLAY_TEAM_AWAY
        ? TECMO_GAMEPLAY_TEAM_AWAY : TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1U] = shooting_team == TECMO_GAMEPLAY_TEAM_HOME
        ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) {
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        scene_test_matrix_setup_failure = 1U;
        return false;
    }
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    if (!scene_handoff_possession(scene, shooting_team, shooter)) {
        scene_test_matrix_setup_failure = 2U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    hoop = scene->orientation_state.offensive_hoop;
    position.x = (int16_t)((int)hoop.x - (int)target_delta_x);
    position.y = (int16_t)((int)hoop.y - (int)target_delta_y);
    if (!scene_actor_coordinate_valid(&position)) {
        scene_test_matrix_setup_failure = 3U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    /* Keep the non-shooters inside the perspective court boundary.  The old
       extrema (x=760/0) are legal rectangle coordinates but invalid actor
       world positions at their corresponding depth, which makes the bound
       foundation synchronizer reject otherwise valid direction cases. */
    far_x = position.x < 384 ? 520 : 240;
    far_y = position.y < 120 ? 220 : 20;
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        scene->actors[actor].position.x = far_x;
        scene->actors[actor].position.y = far_y;
        scene->actors[actor].anchor = scene->actors[actor].position;
        scene->actors[actor].movement_boundary_latched = false;
    }
    scene->actors[shooter].position = position;
    scene->actors[shooter].anchor = position;
    scene->actors[shooter].facing_right = position.x < hoop.x;
    if (context_kind == 1U || context_kind == 2U) {
        uint8_t defender = shooting_team == TECMO_GAMEPLAY_TEAM_AWAY
            ? 5U : 0U;
        scene->actors[defender].position.x = (int16_t)(position.x +
            (context_kind == 2U ? 8 : 30));
        scene->actors[defender].position.y = (int16_t)(position.y +
            (context_kind == 2U ? 8 : 20));
        if (!scene_actor_coordinate_valid(
                &scene->actors[defender].position)) {
            scene_test_matrix_setup_failure = 4U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        scene->actors[defender].anchor = scene->actors[defender].position;
    }
    scene->state.clock_minutes = 1U;
    scene->state.clock_seconds = 0U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 12U;
    scene->frame = frame_seed;
    if (!scene_attach_ball(scene)) {
        scene_test_matrix_setup_failure = 5U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    if (!scene_sync_live_foundation(scene)) {
        scene_test_matrix_setup_failure = 6U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    if (!scene_start_shot_actor(scene, controller, shooter)) {
        scene_test_matrix_setup_failure = 7U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    return true;
}

static bool scene_test_production_jump_matrix(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    static const int16_t deltas_x[8] = {
        96, -96, 8, 64, -64, 8, 64, -64
    };
    static const int16_t deltas_y[8] = {
        0, 0, 32, 64, 64, -32, -64, -64
    };
    uint8_t profile_roster[TECMO_GAMEPLAY_TEAM_COUNT][2];
    uint8_t matrix_team[2] = {0xFFU, 0xFFU};
    TecmoGameplaySceneLaunch matrix_launch;
    TecmoGameplayScene replay;
    TecmoControlFrame neutral;
    if (scene == NULL || base_launch == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        scene_test_production_matrix_failure = 1U;
        return false;
    }
    memset(profile_roster, 0xFF, sizeof(profile_roster));
    for (uint8_t team_id = 0U;
         team_id < TECMO_TEAM_DATA_TEAM_COUNT; ++team_id) {
        bool has_profile[2] = {false, false};
        for (uint8_t roster = 0U;
             roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM; ++roster) {
            uint8_t profile;
            if (!tecmo_gameplay_shot_profile_from_profile_byte2(
                    scene->pretip_team_data->players[team_id][roster]
                        .profile[2], &profile)) {
                scene_test_production_matrix_failure = 2U;
                return false;
            }
            has_profile[profile] = true;
        }
        if (has_profile[0] && has_profile[1]) {
            if (matrix_team[0] == 0xFFU) matrix_team[0] = team_id;
            else if (matrix_team[1] == 0xFFU &&
                     team_id != matrix_team[0]) matrix_team[1] = team_id;
        }
    }
    if (matrix_team[0] == 0xFFU || matrix_team[1] == 0xFFU) {
        scene_test_production_matrix_failure = 3U;
        return false;
    }
    matrix_launch = *base_launch;
    matrix_launch.away_team = matrix_team[0];
    matrix_launch.home_team = matrix_team[1];
    for (unsigned team_slot = 0U; team_slot < 2U; ++team_slot) {
        uint8_t team_id = matrix_team[team_slot];
        for (uint8_t roster = 0U;
             roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM; ++roster) {
            uint8_t profile;
            if (!tecmo_gameplay_shot_profile_from_profile_byte2(
                    scene->pretip_team_data->players[team_id][roster]
                        .profile[2], &profile)) {
                scene_test_production_matrix_failure = 4U;
                return false;
            }
            if (profile_roster[team_slot][profile] == 0xFFU) {
                profile_roster[team_slot][profile] = roster;
            }
        }
        if (profile_roster[team_slot][0] == 0xFFU ||
            profile_roster[team_slot][1] == 0xFFU) {
            scene_test_production_matrix_failure = 5U + team_slot;
            return false;
        }
    }
    memset(&neutral, 0, sizeof(neutral));
    scene_test_production_matrix_failure = 0U;
    scene_test_matrix_setup_failure = 0U;
    /* Exercise both retained roster profile bits and every Bank05
       $9054/$8DD3/$BF6C hoop-vector sector in both court orientations.  The
       $8B12 reset family remains intentionally fail-closed at zero because
       the complete $8B83-$8BC8 gate is not owned; $842C must still use the
       exact profile bit and eight-way direction when it indexes $8D3D/$8D5D.
       This is a production launch test, not a shooting-lab table walk. */
    for (unsigned family = 0U; family < 1U; ++family) {
        for (unsigned profile = 0U; profile < 2U; ++profile) {
            for (unsigned direction = 0U; direction < 8U; ++direction) {
                bool found = false;
                TecmoGameplayTeam shooting_team = deltas_x[direction] >= 0
                    ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_TEAM_AWAY;
                unsigned team_slot = shooting_team ==
                    TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U;
                for (uint32_t frame = 0U; frame < 256U && !found; ++frame) {
                    bool saw_flight = false;
                    bool saw_release = false;
                    if (!scene_test_prepare_matrix_fixture(
                            scene, &matrix_launch, shooting_team,
                            profile_roster[team_slot][profile], deltas_x[direction],
                            deltas_y[direction], frame, 0U)) {
                        continue;
                    }
                    {
                        TecmoGameplayShotDirectionSlot expected_direction;
                        uint16_t expected_pose;
                        bool expected_facing_right = deltas_x[direction] > 0;
                        int32_t expected_end_x;
                        int32_t expected_end_y;
                        expected_end_x = (int32_t)scene->orientation_state
                                             .offensive_hoop.x * 256;
                        expected_end_y =
                            (int32_t)TECMO_GAMEPLAY_SHOT_TARGET_Y * 256;
                        if (!tecmo_gameplay_shot_resolution_direction_for_delta(
                                deltas_x[direction], deltas_y[direction],
                                &expected_direction) ||
                            (unsigned)expected_direction != direction ||
                            scene->shot_kind !=
                                TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
                            scene->jump_family !=
                                TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 ||
                            (unsigned)scene->jump_profile != profile ||
                            scene->jump_direction != expected_direction ||
                            !tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                                &scene->jump_shots, scene->jump_family,
                                scene->jump_profile, scene->jump_direction,
                                &expected_pose) ||
                            scene->jump_resolved_pose_index != expected_pose ||
                            scene->actors[scene->shot_actor]
                                .pose_orientation_encoded ||
                            scene->actors[scene->shot_actor].facing_right !=
                                expected_facing_right ||
                            scene->shot_launch_facing_right !=
                                expected_facing_right ||
                            scene->shot_target_delta_x != deltas_x[direction] ||
                            scene->shot_target_delta_y != deltas_y[direction] ||
                            scene->shot_end_position.x_q8 != expected_end_x ||
                            scene->shot_end_position.y_q8 != expected_end_y ||
                            !scene_ownership_valid(scene)) {
                        scene_test_matrix_setup_failure = 8U +
                            ((unsigned)scene->shot_kind & 0x03U) * 16U +
                            (unsigned)scene->jump_family * 4U +
                            (unsigned)scene->jump_profile;
                        tecmo_gameplay_scene_end(scene);
                        continue;
                    }
                    }
                    {
                        TecmoGameplayScene malformed = *scene;
                        TecmoGameplayScene snapshot;
                        malformed.jump_resolved_pose_index ^= 0x0001U;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_production_matrix_failure =
                                400U + family * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        malformed.actors[malformed.shot_actor].facing_right =
                            !malformed.actors[malformed.shot_actor].facing_right;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_production_matrix_failure =
                                500U + family * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        malformed.jump_rim_rattle_raw_selector = 1U;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_production_matrix_failure =
                                600U + family * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                    }
                    replay = *scene;
                    for (unsigned update = 0U; update < 12U; ++update) {
                        if (!scene_update_shot(scene, &neutral) ||
                            !scene_update_shot(&replay, &neutral)) {
                            scene_test_production_matrix_failure =
                                100U + family * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        if (scene->shot_kind ==
                                TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
                            break;
                        }
                        saw_release = saw_release || scene->jump_b_released;
                        if (scene->shot_frame >= 3U) {
                            uint16_t expected_phase_pose;
                            if (!tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
                                    &scene->jump_shots, scene->jump_family,
                                    scene->jump_profile, scene->jump_direction,
                                    scene->jump_phase_counter,
                                    &expected_phase_pose) ||
                                scene->actors[scene->shot_actor].pose_index !=
                                    expected_phase_pose ||
                                expected_phase_pose ==
                                    scene->jump_resolved_pose_index) {
                                scene_test_production_matrix_failure =
                                    700U + family * 100U + profile * 20U +
                                    direction;
                                tecmo_gameplay_scene_end(scene);
                                return false;
                            }
                            saw_flight = true;
                        }
                    }
                    if (!saw_flight || !saw_release ||
                        memcmp(scene, &replay, sizeof(*scene)) != 0) {
                        scene_test_production_matrix_failure =
                            200U + family * 100U + profile * 20U + direction;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    tecmo_gameplay_scene_end(scene);
                    found = true;
                }
                if (!found) {
                    scene_test_production_matrix_failure =
                        300U + family * 100U + profile * 20U + direction;
                    return false;
                }
            }
        }
    }
    return true;
}

static bool scene_test_production_contact_contexts(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    uint8_t context;
    uint8_t shooter_roster = 0U;
    uint8_t best_rating = 0U;
    uint8_t expected_contact[3] = {0U, 0U, 1U};
    uint8_t expected_contest[3] = {0U, 1U, 1U};
    uint8_t probability[3] = {0U, 0U, 0U};
    TecmoControlFrame neutral;
    TecmoGameplayScene corrupted;
    TecmoGameplayScene snapshot;
    scene_test_contact_failure = 0U;
    if (scene == NULL || base_launch == NULL) {
        scene_test_contact_failure = 1U;
        return false;
    }
    if (scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        scene_test_contact_failure = 2U;
        return false;
    }
    for (uint8_t roster = 0U;
         roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM; ++roster) {
        const TecmoTeamDataPlayer *player =
            &scene->pretip_team_data->players[base_launch->away_team][roster];
        if (player->profile[0] >= best_rating) {
            best_rating = player->profile[0];
            shooter_roster = roster;
        }
    }
    memset(&neutral, 0, sizeof(neutral));
    for (context = 0U; context < 3U; ++context) {
        if (!scene_test_prepare_matrix_fixture(
                scene, base_launch, TECMO_GAMEPLAY_TEAM_AWAY, shooter_roster,
                -64, 0, (uint32_t)(context + 17U), context)) {
            scene_test_contact_failure = 10U + context;
            return false;
        }
        if (scene->shot_contact_context != expected_contact[context] ||
            scene->shot_contest_context != expected_contest[context]) {
            scene_test_contact_failure = 20U + context;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        probability[context] = scene->shot_make_probability;
        corrupted = *scene;
        corrupted.shot_contact_context = !corrupted.shot_contact_context;
        snapshot = corrupted;
        if (scene_update_shot(&corrupted, &neutral) ||
            memcmp(&corrupted, &snapshot, sizeof(corrupted)) != 0) {
            scene_test_contact_failure = 25U + context;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        corrupted = *scene;
        corrupted.shot_contest_context = !corrupted.shot_contest_context;
        snapshot = corrupted;
        if (scene_update_shot(&corrupted, &neutral) ||
            memcmp(&corrupted, &snapshot, sizeof(corrupted)) != 0) {
            scene_test_contact_failure = 28U + context;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        corrupted = *scene;
        corrupted.shot_context_signature ^= 0x00000001U;
        snapshot = corrupted;
        if (scene_update_shot(&corrupted, &neutral) ||
            memcmp(&corrupted, &snapshot, sizeof(corrupted)) != 0) {
            scene_test_contact_failure = 32U + context;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        if (!scene_update_shot(scene, &neutral)) {
            scene_test_contact_failure = 30U + context;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }
    if (!(probability[0] > probability[1] &&
          probability[1] > probability[2])) {
        scene_test_contact_failure = 40U;
        return false;
    }
    return true;
}

static unsigned scene_test_close_matrix_failure;
static unsigned scene_test_close_matrix_detail;

static bool scene_test_production_close_matrix(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    static const int16_t variant0_dx[8] = {
        8, -8, 2, 8, -8, 2, 8, -8
    };
    static const int16_t variant0_dy[8] = {
        0, 0, 16, 16, 16, -16, -16, -16
    };
    static const int16_t variant2_dx[8] = {
        32, -32, 2, 32, -32, 2, 32, -32
    };
    static const int16_t variant2_dy[8] = {
        0, 0, 16, 16, 16, -16, -16, -16
    };
    uint8_t profile_roster[2][2];
    uint8_t matrix_team[2] = {0xFFU, 0xFFU};
    TecmoGameplaySceneLaunch matrix_launch;
    TecmoControlFrame neutral;
    uint16_t numeric1_pose[8] = {0U};
    if (scene == NULL || base_launch == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        scene_test_close_matrix_failure = 1U;
        return false;
    }
    memset(profile_roster, 0xFF, sizeof(profile_roster));
    for (uint8_t team_id = 0U;
         team_id < TECMO_TEAM_DATA_TEAM_COUNT; ++team_id) {
        bool has_profile[2] = {false, false};
        for (uint8_t roster = 0U;
             roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM; ++roster) {
            uint8_t profile;
            if (!tecmo_gameplay_shot_profile_from_profile_byte2(
                    scene->pretip_team_data->players[team_id][roster]
                        .profile[2], &profile)) {
                scene_test_close_matrix_failure = 2U;
                return false;
            }
            has_profile[profile] = true;
        }
        if (has_profile[0] && has_profile[1]) {
            if (matrix_team[0] == 0xFFU) matrix_team[0] = team_id;
            else if (matrix_team[1] == 0xFFU &&
                     team_id != matrix_team[0]) matrix_team[1] = team_id;
        }
    }
    if (matrix_team[0] == 0xFFU || matrix_team[1] == 0xFFU) {
        scene_test_close_matrix_failure = 3U;
        return false;
    }
    matrix_launch = *base_launch;
    matrix_launch.away_team = matrix_team[0];
    matrix_launch.home_team = matrix_team[1];
    for (unsigned team_slot = 0U; team_slot < 2U; ++team_slot) {
        uint8_t team_id = matrix_team[team_slot];
        for (uint8_t roster = 0U;
             roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM; ++roster) {
            uint8_t profile;
            if (!tecmo_gameplay_shot_profile_from_profile_byte2(
                    scene->pretip_team_data->players[team_id][roster]
                        .profile[2], &profile)) {
                scene_test_close_matrix_failure = 4U;
                return false;
            }
            if (profile_roster[team_slot][profile] == 0xFFU) {
                profile_roster[team_slot][profile] = roster;
            }
        }
    }
    if (profile_roster[0][0] == 0xFFU || profile_roster[0][1] == 0xFFU ||
        profile_roster[1][0] == 0xFFU || profile_roster[1][1] == 0xFFU) {
        scene_test_close_matrix_failure = 5U;
        return false;
    }
    memset(&neutral, 0, sizeof(neutral));
    scene_test_close_matrix_failure = 0U;
    for (unsigned variant = 0U; variant < 3U; ++variant) {
        for (unsigned profile = 0U; profile < 2U; ++profile) {
            for (unsigned direction = 0U; direction < 8U; ++direction) {
                bool found = false;
                unsigned prepared_count = 0U;
                unsigned started_count = 0U;
                unsigned close_count = 0U;
                unsigned variant_count = 0U;
                const int16_t *dx = variant == 2U
                    ? variant2_dx : variant0_dx;
                const int16_t *dy = variant == 2U
                    ? variant2_dy : variant0_dy;
                for (unsigned team_attempt = 0U;
                     team_attempt < 2U && !found; ++team_attempt) {
                    TecmoGameplayTeam shooting_team = team_attempt == 0U
                        ? TECMO_GAMEPLAY_TEAM_AWAY
                        : TECMO_GAMEPLAY_TEAM_HOME;
                    unsigned team_slot = shooting_team ==
                        TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U;
                    for (uint32_t frame = 0U; frame < 256U && !found;
                         ++frame) {
                    /* The separate bound context matrix exercises far,
                       contest-only, and contact+contest contexts. Keep this
                       exhaustive selector matrix uncontested so near-hoop
                       perspective setup cannot mask a pose-selector case. */
                    unsigned context_kind = 0U;
                    TecmoGameplayScene replay;
                    uint16_t pose;
                    if (!scene_test_prepare_matrix_fixture(
                            scene, &matrix_launch, shooting_team,
                            profile_roster[team_slot][profile], dx[direction],
                            dy[direction], frame, context_kind)) {
                        continue;
                    }
                    ++prepared_count;
                    ++started_count;
                    if (scene->shot_kind !=
                            TECMO_GAMEPLAY_SCENE_SHOT_DUNK &&
                        scene->shot_kind !=
                            TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1 &&
                        scene->shot_kind !=
                            TECMO_GAMEPLAY_SCENE_SHOT_LAYUP) {
                        tecmo_gameplay_scene_end(scene);
                        continue;
                    }
                    ++close_count;
                    if ((unsigned)scene->close_shot_variant != variant) {
                        /* Stable-source bits select among the three native
                           numeric identities; an otherwise valid close shot
                           of another identity is a bounded search miss. */
                        tecmo_gameplay_scene_end(scene);
                        continue;
                    }
                    {
                        unsigned selector_failure = 0U;
                        bool pose_ok = scene_close_pose_for_step(
                            scene, 0U, &pose);
                        if ((unsigned)scene->close_shot_profile != profile) {
                            selector_failure |= 2U;
                        }
                        if ((unsigned)scene->close_shot_direction != direction) {
                            selector_failure |= 4U;
                        }
                        if (scene->shot_kind !=
                                (variant == 0U
                                    ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                                    : variant == 1U
                                        ? TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1
                                        : TECMO_GAMEPLAY_SCENE_SHOT_LAYUP)) {
                            selector_failure |= 64U;
                        }
                        if (!scene_ownership_valid(scene)) {
                            selector_failure |= 8U;
                        }
                        if (!pose_ok) selector_failure |= 16U;
                        else if (scene->actors[scene->shot_actor].pose_index !=
                                 pose) {
                            selector_failure |= 32U;
                        }
                        if (selector_failure != 0U) {
                            scene_test_close_matrix_failure =
                                100U + variant * 100U + profile * 20U + direction;
                            scene_test_close_matrix_detail = selector_failure |
                                ((unsigned)scene->close_shot_variant << 8U) |
                                ((unsigned)scene->close_shot_profile << 12U) |
                                ((unsigned)scene->close_shot_direction << 16U) |
                                ((unsigned)scene->shot_kind << 20U);
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                    }
                    {
                        TecmoGameplayScene malformed = *scene;
                        TecmoGameplayScene snapshot;
                        malformed.native_policy_sample ^= 0x80000000U;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_close_matrix_failure =
                                500U + variant * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        malformed.shot_launch_frame ^= 0x00010000U;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_close_matrix_failure =
                                600U + variant * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        malformed.shot_launch_facing_right =
                            !malformed.shot_launch_facing_right;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_close_matrix_failure =
                                700U + variant * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        ++malformed.shot_start_position.x_q8;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_close_matrix_failure =
                                800U + variant * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        malformed = *scene;
                        malformed.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
                        snapshot = malformed;
                        if (scene_update_shot(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_close_matrix_failure =
                                900U + variant * 100U + profile * 20U +
                                direction;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                    }
                    ++variant_count;
                    if (variant == 1U) numeric1_pose[direction] = pose;
                    replay = *scene;
                    if (!scene_update_shot(scene, &neutral) ||
                        !scene_update_shot(&replay, &neutral) ||
                        memcmp(scene, &replay, sizeof(*scene)) != 0) {
                        scene_test_close_matrix_failure =
                            200U + variant * 100U + profile * 20U + direction;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    tecmo_gameplay_scene_end(scene);
                    found = true;
                    }
                }
                if (!found) {
                    scene_test_close_matrix_failure =
                        300U + variant * 100U + profile * 20U + direction;
                    scene_test_close_matrix_detail =
                        prepared_count * 1000000U + started_count * 10000U +
                        close_count * 100U + variant_count;
                    return false;
                }
            }
        }
    }
    /* Bank05 $8C7D fixed group $10 ignores profile for numeric identity 1. */
    for (unsigned direction = 0U; direction < 8U; ++direction) {
        TecmoGameplayCloseShotDirection close_direction =
            (TecmoGameplayCloseShotDirection)direction;
        uint16_t expected;
        if (!tecmo_gameplay_close_shots_resolve_numeric_variant1_pose_pointer_index(
                &scene->close_shots, close_direction, &expected) ||
            numeric1_pose[direction] != expected) {
            scene_test_close_matrix_failure = 400U + direction;
            return false;
        }
    }
    return true;
}

static bool scene_test_route_tail_matrix(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    static const TecmoGameplayShotRimRouteKind expected_kind[4] = {
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708
    };
    static const uint16_t expected_cpu[4] = {
        0xA708U, 0xA7A9U, 0xA8E9U, 0xA708U
    };
    uint32_t trace_hash[4] = {0U, 0U, 0U, 0U};
    unsigned observed_updates[4] = {0U, 0U, 0U, 0U};
    static const unsigned expected_duration[4] = {6U, 17U, 7U, 8U};
    TecmoControlFrame neutral;
    TecmoGameplayCourtCoordinate endpoint;
    TecmoGameplayScene before_terminal;
    TecmoGameplayScene malformed;
    TecmoGameplayScene snapshot;
    uint8_t selector;
    scene_test_route_failure = 0U;
    memset(&neutral, 0, sizeof(neutral));
    for (selector = 0U; selector < 4U; ++selector) {
        if (!scene_test_find_close_route(scene, base_launch, selector)) {
            scene_test_route_failure = (unsigned)(10U + selector);
            return false;
        }
        if (!scene_test_advance_to_rim_tail(scene)) {
            scene_test_route_failure = (unsigned)(20U + selector);
            return false;
        }
        if (scene->shot_rim_route.kind != expected_kind[selector] ||
            scene->shot_rim_route.source_target_cpu != expected_cpu[selector] ||
            scene->shot_rim_route.selector != selector ||
            (scene->shot_rim_rattle_raw_selector & 0x03U) != selector) {
            scene_test_route_failure = (unsigned)(30U + selector);
            return false;
        }
        malformed = *scene;
        malformed.shot_rim_tail_duration++;
        malformed.shot_duration++;
        snapshot = malformed;
        if (scene_update_shot(&malformed, &neutral) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            scene_test_route_failure = (unsigned)(31U + selector);
            return false;
        }
        malformed = *scene;
        malformed.shot_rim_tail_base_frame++;
        malformed.shot_frame++;
        malformed.shot_duration++;
        snapshot = malformed;
        if (scene_update_shot(&malformed, &neutral) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            scene_test_route_failure = (unsigned)(35U + selector);
            return false;
        }
        malformed = *scene;
        malformed.shot_rim_route.selector =
            (uint8_t)((selector + 1U) & 0x03U);
        snapshot = malformed;
        if (scene_update_shot(&malformed, &neutral) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            scene_test_route_failure = (unsigned)(39U + selector);
            return false;
        }
        endpoint.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
        endpoint.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
        for (unsigned actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->actors[actor].position.x = 650;
            scene->actors[actor].position.y = 230;
            scene->actors[actor].anchor = scene->actors[actor].position;
        }
        /* Alternate the proven settlement relation so all four route
           identities exercise both same-team retention and opposing-team
           handoff.  Actor-slot order, not geometric nearest distance, is the
           bounded source-order substitution. */
        if ((selector & 1U) == 0U) {
            scene->actors[5U].position = endpoint;
            scene->actors[5U].anchor = endpoint;
        } else {
            scene->actors[1U].position = endpoint;
            scene->actors[1U].anchor = endpoint;
        }
        for (unsigned update = 0U;
             update < expected_duration[selector]; ++update) {
            if (!scene_update_shot(scene, &neutral)) {
                scene_test_route_failure = (unsigned)(40U + selector);
                return false;
            }
            ++observed_updates[selector];
            if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
                trace_hash[selector] = trace_hash[selector] * 16777619U ^
                    (uint32_t)scene->ball_position.x_q8;
                trace_hash[selector] = trace_hash[selector] * 16777619U ^
                    (uint32_t)scene->ball_position.y_q8;
                if (!scene->shot_rim_tail_active) {
                    scene_test_route_failure = (unsigned)(50U + selector);
                    return false;
                }
                if (selector == 1U &&
                    (scene->jump_ball_altitude_q8 != 0x3800U ||
                     scene->jump_rim_rattle_audio_repeats !=
                         ((update + 1U) / 4U > 3U
                              ? 3U : (update + 1U) / 4U))) {
                    scene_test_route_failure = (unsigned)(60U + update);
                    return false;
                }
            }
        }
        if (observed_updates[selector] != expected_duration[selector] ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
            scene->shot_rim_route.selector != 0U ||
            scene->shot_rim_route.kind != 0 ||
            scene->shot_rim_route.source_target_cpu != 0U ||
            ((selector & 1U) == 0U
                ? (scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
                   scene->ball_holder != 5U)
                : (scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
                   scene->ball_holder != 1U))) {
            scene_test_route_failure = (unsigned)(70U + selector);
            return false;
        }
        before_terminal = *scene;
        if (scene_update_shot(scene, &neutral) ||
            memcmp(scene, &before_terminal, sizeof(*scene)) != 0) {
            scene_test_route_failure = (unsigned)(80U + selector);
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }
    if (observed_updates[0] != 6U || observed_updates[1] != 17U ||
        observed_updates[2] != 7U || observed_updates[3] != 8U) {
        scene_test_route_failure = 40U;
        return false;
    }
    if (trace_hash[0] == trace_hash[3]) {
        scene_test_route_failure = 41U;
        return false;
    }
    if (trace_hash[0] == trace_hash[2]) {
        scene_test_route_failure = 42U;
        return false;
    }
    if (trace_hash[3] == trace_hash[2]) {
        scene_test_route_failure = 43U;
        return false;
    }
    return true;
}

static bool scene_test_find_bound_close_terminal_case(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    unsigned desired_variant,
    TecmoGameplayShotOutcome desired_outcome,
    int desired_selector)
{
    static const int16_t variant0_dx[8] = {
        8, -8, 2, 8, -8, 2, 8, -8
    };
    static const int16_t variant0_dy[8] = {
        0, 0, 16, 16, 16, -16, -16, -16
    };
    static const int16_t variant2_dx[8] = {
        32, -32, 2, 32, -32, 2, 32, -32
    };
    static const int16_t variant2_dy[8] = {
        0, 0, 16, 16, 16, -16, -16, -16
    };
    const int16_t *dx = desired_variant == 2U ? variant2_dx : variant0_dx;
    const int16_t *dy = desired_variant == 2U ? variant2_dy : variant0_dy;
    if (scene == NULL || base_launch == NULL || desired_variant > 2U ||
        (desired_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
         desired_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) ||
        desired_selector < -1 || desired_selector > 3) {
        return false;
    }
    for (unsigned team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT; ++team) {
        for (unsigned direction = 0U; direction < 8U; ++direction) {
            for (uint32_t frame = 0U; frame < 256U; ++frame) {
                if (!scene_test_prepare_matrix_fixture(
                        scene, base_launch, (TecmoGameplayTeam)team, 0U,
                        dx[direction], dy[direction], frame, 0U)) {
                    continue;
                }
                if (scene_shot_is_close(scene->shot_kind) &&
                    (unsigned)scene->close_shot_variant == desired_variant &&
                    scene->shot_outcome == desired_outcome &&
                    (desired_selector < 0 ||
                     scene->shot_rim_route.selector ==
                         (uint8_t)desired_selector) &&
                    scene_ownership_valid(scene)) {
                    return true;
                }
                tecmo_gameplay_scene_end(scene);
            }
        }
    }
    return false;
}

/* The matrix fixture deliberately relocates actors after the normal LIVE
   foundation has been built. Rebuild only the future holder before terminal
   handoff; its fixed linked actor remains untouched because the resolver only
   consumes that actor's position. This is test-fixture repair, not a
   shot-path fallback. */
static bool scene_test_prepare_terminal_holder(
    TecmoGameplayScene *scene,
    uint8_t holder,
    bool made_handoff)
{
    uint8_t linked;
    TecmoGameplaySceneActor *actor;
    TecmoGameplayMovementState movement;
    TecmoGameplayBallDribbleFrame dribble;
    TecmoGameplayCourtCoordinateQ8 ball_q8;
    uint16_t pose_index = 0U;
    bool found = false;
    memset(&movement, 0, sizeof(movement));
    if (scene == NULL || holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[holder].active) {
        scene_test_terminal_detail = 401U;
        return false;
    }
    linked = scene->cpu_actors[holder].linked_actor;
    if (linked >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT || linked == holder ||
        !scene->actors[linked].active) {
        scene_test_terminal_detail = 402U;
        return false;
    }
    actor = &scene->actors[holder];
    if (made_handoff) {
        /* The matrix's far_y=20 staging can place the attached ball above
           the resolver's court boundary.  Made possession is handed to the
           opposing team, so use the source-tested safe canonical position;
           miss claimants retain their captured endpoint position. */
        actor->position.x = 352;
        actor->position.y = 198;
        actor->anchor = actor->position;
    }
    for (uint8_t direction = 0U; direction < 8U && !found; ++direction) {
        if (!tecmo_gameplay_movement_state_initialize(
                &scene->movement_assets, &movement, &actor->position,
                direction) ||
            !scene_movement_pose_index(
                scene, &movement,
                &scene->actors[scene->cpu_actors[holder].linked_actor]
                    .position,
                &pose_index) ||
            !tecmo_gameplay_ball_dribble_resolve(
                &scene->ball_dribble_assets, &scene->movement_assets,
                &movement,
                &scene->actors[scene->cpu_actors[holder].linked_actor]
                    .position,
                &dribble) ||
            !tecmo_gameplay_court_coordinate_to_q8(
                &dribble.visible_position, &ball_q8)) {
            continue;
        }
        found = true;
    }
    if (!found) {
        scene_test_terminal_detail = 430U;
        return false;
    }
    {
        actor->movement_action_state = movement.action_state;
        actor->movement_direction = movement.direction;
        actor->movement_fractional_accumulator =
            movement.fractional_accumulator;
        actor->movement_animation_phase = movement.animation_phase;
        actor->movement_boundary_latched =
            movement.boundary_violation_latched;
        actor->pose_index = pose_index;
    }
    {
        if (!scene_live_ball_frame_for_actors(
                scene, scene->actors, holder, &dribble)) {
            scene_test_terminal_detail = 431U;
            return false;
        }
        if (!tecmo_gameplay_court_coordinate_to_q8(
                &dribble.visible_position, &ball_q8)) {
            scene_test_terminal_detail = 432U;
            return false;
        }
        return true;
    }
}

static bool scene_test_run_bound_close_terminal_case(
    TecmoGameplayScene *scene,
    TecmoGameplayShotOutcome desired_outcome,
    unsigned expected_variant,
    int desired_selector,
    const TecmoControlFrame *neutral)
{
    TecmoGameplayScene snapshot;
    TecmoGameplayCourtCoordinate endpoint;
    TecmoGameplayTeam shooting_team;
    TecmoGameplayTeam claimant_team;
    TecmoGameplaySceneClaimantSettlementTrace claimant_trace_before;
    uint16_t score_before[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t points;
    uint8_t stat_team;
    uint8_t stat_roster;
    unsigned updates = 0U;
    if (scene == NULL || neutral == NULL ||
        !scene_shot_is_close(scene->shot_kind) ||
        (unsigned)scene->close_shot_variant != expected_variant ||
        scene->shot_outcome != desired_outcome ||
        (desired_selector >= 0 &&
         scene->shot_rim_route.selector != (uint8_t)desired_selector) ||
        !scene_ownership_valid(scene)) {
        scene_test_terminal_detail = 1U;
        return false;
    }
    shooting_team = (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
    claimant_trace_before = scene->claimant_settlement_trace;
    if (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        /* The trace keeps zero as its never-emitted sentinel.  Exercise the
           production terminal bridge at its diagnostic wrap boundary without
           injecting a claimant, result, possession, or finish event. */
        scene->claimant_settlement_trace.event_serial = UINT32_MAX;
        claimant_trace_before = scene->claimant_settlement_trace;
    }
    points = scene->shot_points;
    stat_team = scene->shot_actor_team;
    stat_roster = scene->shot_actor_roster_index;
    if (stat_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        stat_roster >= TECMO_PLAYER_STATS_ROSTER_COUNT ||
        scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
        scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PA] !=
            (points == 3U ? 1U : 0U)) {
        scene_test_terminal_detail = 2U;
        return false;
    }
    score_before[TECMO_GAMEPLAY_TEAM_AWAY] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    score_before[TECMO_GAMEPLAY_TEAM_HOME] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    if (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        endpoint.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
        endpoint.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
        for (unsigned actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->actors[actor].position.x = 650;
            scene->actors[actor].position.y = 230;
            scene->actors[actor].anchor = scene->actors[actor].position;
        }
        /* Actor-slot order is the bounded claimant scan substitution. Actor
           1 is deliberately retained even when it is the shooter's team;
           the native $BA/$40 predicate is not proven universal. */
        scene->actors[1U].position = endpoint;
        scene->actors[1U].anchor = endpoint;
        claimant_team = (TecmoGameplayTeam)scene->actors[1U].team;
    } else {
        claimant_team = scene_other_team(shooting_team);
    }
    if (!scene_test_prepare_terminal_holder(
            scene,
            desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ? 1U :
                scene_first_actor_for_team(claimant_team),
            desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE)) {
        if (scene_test_terminal_detail == 0U) {
            scene_test_terminal_detail = 400U;
        }
        return false;
    }
    while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           updates < 140U) {
        if (!scene_update_shot(scene, neutral)) {
            scene_test_terminal_detail = 500000U + scene->shot_frame;
            return false;
        }
        ++updates;
    }
    {
        unsigned final_failure = 0U;
        uint8_t settled_points = scene->shot_points;
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            final_failure |= 1U;
        }
        if (!scene_shot_state_valid(scene)) final_failure |= 2U;
        if (!scene_ownership_valid(scene)) final_failure |= 4U;
        if (scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
                (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                 shooting_team == TECMO_GAMEPLAY_TEAM_AWAY
                     ? (uint16_t)(score_before[TECMO_GAMEPLAY_TEAM_AWAY] +
                                  settled_points)
                     : score_before[TECMO_GAMEPLAY_TEAM_AWAY])) {
            final_failure |= 8U;
        }
        if (scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] !=
                (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                 shooting_team == TECMO_GAMEPLAY_TEAM_HOME
                     ? (uint16_t)(score_before[TECMO_GAMEPLAY_TEAM_HOME] +
                                  settled_points)
                     : score_before[TECMO_GAMEPLAY_TEAM_HOME])) {
            final_failure |= 16U;
        }
        if (scene->state.possession != claimant_team) final_failure |= 32U;
        if (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS &&
            scene->ball_holder != 1U) final_failure |= 64U;
        if (scene->player_stats.counters[stat_team][stat_roster][
                TECMO_PLAYER_STATS_COUNTER_FGM] !=
                (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ? 1U : 0U) ||
            scene->player_stats.counters[stat_team][stat_roster][
                TECMO_PLAYER_STATS_COUNTER_THREE_PM] !=
                (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                 settled_points == 3U ? 1U : 0U))
            final_failure |= 128U;
        /* A terminal miss reaches the one source-shaped claimant bridge;
           a made basket stays on generic scene handoff and must not emit it.
           This is an event-boundary assertion, not a claim that either
           terminal result has a complete original-ROM caller reconstruction. */
        if (desired_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
            if (!scene->claimant_settlement_trace.valid ||
                scene->claimant_settlement_trace.contract_tag !=
                    TECMO_GAMEPLAY_SCENE_CLAIMANT_TRACE_TAG ||
                scene->claimant_settlement_trace.event_serial != 1U ||
                scene->claimant_settlement_trace.event_serial ==
                    claimant_trace_before.event_serial ||
                scene->claimant_settlement_trace.transaction.contract_tag !=
                    TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG ||
                scene->claimant_settlement_trace.before.contract_tag !=
                    TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG ||
                scene->claimant_settlement_trace.after.contract_tag !=
                    TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG ||
                scene->claimant_settlement_trace.after.semantic_ball_holder !=
                    1U ||
                scene->claimant_settlement_trace.after.semantic_scene_possession !=
                    (uint8_t)claimant_team) {
                final_failure |= 256U;
            }
        } else if (memcmp(&scene->claimant_settlement_trace,
                          &claimant_trace_before,
                          sizeof(claimant_trace_before)) != 0) {
            final_failure |= 256U;
        }
        if (final_failure != 0U) {
            scene_test_terminal_detail = 20000U + updates * 128U +
                final_failure;
            return false;
        }
    }
    snapshot = *scene;
    if (scene_update_shot(scene, neutral) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        scene_test_terminal_detail = 300U;
        return false;
    }
    return true;
}

static bool scene_test_run_bound_approximate_make_terminal(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    const TecmoControlFrame *neutral,
    int desired_selector)
{
    TecmoControlFrame held;
    TecmoGameplayScene malformed;
    TecmoGameplayScene snapshot;
    TecmoGameplayTeam shooting_team;
    uint16_t score_before[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t points;
    uint8_t stat_team;
    uint8_t stat_roster;
    unsigned updates = 0U;
    if (scene == NULL || base_launch == NULL || neutral == NULL ||
        !scene_test_find_approx_make(scene, base_launch, 2U,
                                     desired_selector) ||
        scene->shot_schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION ||
        scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        !scene->predicted_make_route || !scene_ownership_valid(scene)) {
        return false;
    }
    if (desired_selector == 1 &&
        (scene->shot_rim_route.selector != 1U ||
         scene->shot_rim_route.kind != TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 ||
         (scene->shot_rim_rattle_raw_selector & 0x03U) != 1U ||
         scene->shot_rim_rattle_selected ||
         scene->jump_rim_rattle.active || scene->jump_rim_rattle.complete)) {
        return false;
    }
    shooting_team = (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
    points = scene->shot_points;
    stat_team = scene->shot_actor_team;
    stat_roster = scene->shot_actor_roster_index;
    if (stat_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        stat_roster >= TECMO_PLAYER_STATS_ROSTER_COUNT ||
        scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
        scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PA] !=
            (points == 3U ? 1U : 0U))
        return false;
    score_before[TECMO_GAMEPLAY_TEAM_AWAY] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    score_before[TECMO_GAMEPLAY_TEAM_HOME] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    memset(&held, 0, sizeof(held));
    held.held.cancel = true;
    if (desired_selector == 1) {
        malformed = *scene;
        malformed.shot_rim_rattle_selected = true;
        snapshot = malformed;
        if (scene_update_shot(&malformed, &held) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            return false;
        }
    }
    while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           updates < 80U) {
        /* Hold through the gather cap, then provide the current-level B clear
           that Bank05 $86DD requires.  Reaching the cap itself must not be
           treated as a release edge. */
        const TecmoControlFrame *controls = !scene->jump_b_released &&
                scene->shot_frame <
                    TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME
            ? &held : neutral;
        if (!scene_update_shot(scene, controls)) return false;
        ++updates;
        if (desired_selector == 1 &&
            (scene->shot_rim_rattle_selected ||
             scene->jump_rim_rattle.active ||
             scene->jump_rim_rattle.complete)) {
            return false;
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene_shot_state_valid(scene) || !scene_ownership_valid(scene) ||
        scene->state.score[shooting_team] !=
            (uint16_t)(score_before[shooting_team] + points) ||
        scene->state.score[scene_other_team(shooting_team)] !=
            score_before[scene_other_team(shooting_team)] ||
        scene->state.possession != scene_other_team(shooting_team)) {
        return false;
    }
    if (scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGM] != 1U ||
        scene->player_stats.counters[stat_team][stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PM] !=
            (points == 3U ? 1U : 0U))
        return false;
    if (desired_selector == 1 && scene->shot_rim_rattle_selected) {
        return false;
    }
    snapshot = *scene;
    if (scene_update_shot(scene, neutral) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        return false;
    }
    return true;
}

static bool scene_test_production_terminal_scenarios(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    static const int16_t exact_delta_x[] = {
        -320, -256, -192, -160, -128, -96, -64, -33,
        96, 128, 160, 192, 256, 320, -320, -256
    };
    static const int16_t exact_delta_y[] = {
        64, 64, 64, 64, 0, 0, 0, 0,
        0, 0, 0, 0, 64, 64, -80, -64
    };
    static const uint8_t miss_selectors[4] = {0U, 3U, 2U, 1U};
    static const TecmoGameplayShotRimRouteKind miss_kinds[4] = {
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9,
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9
    };
    static const unsigned miss_expected_updates[4] = {
        92U, 94U, 93U, 102U
    };
    TecmoControlFrame neutral;
    TecmoControlFrame held;
    TecmoControlFrame offball_controls;
    const TecmoControlFrame *offball_inputs[
        TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayScene malformed;
    TecmoGameplayScene snapshot;
    uint16_t score_before;
    uint16_t exact_score_before[TECMO_GAMEPLAY_TEAM_COUNT];
    unsigned corruption;
    unsigned guard;
    unsigned scenario;
    bool exact_make_found = false;
    TecmoGameplayTeam exact_team = TECMO_GAMEPLAY_TEAM_AWAY;
    uint8_t exact_stat_team = 0U;
    uint8_t exact_stat_roster = 0U;
    unsigned exact_seen_mask = 0U;
    scene_test_terminal_failure = 0U;
    scene_test_terminal_detail = 0U;
    if (scene == NULL || base_launch == NULL) {
        scene_test_terminal_failure = 1U;
        return false;
    }
    memset(&neutral, 0, sizeof(neutral));
    memset(&held, 0, sizeof(held));
    memset(&offball_controls, 0, sizeof(offball_controls));
    offball_inputs[0U] = &neutral;
    offball_inputs[1U] = &neutral;
    held.held.cancel = true;

    /* The exact captured make is a bound production launch: f0/p0/d1, point
       3, and the source-pinned schedule. Search only the immutable frame and
       available roster inputs; every active miss is ended before continuing. */
    for (unsigned team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT &&
             !exact_make_found; ++team) {
        for (uint8_t roster = 0U;
             roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM &&
             !exact_make_found; ++roster) {
            for (size_t vector = 0U;
                 vector < sizeof(exact_delta_x) / sizeof(exact_delta_x[0]) &&
                 !exact_make_found; ++vector) {
                for (uint32_t frame = 0U; frame < 256U &&
                         !exact_make_found; ++frame) {
                    if (!scene_test_prepare_matrix_fixture(
                            scene, base_launch, (TecmoGameplayTeam)team, roster,
                            exact_delta_x[vector], exact_delta_y[vector],
                            frame, 0U)) {
                        continue;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
                        exact_seen_mask |= 1U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->shot_points == 3U) {
                        exact_seen_mask |= 2U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->jump_profile ==
                            TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0) {
                        exact_seen_mask |= 4U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->jump_direction ==
                            TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1) {
                        exact_seen_mask |= 8U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->jump_family ==
                            TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0) {
                        exact_seen_mask |= 16U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->predicted_make_route) {
                        exact_seen_mask |= 32U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->shot_schedule ==
                            TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) {
                        exact_seen_mask |= 64U;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->predicted_make_route &&
                        scene->shot_points == 3U &&
                        scene->shot_schedule ==
                            TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT &&
                        scene->jump_family ==
                            TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 &&
                        scene->jump_profile ==
                            TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 &&
                        scene->jump_direction ==
                            TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1 &&
                        scene->shot_rim_route.selector == 1U &&
                        scene->shot_rim_route.kind ==
                            TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 &&
                        scene->shot_rim_route.source_target_cpu == 0xA7A9U &&
                        !scene->shot_rim_rattle_selected &&
                        scene_ownership_valid(scene)) {
                        exact_make_found = true;
                        exact_team = (TecmoGameplayTeam)team;
                        exact_stat_team = scene->shot_actor_team;
                        exact_stat_roster = scene->shot_actor_roster_index;
                    } else {
                        tecmo_gameplay_scene_end(scene);
                    }
                }
            }
        }
    }
    if (!exact_make_found) {
        scene_test_terminal_failure = 1000U + exact_seen_mask;
        return false;
    }
    if (exact_stat_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        exact_stat_roster >= TECMO_PLAYER_STATS_ROSTER_COUNT ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PA] != 1U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGM] != 0U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PM] != 0U) {
        scene_test_terminal_failure = 1001U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }

    exact_score_before[TECMO_GAMEPLAY_TEAM_AWAY] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    exact_score_before[TECMO_GAMEPLAY_TEAM_HOME] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    if (scene->shot_frame != 1U) {
        scene_test_terminal_failure = 9U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    if (scene->shot_start_position.x_q8 !=
            (scene->shot_actor_launch_position.x +
             (scene->shot_launch_facing_right ? 7 : -7)) * 256 ||
        scene->shot_start_position.y_q8 !=
            (scene->shot_actor_launch_position.y - 18) * 256) {
        scene_test_terminal_failure = 16U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    {
        TecmoGameplayScene unchanged;
        TecmoGameplayCourtCoordinate offball_before;
        uint8_t controller = scene->shot_controller;
        uint8_t offball_actor;
        unsigned movement_ticks;
        bool moved = false;
        if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
            scene->controlled_actor[controller] != scene->shot_actor) {
            scene_test_terminal_failure = 19U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        offball_controls.pressed.confirm = true;
        offball_controls.pressed.cancel = true;
        offball_controls.held.left =
            scene->actors[scene->shot_actor].position.x >= 384;
        offball_controls.held.right = !offball_controls.held.left;
        offball_inputs[controller] = &offball_controls;
        unchanged = *scene;
        if (!scene_update_shot_controlled_offball(scene, offball_inputs) ||
            memcmp(scene, &unchanged, sizeof(*scene)) != 0) {
            scene_test_terminal_failure = 190U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        offball_actor = scene->shot_actor <
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
            ? (uint8_t)((scene->shot_actor + 1U) %
                        TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT)
            : (uint8_t)(TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT +
                        ((scene->shot_actor + 1U -
                          TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) %
                         TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT));
        if (offball_actor == scene->shot_actor ||
            scene->actors[offball_actor].team !=
                scene->actors[scene->shot_actor].team) {
            scene_test_terminal_failure = 191U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        scene->controlled_actor[controller] = offball_actor;
        offball_controls.held.left =
            scene->actors[offball_actor].position.x >= 384;
        offball_controls.held.right = !offball_controls.held.left;
        offball_before = scene->actors[offball_actor].position;
        for (movement_ticks = 0U; movement_ticks < 16U; ++movement_ticks) {
            if (!scene_update_shot_controlled_offball(
                    scene, offball_inputs)) {
                scene_test_terminal_failure = 192U;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            if (scene->actors[offball_actor].position.x != offball_before.x ||
                scene->actors[offball_actor].position.y != offball_before.y) {
                moved = true;
                break;
            }
        }
        scene->controlled_actor[controller] = scene->shot_actor;
        if (!moved || scene->shot_frame != unchanged.shot_frame ||
            scene->shot_kind != unchanged.shot_kind ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene_pass_active(scene) ||
            scene->actors[scene->shot_actor].position.x !=
                unchanged.actors[unchanged.shot_actor].position.x ||
            scene->actors[scene->shot_actor].position.y !=
                unchanged.actors[unchanged.shot_actor].position.y) {
            scene_test_terminal_failure = 193U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        {
            TecmoGameplayCourtCoordinate cpu_before[
                TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
            TecmoGameplayCpuSteeringRouteMotionState *motion;
            uint8_t primary = scene->live_foundation.primary_actor;
            uint8_t defender = scene->live_foundation.defender_actor;
            uint8_t route_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            for (unsigned actor = 0U;
                 actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                cpu_before[actor] = scene->actors[actor].position;
                scene->live_foundation.play_state.actor_state[actor] = 0x06U;
                scene->live_foundation.play_state.wait_counter[actor] = 0xFFU;
                scene->live_foundation.play_state.route_motion[actor].active =
                    false;
                if (route_actor == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
                    actor != scene->shot_actor && actor != primary &&
                    actor != defender &&
                    actor != scene->controlled_actor[0U] &&
                    actor != scene->controlled_actor[1U]) {
                    route_actor = (uint8_t)actor;
                }
            }
            if (route_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
                scene_test_terminal_failure = 194U;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            motion = &scene->live_foundation.play_state
                          .route_motion[route_actor];
            memset(motion, 0, sizeof(*motion));
            motion->contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
            motion->horizontal_accumulator_q6 = (uint16_t)(
                (uint16_t)scene->actors[route_actor].position.x << 6U);
            motion->depth_accumulator_q6 = (uint16_t)(
                (uint16_t)scene->actors[route_actor].position.y << 6U);
            motion->horizontal_velocity_q6 =
                scene->actors[route_actor].position.x < 700 ? 64 : -64;
            motion->depth_velocity_q6 = 0;
            motion->remaining_timer = 4U;
            motion->active = true;
            scene->live_foundation.play_state.actor_state[route_actor] = 0x05U;
            scene->live_foundation.play_state.wait_counter[route_actor] = 0U;
            scene->live_foundation.play_state.target_object[route_actor] =
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
            scene->live_foundation.play_state.target_x[route_actor] =
                scene->actors[route_actor].position.x;
            scene->live_foundation.play_state.target_depth[route_actor] =
                scene->actors[route_actor].position.y;
            scene->live_foundation.source_target_valid[route_actor] = true;
            scene->live_foundation.source_direction_valid[route_actor] = false;
            scene->live_foundation.deferred[route_actor] = false;
            scene->live_foundation.deferred_reason[route_actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
            scene->cpu_actors[route_actor].decision_serial = 1U;
            scene->cpu_actors[route_actor].snapshot_fingerprint = 0U;
            scene->cpu_actors[route_actor].target_position =
                scene->actors[route_actor].position;
            scene->cpu_actors[route_actor].target_kind =
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET;
            scene->cpu_actors[route_actor].direction =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            scene->cpu_actors[route_actor].held_direction_bits =
                TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            scene->cpu_actors[route_actor].target_valid = true;
            scene->cpu_actors[route_actor].writes_direction = false;
            scene->state.clock_divider = 2U;
            memset(&scene->opcode16_frame_context, 0,
                   sizeof(scene->opcode16_frame_context));
            if (!tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets,
                    &scene->live_foundation) ||
                !scene_cpu_opcode16_workspace_capture(
                    scene, &scene->opcode16_frame_context) ||
                !scene_update_shot_cpu_offball(scene)) {
                scene_test_terminal_failure = 195U;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            if (scene->actors[route_actor].position.x ==
                    cpu_before[route_actor].x ||
                scene->actors[route_actor].position.y !=
                    cpu_before[route_actor].y ||
                !scene->live_foundation.play_state
                     .route_motion[route_actor].active ||
                scene->live_foundation.play_state
                        .route_motion[route_actor].remaining_timer != 3U ||
                scene->shot_frame != unchanged.shot_frame ||
                scene->shot_kind != unchanged.shot_kind ||
                scene_pass_active(scene)) {
                scene_test_terminal_failure = 196U;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            for (unsigned actor = 0U;
                 actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                if ((actor == scene->shot_actor || actor == primary ||
                     actor == defender) &&
                    (scene->actors[actor].position.x != cpu_before[actor].x ||
                     scene->actors[actor].position.y != cpu_before[actor].y)) {
                    scene_test_terminal_failure = 197U;
                    tecmo_gameplay_scene_end(scene);
                    return false;
                }
            }
        }
    }
    malformed = *scene;
    ++malformed.shot_start_position.x_q8;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 17U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    malformed = *scene;
    ++malformed.shot_start_position.x_q8;
    ++malformed.ball_position.x_q8;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 18U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    malformed = *scene;
    malformed.shot_rim_rattle_selected = true;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 14U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    malformed = *scene;
    malformed.shot_a0f3_origin_valid = true;
    malformed.shot_a0f3_origin_x = 1U;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 15U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    malformed = *scene;
    malformed.jump_entry_pose_index ^= 1U;
    malformed.actors[malformed.shot_actor].pose_index ^= 1U;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 10U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    while (scene->shot_frame < 8U) {
        if (!scene_update_shot(scene, &held)) {
            scene_test_terminal_failure = 11U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    /* Make the early released candidate pose-coherent so this specifically
       exercises the exact frame-1..8 release polarity invariant. */
    malformed = *scene;
    malformed.jump_b_released = true;
    malformed.jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
    malformed.actors[malformed.shot_actor].pose_index =
        malformed.jump_resolved_pose_index;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        scene_test_terminal_failure = 12U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    if (!scene_update_shot(scene, &neutral) || scene->shot_frame != 9U ||
        !scene->jump_b_released || !scene_shot_state_valid(scene) ||
        !scene_update_shot(scene, &neutral) || scene->shot_frame != 10U) {
        scene_test_terminal_failure = 13U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    for (corruption = 0U; corruption < 8U; ++corruption) {
        malformed = *scene;
        switch (corruption) {
        case 0U:
            malformed.jump_actor_state = 0xFFU;
            break;
        case 1U:
            malformed.jump_ball_state = 0xFFU;
            break;
        case 2U:
            malformed.jump_phase_counter ^= 1U;
            break;
        case 3U:
            malformed.jump_actor_altitude_q8 = 1U;
            break;
        case 4U:
            malformed.jump_actor_velocity_q8 ^= 1U;
            break;
        case 5U:
            malformed.actors[malformed.shot_actor].pose_index ^= 1U;
            break;
        case 6U:
            malformed.jump_entry_pose_index ^= 1U;
            malformed.actors[malformed.shot_actor].pose_index ^= 1U;
            break;
        default:
            malformed.ball_position.x_q8 += 1;
            break;
        }
        snapshot = malformed;
        if (scene_update_shot(&malformed, &neutral) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            scene_test_terminal_failure = 20U + corruption;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    score_before = scene->state.score[exact_team];
    guard = 0U;
    while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           guard < 130U) {
        if (!scene_update_shot(scene, &neutral)) {
            scene_test_terminal_failure = 30U + guard;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        ++guard;
        if (scene->shot_rim_rattle_selected ||
            scene->jump_rim_rattle.active ||
            scene->jump_rim_rattle.complete) {
            scene_test_terminal_failure = 40U + guard;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.score[exact_team] !=
            (uint16_t)(score_before + 3U) ||
        scene->state.score[scene_other_team(exact_team)] !=
            exact_score_before[scene_other_team(exact_team)] ||
        scene->state.possession != scene_other_team(exact_team) ||
        scene->ball_holder !=
            scene_first_actor_for_team(scene_other_team(exact_team)) ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team !=
            (uint8_t)scene_other_team(exact_team) ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PA] != 1U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_FGM] != 1U ||
        scene->player_stats.counters[exact_stat_team][exact_stat_roster][
            TECMO_PLAYER_STATS_COUNTER_THREE_PM] != 1U ||
        !scene_shot_state_valid(scene) || !scene_ownership_valid(scene)) {
        scene_test_terminal_failure = 170U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    snapshot = *scene;
    if (scene_update_shot(scene, &neutral) ||
        memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
        scene_test_terminal_failure = 171U;
        tecmo_gameplay_scene_end(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    /* Bound exact-miss routes cover ordinary A708/A8E9 tails and the normal
       A7A9 rattle through terminal claimant settlement. */
    for (scenario = 0U; scenario < 4U; ++scenario) {
        bool found = false;
        TecmoGameplayTeam miss_team = TECMO_GAMEPLAY_TEAM_AWAY;
        for (unsigned team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT &&
                 !found; ++team) {
            for (size_t vector = 0U;
                 vector < sizeof(exact_delta_x) / sizeof(exact_delta_x[0]) &&
                 !found; ++vector) {
                for (uint8_t roster = 0U;
                     roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM && !found;
                     ++roster) {
                    for (uint32_t frame = 0U; frame < 256U && !found;
                         ++frame) {
                        if (!scene_test_prepare_matrix_fixture(
                                scene, base_launch, (TecmoGameplayTeam)team,
                                roster, exact_delta_x[vector],
                                exact_delta_y[vector], frame, 0U)) {
                            continue;
                        }
                        if (scene->shot_kind ==
                                TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                            !scene->predicted_make_route &&
                            scene->shot_points == 3U &&
                            scene->shot_schedule ==
                                TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT &&
                            scene->jump_family ==
                                TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 &&
                            scene->jump_profile ==
                                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 &&
                            scene->jump_direction ==
                                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1 &&
                            scene->shot_rim_route.selector ==
                                miss_selectors[scenario] &&
                            scene->shot_rim_route.kind == miss_kinds[scenario] &&
                            scene->shot_rim_rattle_selected == (scenario == 3U) &&
                            scene_ownership_valid(scene)) {
                            found = true;
                            miss_team = (TecmoGameplayTeam)team;
                        } else {
                            tecmo_gameplay_scene_end(scene);
                        }
                    }
                }
            }
        }
        if (!found) {
            scene_test_terminal_failure = 200U + scenario;
            return false;
        }
        {
            TecmoGameplayCourtCoordinate endpoint;
            uint32_t trace = 0U;
            unsigned updates = 0U;
            uint8_t max_repeats = 0U;
            uint8_t miss_stat_team;
            uint8_t miss_stat_roster;
            bool tail_corruption_checked = false;
            bool saw_tgls_release = false;
            bool saw_tgls_first_tick = false;
            bool saw_authentic_rattle = false;
            bool saw_a8e9_normalized = false;
            bool saw_a9da_assignment = false;
            bool saw_b73a_assignment = false;
            bool release_corruption_checked = false;
            bool terminal_corruption_checked = false;
            uint16_t miss_score_before[TECMO_GAMEPLAY_TEAM_COUNT];
            miss_score_before[TECMO_GAMEPLAY_TEAM_AWAY] =
                scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
            miss_score_before[TECMO_GAMEPLAY_TEAM_HOME] =
                scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
            endpoint.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
            endpoint.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
            for (unsigned actor = 0U;
                 actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                scene->actors[actor].position.x = 650;
                scene->actors[actor].position.y = 230;
                scene->actors[actor].anchor = scene->actors[actor].position;
            }
            scene->actors[1U].position = endpoint;
            scene->actors[1U].anchor = endpoint;
            miss_team = (TecmoGameplayTeam)scene->actors[1U].team;
            miss_stat_team = scene->shot_actor_team;
            miss_stat_roster = scene->shot_actor_roster_index;
            if (miss_stat_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
                miss_stat_roster >= TECMO_PLAYER_STATS_ROSTER_COUNT ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PA] != 1U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_FGM] != 0U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PM] != 0U) {
                scene_test_terminal_failure = 209U + scenario;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            malformed = *scene;
            malformed.ball_position.x_q8 += 1;
            snapshot = malformed;
            if (scene_update_jump_miss(&malformed, &neutral) ||
                memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
                scene_test_terminal_failure = 210U + scenario;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            malformed = *scene;
            malformed.jump_ball_state = 0xFFU;
            snapshot = malformed;
            if (scene_update_jump_miss(&malformed, &neutral) ||
                memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
                scene_test_terminal_failure = 220U + scenario;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            if (scenario == 3U) {
                malformed = *scene;
                malformed.fixed_rng.serial = UINT32_MAX;
                snapshot = malformed;
                if (scene_update_jump_miss(&malformed, &neutral) ||
                    memcmp(&malformed, &snapshot,
                           sizeof(malformed)) != 0) {
                    scene_test_terminal_failure = 229U;
                    tecmo_gameplay_scene_end(scene);
                    return false;
                }
            }
            while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
                   updates < 130U) {
                if (!scene_update_shot(scene, &neutral)) {
                    scene_test_terminal_failure = 230U + scenario;
                    scene_test_terminal_detail = scene->shot_frame;
                    tecmo_gameplay_scene_end(scene);
                    return false;
                }
                ++updates;
                if (scenario == 3U && scene->shot_frame == 2U) {
                    saw_tgls_release = scene->shot_a0f3_origin_valid &&
                        scene->shot_a0f3_preflight_valid &&
                        scene->shot_a0f3_motion_valid &&
                        scene->shot_a0f3_raw_position_valid &&
                        scene->shot_a0f3_tick_count == 0U &&
                        scene->fixed_rng.last_callsite ==
                            TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD &&
                        scene->shot_a0f3_raw_x ==
                            scene->shot_a0f3_origin_x &&
                        scene->shot_a0f3_raw_depth ==
                            scene->shot_a0f3_origin_depth;
                    for (unsigned raw_corruption = 0U;
                         raw_corruption < 8U; ++raw_corruption) {
                        malformed = *scene;
                        switch (raw_corruption) {
                        case 0U: malformed.shot_a0f3_origin_x ^= 1U; break;
                        case 1U:
                            malformed.shot_a0f3_preflight_raw_006a ^= 1U;
                            break;
                        case 2U:
                            malformed.shot_a0f3_result.velocity_x_q6 ^= 1U;
                            break;
                        case 3U:
                            malformed.shot_a0f3_motion.accumulator_x_q6 ^= 1U;
                            break;
                        case 4U: malformed.shot_a0f3_raw_x ^= 1U; break;
                        case 5U: ++malformed.shot_a0f3_tick_count; break;
                        case 6U:
                            ++malformed.shot_a0f3_release_c05d_serial;
                            break;
                        default: malformed.fixed_rng.contract_tag ^= 1U; break;
                        }
                        snapshot = malformed;
                        if (scene_update_jump_miss(&malformed, &neutral) ||
                            memcmp(&malformed, &snapshot,
                                   sizeof(malformed)) != 0) {
                            scene_test_terminal_failure = 228U;
                            scene_test_terminal_detail = raw_corruption;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                    }
                    release_corruption_checked = true;
                } else if (scenario == 3U && scene->shot_frame == 3U) {
                    saw_tgls_first_tick =
                        scene->shot_a0f3_result.duration_051e_0513 == 0U
                            ? scene->shot_a0f3_tick_count == 0U
                            : scene->shot_a0f3_tick_count == 1U;
                }
                if (scenario == 3U && scene->jump_rim_rattle.active) {
                    saw_authentic_rattle =
                        (uint16_t)scene->jump_rim_rattle
                            .saved_horizontal_velocity_q6 ==
                            scene->shot_a0f3_motion.velocity_x_q6 &&
                        (uint16_t)scene->jump_rim_rattle
                            .saved_vertical_velocity_q6 ==
                            scene->shot_a0f3_motion.velocity_depth_q6;
                }
                if (scenario == 3U &&
                    scene->shot_a8e9_normalized_valid) {
                    uint8_t chosen =
                        scene->shot_a9da_result.chosen_actor_002d;
                    saw_a8e9_normalized = true;
                    if (!terminal_corruption_checked) {
                        if (!scene->shot_a9da_assignment_valid ||
                            !scene->shot_a9da_opcode13_pending ||
                            chosen >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                            scene->shot_a9da_result.outcome !=
                                TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ASSIGNED ||
                            scene->shot_a9da_latch.producer !=
                                TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA ||
                            scene->shot_a9da_latch.raw_x_038d_038e !=
                                scene->shot_a9da_result
                                    .projected_x_038d_038e ||
                            scene->shot_a9da_latch.raw_depth_038f_0390 !=
                                scene->shot_a9da_result
                                    .projected_depth_038f_0390 ||
                            scene->shot_a9da_output
                                    .stream_offset[chosen] != 0x002DU) {
                            scene_test_terminal_failure = 226U;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        for (unsigned raw_corruption = 0U;
                             raw_corruption < 7U; ++raw_corruption) {
                            malformed = *scene;
                            if (raw_corruption == 0U) {
                                malformed.shot_a8e9_raw_006a ^= 1U;
                            } else if (raw_corruption == 1U) {
                                malformed.shot_a8e9_normalized
                                    .raw_vx_04f1_04fc ^= 1U;
                            } else if (raw_corruption == 2U) {
                                malformed.fixed_rng.serial = UINT32_MAX;
                            } else if (raw_corruption == 3U) {
                                malformed.shot_a9da_input.ball_x ^= 1U;
                            } else if (raw_corruption == 4U) {
                                malformed.shot_a9da_output
                                    .stream_offset[chosen] ^= 1U;
                            } else if (raw_corruption == 5U) {
                                malformed.shot_a9da_result
                                    .projected_x_038d_038e ^= 1U;
                            } else {
                                ++malformed.shot_a9da_latch.write_serial;
                            }
                            snapshot = malformed;
                            if (scene_update_jump_miss(
                                    &malformed, &neutral) ||
                                memcmp(&malformed, &snapshot,
                                       sizeof(malformed)) != 0) {
                                scene_test_terminal_failure = 227U;
                                scene_test_terminal_detail = raw_corruption;
                                tecmo_gameplay_scene_end(scene);
                                return false;
                            }
                        }
                        memset(&scene->opcode16_frame_context, 0,
                               sizeof(scene->opcode16_frame_context));
                        if (!scene_cpu_opcode16_workspace_capture(
                                scene, &scene->opcode16_frame_context)) {
                            scene_test_terminal_failure = 225U;
                            scene_test_terminal_detail = 1U;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        if (!scene_update_shot_cpu_offball(scene)) {
                            scene_test_terminal_failure = 225U;
                            scene_test_terminal_detail = 2U;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        if (scene->shot_a9da_opcode13_pending ||
                            scene->live_foundation.play_state
                                    .stream_offset[chosen] != 0x0032U ||
                            scene->live_foundation.last_step_offset[chosen] !=
                                0x0032U ||
                            !scene->live_foundation
                                 .source_raw_target_valid[chosen] ||
                            (uint16_t)scene->live_foundation.play_state
                                    .target_x[chosen] !=
                                scene->shot_a9da_latch.raw_x_038d_038e ||
                            (uint16_t)scene->live_foundation.play_state
                                    .target_depth[chosen] !=
                                scene->shot_a9da_latch
                                    .raw_depth_038f_0390) {
                            scene_test_terminal_failure = 225U;
                            scene_test_terminal_detail =
                                scene->shot_a9da_opcode13_pending ? 3U :
                                scene->live_foundation.play_state
                                        .stream_offset[chosen] != 0x0032U ? 4U :
                                scene->live_foundation.last_step_offset[chosen] !=
                                        0x0032U ? 5U :
                                !scene->live_foundation
                                     .source_raw_target_valid[chosen] ? 6U :
                                (uint16_t)scene->live_foundation.play_state
                                        .target_x[chosen] !=
                                    scene->shot_a9da_latch.raw_x_038d_038e ? 7U :
                                8U;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        saw_a9da_assignment = true;
                        terminal_corruption_checked = true;
                    }
                }
                if (scenario == 3U &&
                    scene->shot_b73a_assignment_applied &&
                    !saw_b73a_assignment) {
                    if (scene->shot_b73a_raw_0499 != 0U ||
                        scene->shot_b73a_handler_cpu != 0xB6E5U ||
                        scene->shot_b73a_opcode20_actor_mask == 0U ||
                        scene->jump_ball_state != 0x10U ||
                        !scene->shot_a9da_result
                            .state10_and_b3dd_committed ||
                        scene->shot_a9da_result.raw_0067 != 0xF0U ||
                        scene->shot_a9da_result.raw_0068 != 0x02U ||
                        !scene->a023_latch_frame_context.available ||
                        scene->a023_latch_frame_context.latch.producer_kind !=
                            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721) {
                        scene_test_terminal_failure = 239U;
                        scene_test_terminal_detail = scene->shot_frame;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    saw_b73a_assignment = true;
                }
                trace = trace * 16777619U ^
                    (uint32_t)scene->ball_position.x_q8;
                trace = trace * 16777619U ^
                    (uint32_t)scene->ball_position.y_q8;
                if (!tail_corruption_checked &&
                    scene->shot_rim_tail_active) {
                    malformed = *scene;
                    malformed.shot_rim_tail_duration++;
                    malformed.shot_duration++;
                    snapshot = malformed;
                    if (scene_update_shot(&malformed, &neutral) ||
                        memcmp(&malformed, &snapshot,
                               sizeof(malformed)) != 0) {
                        scene_test_terminal_failure = 235U + scenario;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    malformed = *scene;
                    malformed.shot_rim_tail_base_frame++;
                    malformed.shot_frame++;
                    malformed.shot_duration++;
                    snapshot = malformed;
                    if (scene_update_shot(&malformed, &neutral) ||
                        memcmp(&malformed, &snapshot,
                               sizeof(malformed)) != 0) {
                        scene_test_terminal_failure = 236U + scenario;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    tail_corruption_checked = true;
                }
                if (scene->jump_rim_rattle_audio_repeats > max_repeats) {
                    max_repeats = scene->jump_rim_rattle_audio_repeats;
                }
            }
            if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
                scene->state.possession != miss_team ||
                scene->ball_holder != 1U || trace == 0U ||
                scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
                    miss_score_before[TECMO_GAMEPLAY_TEAM_AWAY] ||
                scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] !=
                    miss_score_before[TECMO_GAMEPLAY_TEAM_HOME] ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PA] != 1U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_FGM] != 0U ||
                scene->player_stats.counters[miss_stat_team][miss_stat_roster][
                    TECMO_PLAYER_STATS_COUNTER_THREE_PM] != 0U ||
                updates != miss_expected_updates[scenario] ||
                (scenario == 3U && max_repeats != 3U) ||
                (scenario == 3U &&
                  (!saw_tgls_release || !saw_tgls_first_tick ||
                   !saw_authentic_rattle || !saw_a8e9_normalized ||
                   !saw_a9da_assignment || !saw_b73a_assignment ||
                  !release_corruption_checked ||
                  !terminal_corruption_checked)) ||
                (scenario != 3U && !tail_corruption_checked) ||
                !scene_shot_state_valid(scene) ||
                !scene_ownership_valid(scene)) {
                scene_test_terminal_failure = 240U + scenario;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            snapshot = *scene;
            if (scene_update_shot(scene, &neutral) ||
                memcmp(scene, &snapshot, sizeof(*scene)) != 0) {
                scene_test_terminal_failure = 250U + scenario;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
        }
        tecmo_gameplay_scene_end(scene);
    }

    /* A normal jump miss may finish with every actor outside the strict
       $B73E-$B87C claimant envelope. Retain the grounded ball with no owner,
       prove that the class-3 chase visibly moves through legal TGMO states,
       and emit exactly one source-shaped event only after the exact scan
       admits a claimant. */
    {
        TecmoGameplayTeam fallback_shooting_team = TECMO_GAMEPLAY_TEAM_AWAY;
        TecmoGameplayTeam fallback_next_team = TECMO_GAMEPLAY_TEAM_HOME;
        const TecmoControlFrame *loose_controls[
            TECMO_GAMEPLAY_CONTROLLER_COUNT] = {&neutral, &neutral};
        TecmoControlFrame chase_control[TECMO_GAMEPLAY_CONTROLLER_COUNT];
        const TecmoControlFrame *chase_control_ptr[
            TECMO_GAMEPLAY_CONTROLLER_COUNT];
        TecmoGameplayCourtCoordinate chase_start;
        TecmoGameplayCourtCoordinate controlled_start;
        uint8_t chase_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        uint8_t controlled_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        size_t controlled_pad = TECMO_GAMEPLAY_CONTROLLER_COUNT;
        uint8_t expected_team = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        uint32_t fallback_serial = 0U;
        unsigned fallback_updates = 0U;
        bool chase_moved = false;
        bool fallback_found = false;
        for (unsigned team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT &&
                 !fallback_found; ++team) {
            for (uint8_t roster = 0U;
                 roster < TECMO_TEAM_DATA_PLAYERS_PER_TEAM &&
                 !fallback_found; ++roster) {
                for (uint32_t frame = 0U; frame < 256U; ++frame) {
                    if (!scene_test_prepare_matrix_fixture(
                            scene, base_launch, (TecmoGameplayTeam)team,
                            roster, team == 0U ? -160 : 160, 0, frame, 0U)) {
                        continue;
                    }
                    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                        scene->shot_outcome ==
                            TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
                        fallback_found = true;
                        break;
                    }
                    tecmo_gameplay_scene_end(scene);
                }
            }
        }
        if (!fallback_found) {
            scene_test_terminal_failure = 260U;
            return false;
        }
        fallback_shooting_team =
            (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
        fallback_next_team = scene_other_team(fallback_shooting_team);
        fallback_serial = scene->claimant_settlement_trace.event_serial;
        for (uint8_t actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            scene->actors[actor].position.x = (int16_t)(340 + actor * 3);
            scene->actors[actor].position.y = 198;
            scene->actors[actor].anchor = scene->actors[actor].position;
        }
        if (!scene_test_prepare_terminal_holder(
                scene, scene_first_actor_for_team(fallback_next_team), true)) {
            scene_test_terminal_failure = 261U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               fallback_updates < 140U) {
            if (!scene_update_shot(scene, &neutral)) {
                scene_test_terminal_failure = 262U;
                scene_test_terminal_detail = scene->shot_frame;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            ++fallback_updates;
        }
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
            scene->state.possession != fallback_shooting_team ||
            scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            !scene->loose_ball_state.active ||
            scene->claimant_settlement_trace.event_serial != fallback_serial ||
            !scene_shot_state_valid(scene) || !scene_ownership_valid(scene)) {
            scene_test_terminal_failure = 263U;
            scene_test_terminal_detail = fallback_updates;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        for (unsigned relation_case = 0U; relation_case < 2U;
             ++relation_case) {
            TecmoGameplayTeam claimant_team = relation_case == 0U
                ? fallback_shooting_team : fallback_next_team;
            uint8_t claimant_actor = scene_first_actor_for_team(claimant_team);
            malformed = *scene;
            malformed.actors[claimant_actor].position.x = (int16_t)(
                malformed.ball_position.x_q8 / 256);
            malformed.actors[claimant_actor].position.y = (int16_t)(
                malformed.ball_position.y_q8 / 256);
            malformed.actors[claimant_actor].anchor =
                malformed.actors[claimant_actor].position;
            if (!scene_update_loose_ball(&malformed, loose_controls) ||
                malformed.loose_ball_state.active ||
                malformed.ball_holder != claimant_actor ||
                malformed.state.possession != claimant_team ||
                malformed.claimant_settlement_trace.event_serial !=
                    fallback_serial + 1U ||
                !scene_ownership_valid(&malformed)) {
                scene_test_terminal_failure = 271U + relation_case;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
        }
        malformed = *scene;
        malformed.state.clock_minutes = 0U;
        malformed.state.clock_seconds = 1U;
        malformed.state.clock_divider = 1U;
        if (!tecmo_gameplay_scene_update(
                &malformed, &neutral, &neutral) ||
            malformed.state.phase !=
                TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
            malformed.loose_ball_state.active ||
            malformed.ball_holder !=
                scene->live_foundation.primary_actor ||
            malformed.state.possession != fallback_shooting_team ||
            malformed.claimant_settlement_trace.event_serial !=
                fallback_serial ||
            !scene_ownership_valid(&malformed)) {
            scene_test_terminal_failure = 264U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        chase_actor = scene->loose_ball_state.chase_actor;
        for (size_t controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (scene->controlled_actor[controller] <
                    TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
                controlled_pad = controller;
                controlled_actor = scene->controlled_actor[controller];
                break;
            }
        }
        if (chase_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            controlled_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            chase_actor == controlled_actor) {
            scene_test_terminal_failure = 269U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        malformed = *scene;
        memset(chase_control, 0, sizeof(chase_control));
        chase_control_ptr[0U] = &chase_control[0U];
        chase_control_ptr[1U] = &chase_control[1U];
        malformed.actors[controlled_actor].position.x = (int16_t)(
            malformed.ball_position.x_q8 / 256 + 30);
        malformed.actors[controlled_actor].position.y = (int16_t)(
            malformed.ball_position.y_q8 / 256);
        if (!scene_actor_world_position_valid(
                &malformed.actors[controlled_actor])) {
            malformed.actors[controlled_actor].position.x = (int16_t)(
                malformed.ball_position.x_q8 / 256 - 30);
            chase_control[controlled_pad].held.right = true;
        } else {
            chase_control[controlled_pad].held.left = true;
        }
        malformed.actors[controlled_actor].anchor =
            malformed.actors[controlled_actor].position;
        controlled_start = malformed.actors[controlled_actor].position;
        if (!scene_update_loose_ball(&malformed, chase_control_ptr) ||
            !scene_update_loose_ball(&malformed, chase_control_ptr) ||
            !scene_update_loose_ball(&malformed, chase_control_ptr) ||
            !scene_update_loose_ball(&malformed, chase_control_ptr) ||
            malformed.actors[controlled_actor].position.x ==
                controlled_start.x ||
            !malformed.loose_ball_state.active ||
            malformed.claimant_settlement_trace.event_serial !=
                fallback_serial) {
            scene_test_terminal_failure = 270U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        expected_team = scene->actors[chase_actor].team;
        scene->actors[chase_actor].position.x = (int16_t)(
            scene->ball_position.x_q8 / 256 + 12);
        scene->actors[chase_actor].position.y = (int16_t)(
            scene->ball_position.y_q8 / 256);
        if (!scene_actor_world_position_valid(&scene->actors[chase_actor])) {
            scene->actors[chase_actor].position.x = (int16_t)(
                scene->ball_position.x_q8 / 256 - 12);
        }
        scene->actors[chase_actor].anchor =
            scene->actors[chase_actor].position;
        chase_start = scene->actors[chase_actor].position;
        malformed = *scene;
        malformed.actors[chase_actor].movement_action_state = 0xFFU;
        snapshot = malformed;
        if (scene_update_loose_ball(&malformed, loose_controls) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            scene_test_terminal_failure = 265U;
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        for (guard = 0U; guard < 32U &&
             scene->loose_ball_state.active; ++guard) {
            if (!scene_update_loose_ball(scene, loose_controls)) {
                scene_test_terminal_failure = 266U;
                scene_test_terminal_detail = guard;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
            chase_moved = chase_moved ||
                scene->actors[chase_actor].position.x != chase_start.x ||
                scene->actors[chase_actor].position.y != chase_start.y;
            if (scene->loose_ball_state.active &&
                scene->claimant_settlement_trace.event_serial !=
                    fallback_serial) {
                scene_test_terminal_failure = 267U;
                tecmo_gameplay_scene_end(scene);
                return false;
            }
        }
        if (scene->loose_ball_state.active || !chase_moved ||
            scene->ball_holder != chase_actor ||
            scene->state.possession != (TecmoGameplayTeam)expected_team ||
            scene->claimant_settlement_trace.event_serial !=
                fallback_serial + 1U ||
            !scene->claimant_settlement_trace.valid ||
            !scene_loose_ball_state_valid(scene) ||
            !scene_ownership_valid(scene)) {
            scene_test_terminal_failure = 268U;
            scene_test_terminal_detail = guard |
                (scene->loose_ball_state.active ? 0x0100U : 0U) |
                (!chase_moved ? 0x0200U : 0U) |
                (scene->ball_holder != chase_actor ? 0x0400U : 0U) |
                (scene->state.possession != (TecmoGameplayTeam)expected_team
                     ? 0x0800U : 0U) |
                (scene->claimant_settlement_trace.event_serial !=
                     fallback_serial + 1U ? 0x1000U : 0U) |
                (!scene->claimant_settlement_trace.valid ? 0x2000U : 0U) |
                (!scene_loose_ball_state_valid(scene) ? 0x4000U : 0U) |
                (!scene_ownership_valid(scene) ? 0x8000U : 0U);
            tecmo_gameplay_scene_end(scene);
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }

    /* A bound ordinary jump make must use the native approximation schedule
       and explicitly settle a two-point attempt exactly once. */
    if (!scene_test_run_bound_approximate_make_terminal(
            scene, base_launch, &neutral, -1)) {
        scene_test_terminal_failure = 300U;
        if (scene->active) tecmo_gameplay_scene_end(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    /* A low2==1/A7A9 raw selector is a miss-route identity only.  A bound
       native-approximate MAKE with that selector must score normally without
       activating the rattle object or tail. */
    if (!scene_test_run_bound_approximate_make_terminal(
            scene, base_launch, &neutral, 1)) {
        scene_test_terminal_failure = 301U;
        if (scene->active) tecmo_gameplay_scene_end(scene);
        return false;
    }
    tecmo_gameplay_scene_end(scene);

    /* Close numeric identities each receive a terminal MAKE. The exhaustive
       3x2x8 selector matrix above proves all selector dimensions; these
       cases specifically prove score/settlement ownership at the boundary. */
    for (unsigned variant = 0U; variant < 3U; ++variant) {
        if (!scene_test_find_bound_close_terminal_case(
                scene, base_launch, variant,
                TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE, -1)) {
            scene_test_terminal_failure = 3100U + variant;
            if (scene->active) tecmo_gameplay_scene_end(scene);
            return false;
        }
        if (!scene_test_run_bound_close_terminal_case(
                scene, TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE, variant, -1,
                &neutral)) {
            scene_test_terminal_failure = 3110U + variant;
            if (scene->active) tecmo_gameplay_scene_end(scene);
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }

    /* The four raw miss routes are distributed over the three numeric close
       identities so every identity has a terminal MISS and every source
       selector (A708 0/3, A7A9, A8E9) is exercised in production. */
    {
        static const unsigned miss_variant[4] = {0U, 0U, 1U, 2U};
        static const int miss_selector[4] = {0, 3, 1, 2};
        for (unsigned miss_case = 0U; miss_case < 4U; ++miss_case) {
            if (!scene_test_find_bound_close_terminal_case(
                    scene, base_launch, miss_variant[miss_case],
                    TECMO_GAMEPLAY_SHOT_OUTCOME_MISS,
                    miss_selector[miss_case])) {
                scene_test_terminal_failure = 3200U + miss_case;
                if (scene->active) tecmo_gameplay_scene_end(scene);
                return false;
            }
            if (!scene_test_run_bound_close_terminal_case(
                    scene, TECMO_GAMEPLAY_SHOT_OUTCOME_MISS,
                    miss_variant[miss_case], miss_selector[miss_case],
                    &neutral)) {
                scene_test_terminal_failure = 3210U + miss_case;
                if (scene->active) tecmo_gameplay_scene_end(scene);
                return false;
            }
            tecmo_gameplay_scene_end(scene);
        }
    }
    return true;
}

static unsigned scene_test_jump_orientation_failure;

static bool scene_test_home_a7a9_orientation_one(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    static const int approaches[] = {64, 96, 128, 160, 220, 300};
    static const int16_t heights[] = {95, 105, 120, 143, 160, 180, 200};
    TecmoGameplaySceneLaunch launch;
    TecmoGameplayCourtCoordinate hoop;
    TecmoGameplayCourtCoordinate position;
    TecmoControlFrame neutral;
    size_t approach;
    size_t height;
    uint32_t frame;
    unsigned updates;
    scene_test_jump_orientation_failure = 0U;
    if (scene == NULL || base_launch == NULL) return false;
    launch = *base_launch;
    launch.starter_binding_bound = true;
    for (size_t side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        for (size_t local = 0U;
             local < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT; ++local) {
            launch.starter_roster_index[side][local] = (uint8_t)local;
        }
    }
    launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = false;
    memset(&neutral, 0, sizeof(neutral));
    for (frame = 0U; frame < 256U; ++frame) {
        for (approach = 0U;
             approach < sizeof(approaches) / sizeof(approaches[0]);
             ++approach) {
            for (height = 0U;
                 height < sizeof(heights) / sizeof(heights[0]); ++height) {
                tecmo_gameplay_scene_test_set_skip_pretip(true);
                if (!tecmo_gameplay_scene_launch(scene, &launch)) {
                    scene_test_jump_orientation_failure = 1U;
                    tecmo_gameplay_scene_test_set_skip_pretip(false);
                    return false;
                }
                tecmo_gameplay_scene_test_set_skip_pretip(false);
                if (!scene_handoff_possession(
                        scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
                    scene_test_jump_orientation_failure = 2U;
                    return false;
                }
                hoop = scene->orientation_state.offensive_hoop;
                position.x = (int16_t)((int)hoop.x - approaches[approach]);
                position.y = heights[height];
                if (!scene_actor_coordinate_valid(&position)) {
                    scene_test_jump_orientation_failure = 3U;
                    return false;
                }
                for (size_t actor = 0U;
                     actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                    scene->actors[actor].position.x = 120;
                    scene->actors[actor].position.y = 230;
                    scene->actors[actor].anchor = scene->actors[actor].position;
                    scene->actors[actor].movement_boundary_latched = false;
                }
                scene->actors[5U].position = position;
                scene->actors[5U].anchor = position;
                scene->actors[5U].facing_right = position.x > hoop.x;
                scene->state.clock_minutes = 1U;
                scene->state.clock_seconds = 0U;
                scene->state.clock_divider = 1U;
                scene->state.shot_clock = 12U;
                scene->frame = frame;
                if (!scene_attach_ball(scene) ||
                    !scene_sync_live_foundation(scene) ||
                    !scene_start_shot_actor(scene, 1U, 5U)) {
                    tecmo_gameplay_scene_end(scene);
                    continue;
                }
                if (scene->shot_kind !=
                        TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
                    scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
                    scene->shot_rim_route.selector != 1U ||
                    scene->shot_rim_route.kind !=
                        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 ||
                    !scene->shot_rim_rattle_selected) {
                    tecmo_gameplay_scene_end(scene);
                    continue;
                }
                updates = 0U;
                while (!scene->jump_rim_rattle.active &&
                       scene->shot_kind !=
                           TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
                       updates < 130U) {
                    if (!scene_update_jump_miss(scene, &neutral)) {
                        scene_test_jump_orientation_failure = 4U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    ++updates;
                }
                {
                    bool incoming_negative = scene->jump_rim_rattle
                        .saved_horizontal_velocity_q6 < 0;
                    int16_t expected_x = (int16_t)(
                        scene->shot_resolution.rim_rattle
                            .orientation_start_x[1U] +
                        (incoming_negative ? 1 : -1));
                    size_t expected_render = incoming_negative ? 7U : 4U;
                if (!scene->jump_rim_rattle.active ||
                    !scene_update_jump_miss(scene, &neutral) ||
                    scene->jump_rim_rattle.orientation != 1U ||
                    scene->jump_rim_rattle.x != expected_x ||
                    scene->jump_rim_rattle.render_script_address !=
                        scene->shot_resolution.rim_rattle
                            .render_script_addresses[expected_render]) {
                    scene_test_jump_orientation_failure = 5U + updates;
                    tecmo_gameplay_scene_end(scene);
                    return false;
                }
                }
                {
                    TecmoGameplayScene orientation_probe = *scene;
                    TecmoGameplayScene endpoint_probe = *scene;
                    TecmoGameplayScene snapshot;
                    uint8_t captured_orientation;
                    orientation_probe.orientation_state.attack_direction = 0U;
                    if (!scene_shot_state_valid(&orientation_probe) ||
                        !scene_shot_captured_rattle_orientation(
                            &orientation_probe, &captured_orientation) ||
                        captured_orientation != 1U) {
                        scene_test_jump_orientation_failure = 601U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    snapshot = orientation_probe;
                    if (scene_update_jump_miss(&orientation_probe, &neutral) ||
                        memcmp(&orientation_probe, &snapshot,
                               sizeof(orientation_probe)) != 0) {
                        scene_test_jump_orientation_failure = 602U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    endpoint_probe.shot_end_position.x_q8 += 1;
                    snapshot = endpoint_probe;
                    if (scene_update_jump_miss(&endpoint_probe, &neutral) ||
                        memcmp(&endpoint_probe, &snapshot,
                               sizeof(endpoint_probe)) != 0) {
                        scene_test_jump_orientation_failure = 7U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                }
                while (scene->shot_frame <
                       TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME + 1U) {
                    if (!scene_update_jump_miss(scene, &neutral)) {
                        scene_test_jump_orientation_failure = 8U + updates;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    ++updates;
                }
                {
                    TecmoGameplayScene corrupted = *scene;
                    TecmoGameplayScene snapshot;
                    memset(&corrupted.jump_rim_rattle, 0,
                           sizeof(corrupted.jump_rim_rattle));
                    snapshot = corrupted;
                    if (scene_update_jump_miss(&corrupted, &neutral) ||
                        memcmp(&corrupted, &snapshot,
                               sizeof(corrupted)) != 0) {
                        scene_test_jump_orientation_failure = 9U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                    corrupted = *scene;
                    corrupted.jump_rim_rattle.render_script_address ^= 1U;
                    snapshot = corrupted;
                    if (scene_update_jump_miss(&corrupted, &neutral) ||
                        memcmp(&corrupted, &snapshot,
                               sizeof(corrupted)) != 0) {
                        scene_test_jump_orientation_failure = 10U;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                }
                /* Continue the same bound JUMP miss through the complete
                   captured 103-update A7A9 route.  The opponent is placed at
                   the captured endpoint only after the orientation identity
                   has been observed, so this proves terminal settlement as
                   well as the orientation-1 snap/render contract. */
                {
                    TecmoGameplayCourtCoordinate endpoint;
                    TecmoGameplayScene before_terminal;
                    endpoint.x = (int16_t)(
                        scene->shot_end_position.x_q8 / 256);
                    endpoint.y = (int16_t)(
                        scene->shot_end_position.y_q8 / 256);
                    for (size_t actor = 0U;
                         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
                        scene->actors[actor].position.x = 650;
                        scene->actors[actor].position.y = 230;
                        scene->actors[actor].anchor =
                            scene->actors[actor].position;
                    }
                    scene->actors[0U].position = endpoint;
                    scene->actors[0U].anchor = endpoint;
                    while (scene->shot_kind !=
                               TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
                           updates < 130U) {
                        if (!scene_update_jump_miss(scene, &neutral)) {
                            scene_test_jump_orientation_failure =
                                100U + updates;
                            tecmo_gameplay_scene_end(scene);
                            return false;
                        }
                        ++updates;
                    }
                    before_terminal = *scene;
                    if (scene->shot_kind !=
                            TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
                        scene->state.possession !=
                            TECMO_GAMEPLAY_TEAM_AWAY ||
                        scene->ball_holder != 0U ||
                        !scene_shot_state_valid(scene) ||
                        scene_update_jump_miss(scene, &neutral) ||
                        memcmp(scene, &before_terminal,
                               sizeof(*scene)) != 0) {
                        scene_test_jump_orientation_failure = 200U + updates;
                        tecmo_gameplay_scene_end(scene);
                        return false;
                    }
                }
                tecmo_gameplay_scene_end(scene);
                return true;
            }
        }
    }
    scene_test_jump_orientation_failure = 1000U;
    return false;
}

static unsigned scene_test_claimant_failure;

static bool scene_test_close_rattle_claimant_cases(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    TecmoControlFrame neutral;
    TecmoGameplayScene before_late_failure;
    TecmoGameplayScene corrupted;
    TecmoGameplayCourtCoordinate endpoint;
    unsigned update;
    scene_test_claimant_failure = 0U;
    memset(&neutral, 0, sizeof(neutral));

    /* Same-team source order wins over a geometrically nearer later actor. */
    if (!scene_test_find_close_route(scene, base_launch, 1U) ||
        !scene_test_advance_to_rim_tail(scene)) {
        scene_test_claimant_failure = 1U;
        return false;
    }
    endpoint.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
    endpoint.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
    for (unsigned actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        scene->actors[actor].position.x = 650;
        scene->actors[actor].position.y = 230;
        scene->actors[actor].anchor = scene->actors[actor].position;
    }
    scene->actors[1U].position.x = (int16_t)(endpoint.x + 10);
    scene->actors[1U].position.y = (int16_t)(endpoint.y + 6);
    scene->actors[1U].anchor = scene->actors[1U].position;
    scene->actors[2U].position = endpoint;
    scene->actors[2U].anchor = endpoint;
    /* Claimant movement is not a launch retarget.  The immutable launch
       snapshot keeps this otherwise coherent active shot valid; changing
       either captured geometry or its stored selector delta rejects before
       any rattle/audio/settlement mutation. */
    corrupted = *scene;
    ++corrupted.actors[scene->shot_actor].position.x;
    if (!scene_shot_state_valid(&corrupted)) {
        scene_test_claimant_failure = 2U;
        return false;
    }
    if (!scene_update_shot(&corrupted, &neutral)) {
        scene_test_claimant_failure = 3U;
        return false;
    }
    corrupted = *scene;
    ++corrupted.shot_target_delta_x;
    before_late_failure = corrupted;
    if (scene_update_shot(&corrupted, &neutral) ||
        memcmp(&corrupted, &before_late_failure,
               sizeof(corrupted)) != 0) {
        scene_test_claimant_failure = 3U;
        return false;
    }
    corrupted = *scene;
    ++corrupted.shot_actor_launch_position.x;
    before_late_failure = corrupted;
    if (scene_update_shot(&corrupted, &neutral) ||
        memcmp(&corrupted, &before_late_failure,
               sizeof(corrupted)) != 0) {
        scene_test_claimant_failure = 4U;
        return false;
    }
    for (update = 1U; update <= TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
         ++update) {
        if (!scene_update_shot(scene, &neutral) ||
            scene->jump_ball_altitude_q8 != 0x3800U ||
            scene->jump_rim_rattle_audio_repeats !=
                (update / 4U > 3U ? 3U : update / 4U) ||
            (update < TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES &&
             (!scene->jump_rim_rattle.active ||
              scene->jump_rim_rattle.complete)) ||
            (update == TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES &&
             (scene->jump_rim_rattle.active ||
              !scene->jump_rim_rattle.complete))) {
            scene_test_claimant_failure = 5U + update;
            return false;
        }
    }
    if (!scene_update_shot(scene, &neutral) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != 1U) {
        scene_test_claimant_failure = 22U;
        return false;
    }

    /* An earlier ineligible actor is skipped for a later opponent. */
    if (!scene_test_find_close_route(scene, base_launch, 1U) ||
        !scene_test_advance_to_rim_tail(scene)) {
        scene_test_claimant_failure = 23U;
        return false;
    }
    endpoint.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
    endpoint.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
    for (unsigned actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        scene->actors[actor].position.x = 650;
        scene->actors[actor].position.y = 230;
        scene->actors[actor].anchor = scene->actors[actor].position;
    }
    scene->actors[5U].position = endpoint;
    scene->actors[5U].anchor = endpoint;
    for (update = 0U; update < TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
         ++update) {
        if (!scene_update_shot(scene, &neutral)) {
            scene_test_claimant_failure = 25U + update;
            return false;
        }
    }
    if (!scene_update_shot(scene, &neutral) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 5U) {
        scene_test_claimant_failure = 24U + update;
        return false;
    }

    /* Corruption at the rattle late step and no-claimant settlement both
       reject without committing any scene or output mutation. */
    if (!scene_test_find_close_route(scene, base_launch, 1U) ||
        !scene_test_advance_to_rim_tail(scene)) {
        scene_test_claimant_failure = 40U;
        return false;
    }
    corrupted = *scene;
    corrupted.jump_rim_rattle.timer_remaining = 0U;
    before_late_failure = corrupted;
    if (scene_update_shot(&corrupted, &neutral) ||
        memcmp(&corrupted, &before_late_failure,
               sizeof(corrupted)) != 0 ||
        corrupted.jump_rim_rattle_audio_repeats !=
            before_late_failure.jump_rim_rattle_audio_repeats) {
        scene_test_claimant_failure = 41U;
        return false;
    }
    corrupted = *scene;
    corrupted.jump_ball_state = 0U;
    before_late_failure = corrupted;
    if (scene_update_shot(&corrupted, &neutral) ||
        memcmp(&corrupted, &before_late_failure,
               sizeof(corrupted)) != 0) {
        scene_test_claimant_failure = 42U;
        return false;
    }
    for (unsigned actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        scene->actors[actor].position.x = 650;
        scene->actors[actor].position.y = 230;
        scene->actors[actor].anchor = scene->actors[actor].position;
    }
    for (update = 0U; update < TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
         ++update) {
        if (!scene_update_shot(scene, &neutral)) {
            scene_test_claimant_failure = 43U + update;
            return false;
        }
    }
    before_late_failure = *scene;
    if (!scene_update_shot(scene, &neutral)) {
        scene_test_claimant_failure = 43U;
        return false;
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene->loose_ball_state.active ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->state.possession != before_late_failure.state.possession ||
        scene->claimant_settlement_trace.event_serial !=
            before_late_failure.claimant_settlement_trace.event_serial ||
        scene->jump_rim_rattle_audio_repeats != 0U ||
        !scene_ownership_valid(scene)) {
        scene_test_claimant_failure = 53U;
        return false;
    }
    before_late_failure = *scene;
    if (scene_update_shot(scene, &neutral) ||
        memcmp(scene, &before_late_failure, sizeof(*scene)) != 0) {
        scene_test_claimant_failure = 44U;
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_settlement_one_point_fields(
    TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    TecmoGameplayShotEvaluationInput input;
    TecmoGameplayShotEvaluation evaluation;
    int32_t delta_x;
    int32_t delta_y;
    uint32_t native_policy_sample;
    TecmoGameplayShotRimRoute route;
    uint16_t resolved_pose;
    uint8_t profile;
    TecmoGameplayShotDirectionSlot direction;
    if (scene == NULL || scene->shot_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        !scene->predicted_make_route) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    player = scene_actor_player(scene, actor);
    delta_x = scene->shot_target_delta_x;
    delta_y = scene->shot_target_delta_y;
    if (delta_x < INT16_MIN || delta_x > INT16_MAX ||
        delta_y < INT16_MIN || delta_y > INT16_MAX ||
        (delta_x == 0 && delta_y == 0) || player == NULL) {
        scene_test_claimant_failure = 44U;
        return false;
    }
    if (!tecmo_gameplay_shot_profile_from_profile_byte2(
            player->profile[2], &profile) ||
        !tecmo_gameplay_shot_resolution_direction_for_delta(
            (int16_t)delta_x, (int16_t)delta_y, &direction)) {
        scene_test_claimant_failure = 45U;
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.player_rating = player->profile[0];
    input.point_value = 1U;
    input.close_context = false;
    input.contact_context = scene->shot_contact_context;
    input.contest_context = scene->shot_contest_context;
    input.horizontal_distance = (int16_t)delta_x;
    input.vertical_distance = (int16_t)(
        TECMO_GAMEPLAY_SHOT_TARGET_Y -
        (int)scene->shot_actor_launch_position.y);
    input.profile = profile;
    input.direction = (uint8_t)direction;
    input.numeric_variant = 0U;
    native_policy_sample = scene_shot_native_policy_sample_from_inputs(
        scene->shot_actor_launch_position.x,
        scene->shot_actor_launch_position.y, 1U,
        (int16_t)delta_x, (int16_t)delta_y,
        scene->shot_actor_team, scene->shot_actor_roster_index,
        scene->shot_launch_frame);
    input.family = scene_shot_family_for_context(
        (int16_t)delta_x, (int16_t)delta_y, native_policy_sample);
    input.native_policy_sample = native_policy_sample;
    if (!tecmo_gameplay_shot_resolution_resolve_rim_route(
            &scene->shot_resolution, (uint8_t)native_policy_sample, &route) ||
        !tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
            &scene->jump_shots,
            (TecmoGameplayJumpShotFamily)input.family,
            (TecmoGameplayJumpShotProfile)input.profile,
            (TecmoGameplayJumpShotDirection)input.direction,
            &resolved_pose)) {
        return false;
    }
    if (!tecmo_gameplay_shot_resolution_evaluate(
            &scene->shot_resolution, &input, &evaluation) ||
        evaluation.outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
        evaluation.schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION) {
        return false;
    }
    scene->shot_points = evaluation.point_value;
    scene->shot_flags = 0x01U;
    scene->native_policy_sample = native_policy_sample;
    scene->shot_rim_rattle_raw_selector = (uint8_t)native_policy_sample;
    scene->shot_context_signature = scene_shot_context_signature(
        native_policy_sample, evaluation.contact_context,
        evaluation.contest_context);
    scene->shot_rim_route = route;
    /* This helper rebinds a MAKE fixture; raw A7A9 identity remains metadata,
       but the miss-only rattle activation bit must stay clear. */
    scene->shot_rim_rattle_selected = false;
    scene->jump_family = (TecmoGameplayJumpShotFamily)input.family;
    scene->jump_profile = (TecmoGameplayJumpShotProfile)input.profile;
    scene->jump_direction = (TecmoGameplayJumpShotDirection)input.direction;
    scene->jump_resolved_pose_index = resolved_pose;
    scene->shot_make_probability = evaluation.make_probability;
    scene->shot_contact_context = evaluation.contact_context;
    scene->shot_contest_context = evaluation.contest_context;
    scene->shot_outcome = evaluation.outcome;
    scene->shot_schedule = evaluation.schedule;
    return true;
}

static bool scene_test_approximate_make_physics(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    uint8_t desired_points)
{
    TecmoControlFrame neutral;
    TecmoControlFrame held;
    TecmoGameplayScene malformed;
    TecmoGameplayScene snapshot;
    uint16_t score_before;
    unsigned physics_steps = 0U;
    unsigned updates = 0U;
    bool saw_release = false;
    bool saw_landing = false;
    bool saw_score = false;
    bool settlement_one_point = desired_points == 1U;
    uint8_t settlement_stat_team = 0U;
    uint8_t settlement_stat_roster = 0U;
    if (!scene_test_find_approx_make(
            scene, base_launch,
            desired_points, -1)) {
        return false;
    }
    if (settlement_one_point) {
        /* The controller's current point classifier with shot_flags==0 only
           selects 2/3.  This is a settlement-only one-point fixture, not a
           claim that the production selector emits free throws here. */
        if (!scene_ownership_valid(scene)) {
            return false;
        }
        settlement_stat_team = scene->shot_actor_team;
        settlement_stat_roster = scene->shot_actor_roster_index;
        if (settlement_stat_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
            settlement_stat_roster >= TECMO_PLAYER_STATS_ROSTER_COUNT ||
            scene->player_stats.counters[settlement_stat_team][
                settlement_stat_roster][TECMO_PLAYER_STATS_COUNTER_FGA] != 1U ||
            scene->player_stats.counters[settlement_stat_team][
                settlement_stat_roster][TECMO_PLAYER_STATS_COUNTER_FGM] != 0U ||
            scene->player_stats.counters[settlement_stat_team][
                settlement_stat_roster][TECMO_PLAYER_STATS_COUNTER_THREE_PM] !=
                0U) {
            return false;
        }
    } else if (scene->shot_points != desired_points && desired_points != 0U) {
        return false;
    }
    score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    memset(&neutral, 0, sizeof(neutral));
    memset(&held, 0, sizeof(held));
    held.held.cancel = true;
    /* The approximate controller advances from external frame 5 to frame 6
       while B/cancel remains held, then releases on the next update.  This
       explicitly proves the inclusive unreleased gather boundary. */
    while (scene->shot_frame <
               TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME - 1U) {
        if (!scene_update_shot(scene, &held)) {
            return false;
        }
    }
    if (scene->shot_frame !=
            TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME - 1U ||
        !scene_update_shot(scene, &held) ||
        scene->shot_frame != TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME ||
        scene->jump_b_released ||
        scene->actors[scene->shot_actor].pose_index !=
            scene->jump_entry_pose_index ||
        !scene_shot_state_valid(scene)) {
        return false;
    }
    malformed = *scene;
    malformed.jump_entry_pose_index ^= 1U;
    malformed.actors[malformed.shot_actor].pose_index ^= 1U;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           updates < 80U) {
        uint16_t frame_before = scene->shot_frame;
        if (!scene_update_shot(scene, &neutral)) {
            return false;
        }
        ++updates;
        if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            if (frame_before != TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION - 1U) {
                return false;
            }
            break;
        }
        if (scene->shot_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME) {
            saw_release = scene->jump_b_released &&
                scene->jump_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                scene->jump_actor_velocity_q8 ==
                    TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8;
        }
        if (scene->shot_frame >= TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME + 1U &&
            scene->shot_frame <= TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
            ++physics_steps;
        }
        if (scene->shot_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
            saw_landing = scene->jump_actor_landed &&
                scene->jump_actor_altitude_q8 == 0U &&
                scene->jump_actor_velocity_q8 == 0U &&
                scene->jump_actor_state ==
                    scene->jump_shots.constants.actor_state_recovery;
        }
        if (scene->shot_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME) {
            saw_score = scene->shot_result_awarded &&
                scene->jump_made_settlement.complete &&
                scene->jump_made_settlement.state == 0U &&
                scene->jump_made_settlement.timer == 0U &&
                scene->jump_made_settlement.stage == 0U &&
                scene->jump_made_settlement.updates == 0U;
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !saw_release || physics_steps != 38U || !saw_landing ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            (uint16_t)(score_before + scene->shot_points) || !saw_score) {
        return false;
    }
    if (settlement_one_point &&
        (scene->player_stats.counters[settlement_stat_team][
             settlement_stat_roster][TECMO_PLAYER_STATS_COUNTER_FGM] != 1U ||
         scene->player_stats.counters[settlement_stat_team][
             settlement_stat_roster][TECMO_PLAYER_STATS_COUNTER_THREE_PM] !=
             0U)) {
        return false;
    }

    /* The public update wrapper rejects malformed schedule, state, and
       approximate-settlement markers transactionally before any step/audio. */
    if (!scene_test_find_approx_make(
            scene, base_launch,
            desired_points, -1)) {
        return false;
    }
    if (settlement_one_point) {
        if (!scene_ownership_valid(scene)) {
            return false;
        }
    }
    malformed = *scene;
    malformed.shot_schedule = (TecmoGameplayShotScheduleKind)99;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    if (settlement_one_point) {
        TecmoGameplayScene recovery_probe;
        if (!scene_test_find_approx_make(scene, base_launch, 1U, -1)) {
            return false;
        }
        if (!scene_ownership_valid(scene)) {
            return false;
        }
        while (scene->shot_frame <
                   TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_START_FRAME) {
            if (!scene_update_shot(scene, &neutral)) {
                return false;
            }
        }
        if (scene->shot_frame !=
                TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_START_FRAME) {
            return false;
        }
        recovery_probe = *scene;
        recovery_probe.jump_phase_counter = 0x07U;
        snapshot = recovery_probe;
        if (scene_update_shot(&recovery_probe, &neutral) ||
            memcmp(&recovery_probe, &snapshot, sizeof(recovery_probe)) != 0) {
            return false;
        }
        tecmo_gameplay_scene_end(scene);
        return true;
    }
    malformed = *scene;
    malformed.jump_actor_state = 0xFFU;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    malformed = *scene;
    malformed.jump_phase_counter = 0x07U;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    malformed = *scene;
    malformed.jump_made_settlement.complete = true;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    malformed = *scene;
    malformed.shot_result_awarded = true;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    malformed = *scene;
    malformed.shot_frame = malformed.shot_duration;
    snapshot = malformed;
    if (scene_update_shot(&malformed, &neutral) ||
        memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_owned_shot_boundary(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    char *message,
    size_t message_size)
{
    static const struct {
        int approach;
        int16_t y;
        TecmoGameplayCloseShotVariant variant;
        TecmoGameplaySceneShotKind kind;
    } close_cases[] = {
        {24, TECMO_GAMEPLAY_SHOT_TARGET_Y,
         TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
         TECMO_GAMEPLAY_SCENE_SHOT_DUNK},
        {32, 110, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1,
         TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1},
        {48, TECMO_GAMEPLAY_SHOT_TARGET_Y,
         TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
         TECMO_GAMEPLAY_SCENE_SHOT_LAYUP}
    };
    size_t index;
    if (scene == NULL || base_launch == NULL ||
        scene_start_shot(NULL, 0U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "owned shot NULL/start guard failed");
        return false;
    }
    if (scene_shot_native_policy_sample_from_inputs(
            160, 143, 3U, 24, 0, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
            0x00000100U) ==
        scene_shot_native_policy_sample_from_inputs(
            160, 143, 3U, 24, 0, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
            0x00010000U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "owned native-policy-sample frame-byte-position binding failed");
        return false;
    }
    for (index = 0U; index < sizeof(close_cases) / sizeof(close_cases[0]);
         ++index) {
        TecmoGameplayScene malformed;
        TecmoGameplayScene snapshot;
        uint16_t pose = 0xBEEFU;
        bool prepared = scene_test_prepare_owned_shot_fixture(
                scene, base_launch, close_cases[index].approach,
                close_cases[index].y, (uint32_t)index);
        bool started = prepared && scene_start_shot_actor(scene, 0U, 0U);
        if (!prepared || !started ||
            scene->shot_kind != close_cases[index].kind ||
            scene->close_shot_variant != close_cases[index].variant ||
            scene->shot_result_awarded ||
            !scene_ownership_valid(scene)) {
            char failure[256];
            (void)snprintf(
                failure, sizeof(failure),
                "owned close numeric matrix failed i=%u prepared=%u started=%u prep_stage=%u available=%u active=%u phase=%u legacy=%u binding=%u holder=%u kind=%u variant=%u outcome=%u schedule=%u frame=%u/%u valid=%u",
                (unsigned)index, prepared ? 1U : 0U, started ? 1U : 0U,
                scene_test_prepare_failure_stage,
                scene->available ? 1U : 0U, scene->active ? 1U : 0U,
                (unsigned)scene->state.phase,
                scene->legacy_direct_launch ? 1U : 0U,
                scene->launch.starter_binding_bound ? 1U : 0U,
                (unsigned)scene->ball_holder,
                (unsigned)scene->shot_kind,
                (unsigned)scene->close_shot_variant,
                (unsigned)scene->shot_outcome,
                (unsigned)scene->shot_schedule,
                (unsigned)scene->shot_frame,
                (unsigned)scene->shot_duration,
                scene_ownership_valid(scene) ? 1U : 0U);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            return false;
        }
        if (close_cases[index].variant ==
                TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1 &&
            (!scene_close_pose_for_step(scene, 0U, &pose) || pose == 0xBEEFU)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned numeric-1 fixed pose exposure failed");
            return false;
        }
        malformed = *scene;
        malformed.close_shot_direction =
            (TecmoGameplayCloseShotDirection)8;
        pose = 0xBEEFU;
        if (scene_close_pose_for_step(&malformed, 0U, &pose) ||
            pose != 0xBEEFU) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned close direction/output guard failed");
            return false;
        }
        malformed = *scene;
        malformed.shot_schedule = (TecmoGameplayShotScheduleKind)99;
        snapshot = malformed;
        if (scene_update_shot(&malformed, NULL) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned close malformed schedule rollback failed");
            return false;
        }
        malformed = *scene;
        malformed.shot_a0f3_origin_valid = true;
        malformed.shot_a0f3_origin_x = 1U;
        snapshot = malformed;
        if (scene_update_shot(&malformed, NULL) ||
            memcmp(&malformed, &snapshot, sizeof(malformed)) != 0) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "owned close raw-launch rollback failed");
            return false;
        }
        tecmo_gameplay_scene_end(scene);
    }
    if (!scene_test_production_close_matrix(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned bound close matrix failed case=%u detail=%u (prepared/start/close/selector counts)",
                       scene_test_close_matrix_failure,
                       scene_test_close_matrix_detail);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    if (!scene_test_production_jump_matrix(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned production jump matrix failed case=%u setup=%u (family/profile/direction or flight/replay)",
                       scene_test_production_matrix_failure,
                       scene_test_matrix_setup_failure);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        return false;
    }
    if (!scene_test_production_contact_contexts(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned production contact/contest context matrix failed stage=%u (prepare/context/probability/first-update)",
                       scene_test_contact_failure);
        tecmo_gameplay_scene_test_message(
            message, message_size, failure);
        return false;
    }
    if (!scene_test_route_tail_matrix(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned route-tail matrix failed stage=%u selector=%u advance=%u kind=%u route_selector=%u cpu=%04X frame=%u/%u",
                       scene_test_route_failure,
                       (unsigned)(scene->shot_rim_route.selector),
                       scene_test_advance_failure,
                       (unsigned)scene->shot_rim_route.kind,
                       (unsigned)scene->shot_rim_route.selector,
                       (unsigned)scene->shot_rim_route.source_target_cpu,
                       (unsigned)scene->shot_frame,
                       (unsigned)scene->shot_duration);
        tecmo_gameplay_scene_test_message(
            message, message_size, failure);
        return false;
    }
    if (!scene_test_production_terminal_scenarios(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned bound terminal scenario matrix failed stage=%u detail=%u",
                       scene_test_terminal_failure,
                       scene_test_terminal_detail);
        tecmo_gameplay_scene_test_message(
            message, message_size, failure);
        return false;
    }
    if (!scene_test_home_a7a9_orientation_one(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned jump orientation-1 rattle matrix failed stage=%u kind=%u outcome=%u route=%u/%04X direction=%u frame=%u rattle=%u/%u x=%04X render=%04X",
                       scene_test_jump_orientation_failure,
                       (unsigned)scene->shot_kind,
                       (unsigned)scene->shot_outcome,
                       (unsigned)scene->shot_rim_route.selector,
                       (unsigned)scene->shot_rim_route.source_target_cpu,
                       (unsigned)scene->orientation_state.attack_direction,
                       (unsigned)scene->shot_frame,
                       scene->jump_rim_rattle.active ? 1U : 0U,
                       (unsigned)scene->jump_rim_rattle.orientation,
                       (unsigned)scene->jump_rim_rattle.x,
                       (unsigned)scene->jump_rim_rattle.render_script_address);
        tecmo_gameplay_scene_test_message(
            message, message_size, failure);
        return false;
    }
    if (!scene_test_close_rattle_claimant_cases(scene, base_launch)) {
        char failure[192];
        (void)snprintf(failure, sizeof(failure),
                       "owned claimant/rattle matrix failed stage=%u kind=%u selector=%u cpu=%04X frame=%u/%u possession=%u holder=%u",
                       scene_test_claimant_failure,
                       (unsigned)scene->shot_kind,
                       (unsigned)scene->shot_rim_route.selector,
                       (unsigned)scene->shot_rim_route.source_target_cpu,
                       (unsigned)scene->shot_frame,
                       (unsigned)scene->shot_duration,
                       (unsigned)scene->state.possession,
                       (unsigned)scene->ball_holder);
        tecmo_gameplay_scene_test_message(
            message, message_size, failure);
        return false;
    }
    if (!scene_test_approximate_make_physics(scene, base_launch, 1U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "owned approximate one-point settlement matrix failed");
        return false;
    }
    if (!scene_test_approximate_make_physics(scene, base_launch, 2U)) {
        tecmo_gameplay_scene_test_message(
            message, message_size, "owned approximate two-point matrix failed");
        return false;
    }
    {
        TecmoGameplaySceneLaunch legacy = *base_launch;
        TecmoGameplayScene expected;
        TecmoGameplayScene malformed;
        uint16_t expected_entry;
        uint16_t expected_actor_pose;
        TecmoControlFrame neutral;
        bool legacy_a7a9_found = false;
        uint32_t legacy_frame;
        unsigned legacy_update;
        legacy.starter_binding_bound = false;
        legacy.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
        legacy.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        legacy.game_music_enabled = false;
        tecmo_gameplay_scene_test_set_skip_pretip(true);
        if (!tecmo_gameplay_scene_launch(scene, &legacy)) {
            tecmo_gameplay_scene_test_set_skip_pretip(false);
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned legacy debug launch failed");
            return false;
        }
        tecmo_gameplay_scene_test_set_skip_pretip(false);
        if (!scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_attach_ball(scene)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned legacy debug setup failed");
            return false;
        }
        /* The accepted direct render fixture carries this facing bit from
           its legacy adapter. It is only a diagnostic precondition. */
        scene->actors[0U].facing_right = true;
        expected = *scene;
        expected.action_serial = 1U;
        if (!scene_start_shot_actor(&expected, 0U, 0U)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned debug production probe failed");
            return false;
        }
        expected_entry = expected.jump_entry_pose_index;
        expected_actor_pose = expected.actors[0U].pose_index;
        if (!tecmo_gameplay_scene_start_rim_rattle_debug(scene) ||
            scene->jump_direction != TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1 ||
            scene->shot_schedule !=
                TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT ||
            scene->shot_duration != TECMO_GAMEPLAY_JUMP_RATTLE_DURATION ||
            scene->shot_points != 3U ||
            scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
            scene->jump_entry_pose_index != expected_entry ||
            scene->actors[0U].pose_index != expected_actor_pose ||
            !scene_shot_state_valid(scene)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned debug identity/entry-pose normalization failed");
            return false;
        }
        malformed = *scene;
        malformed.jump_direction =
            (TecmoGameplayJumpShotDirection)7;
        expected = malformed;
        if (scene_update_jump_miss(&malformed, NULL) ||
            memcmp(&malformed, &expected, sizeof(malformed)) != 0 ||
            scene_shot_state_valid(&malformed)) {
            tecmo_gameplay_scene_test_message(
                message, message_size, "owned debug malformed identity rollback failed");
            return false;
        }
        tecmo_gameplay_scene_end(scene);
        for (legacy_frame = 0U; legacy_frame < 256U &&
             !legacy_a7a9_found; ++legacy_frame) {
            tecmo_gameplay_scene_test_set_skip_pretip(true);
            if (!tecmo_gameplay_scene_launch(scene, &legacy)) {
                tecmo_gameplay_scene_test_set_skip_pretip(false);
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "owned legacy A7A9 search launch failed");
                return false;
            }
            tecmo_gameplay_scene_test_set_skip_pretip(false);
            if (!scene_handoff_possession(
                    scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
                !scene_attach_ball(scene)) {
                tecmo_gameplay_scene_end(scene);
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "owned legacy A7A9 search setup failed");
                return false;
            }
            scene->actors[0U].facing_right = true;
            scene->frame = legacy_frame;
            scene->action_serial = 1U;
            if (!scene_start_shot_actor(scene, 0U, 0U)) {
                tecmo_gameplay_scene_end(scene);
                continue;
            }
            if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
                !scene->predicted_make_route &&
                scene->shot_rim_route.kind ==
                    TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 &&
                !scene->shot_rim_rattle_selected) {
                legacy_a7a9_found = true;
                break;
            }
            tecmo_gameplay_scene_end(scene);
        }
        if (!legacy_a7a9_found) {
            tecmo_gameplay_scene_test_message(
                message, message_size,
                "owned legacy A7A9 not-selected miss was unreachable");
            return false;
        }
        memset(&neutral, 0, sizeof(neutral));
        while (scene->shot_frame < 72U) {
            if (!scene_update_shot(scene, &neutral)) {
                tecmo_gameplay_scene_end(scene);
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "owned legacy A7A9 pre-rattle route rejected");
                return false;
            }
        }
        for (legacy_update = 73U; legacy_update <= 75U; ++legacy_update) {
            if (!scene_update_shot(scene, &neutral) ||
                scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
                scene->shot_rim_rattle_selected ||
                scene->jump_rim_rattle.active ||
                scene->jump_rim_rattle.complete ||
                scene->jump_ball_state !=
                    scene->jump_shots.constants.ball_state_route10 ||
                scene->jump_ball_altitude_q8 != 0U ||
                scene->jump_ball_bounce_q8 !=
                    (legacy_update == 74U
                        ? scene->jump_shots.constants.bounce_decay_q8 : 0U) ||
                !scene_shot_state_valid(scene)) {
                tecmo_gameplay_scene_end(scene);
                tecmo_gameplay_scene_test_message(
                    message, message_size,
                    "owned legacy A7A9 ordinary route10/bounce contract failed");
                return false;
            }
        }
        tecmo_gameplay_scene_end(scene);
    }
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
            scene, &launch, &p1, &p2, message, message_size) ||
        !scene_test_owned_shot_boundary(
            scene, &launch, message, message_size) ||
        !scene_test_player_stats_contract(
            scene, &launch, message, message_size)) {
        tecmo_gameplay_scene_destroy(scene);
        return false;
    }
    tecmo_gameplay_scene_destroy(scene);
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
