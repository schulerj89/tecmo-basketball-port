#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <string.h>

typedef struct RulesRestartClock {
    uint8_t minutes;
    uint8_t seconds;
    uint8_t divider;
    uint8_t shot_clock;
} RulesRestartClock;

typedef enum RulesRestartRoute {
    RULES_RESTART_ROUTE_OUT_OF_BOUNDS = 0,
    RULES_RESTART_ROUTE_BACKCOURT
} RulesRestartRoute;

typedef struct RulesRestartCase {
    RulesRestartRoute route;
    TecmoGameplayTeam initial_possession;
    bool game_music_enabled;
} RulesRestartCase;

static TecmoGameplayTeam rules_other_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
        ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_TEAM_AWAY;
}

static uint8_t rules_direction_for_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U;
}

static void rules_snapshot_clock(const TecmoGameplayScene *scene,
                                 RulesRestartClock *clock)
{
    clock->minutes = scene->state.clock_minutes;
    clock->seconds = scene->state.clock_seconds;
    clock->divider = scene->state.clock_divider;
    clock->shot_clock = scene->state.shot_clock;
}

static bool rules_clock_matches(const TecmoGameplayScene *scene,
                                const RulesRestartClock *clock)
{
    return scene->state.clock_minutes == clock->minutes &&
           scene->state.clock_seconds == clock->seconds &&
           scene->state.clock_divider == clock->divider &&
           scene->state.shot_clock == clock->shot_clock;
}

static bool rules_presentation_frozen(
    const TecmoGameplayScene *scene,
    const RulesRestartClock *clock,
    const TecmoGameplayCameraState *camera)
{
    return rules_clock_matches(scene, clock) &&
           memcmp(&scene->camera_state, camera, sizeof(*camera)) == 0;
}

static bool rules_audio_clear(const TecmoGameplayScene *scene)
{
    return !scene->audio_player.sfx_pending &&
           !scene->audio_player.sfx_playing &&
           !scene->audio_player.dmc.active &&
           (scene->audio_player.music == NULL ||
            (!scene->audio_player.music->playing &&
             !scene->audio_player.music->track_pending));
}

static TecmoGameplaySceneLaunch rules_launch(bool game_music_enabled)
{
    TecmoGameplaySceneLaunch launch;
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.game_index = 0U;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = game_music_enabled;
    launch.starter_binding_bound = false;
    return launch;
}

static bool rules_launch_scene(TecmoGameplayScene *scene,
                               bool game_music_enabled,
                               TecmoGameplayTeam initial_possession)
{
    TecmoGameplaySceneLaunch launch = rules_launch(game_music_enabled);
    uint8_t holder = scene_first_actor_for_team(initial_possession);
    if (!tecmo_gameplay_scene_launch(scene, &launch)) return false;
    if (initial_possession != TECMO_GAMEPLAY_TEAM_AWAY &&
        !scene_handoff_possession(scene, initial_possession, holder)) {
        return false;
    }
    return scene->active && scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
           scene->state.possession == initial_possession &&
           scene->ball_holder == holder &&
           scene->orientation_state.current_direction ==
               rules_direction_for_team(initial_possession) &&
           scene->orientation_state.tracked_possession_team ==
               initial_possession;
}

static bool rules_run_timing_fixture(TecmoGameplayScene *scene,
                                     bool foul_fixture,
                                     TecmoGameplayTeam initial_possession)
{
    TecmoGameplayFoulRequest foul;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    RulesRestartClock clock;
    TecmoGameplayCameraState camera;
    TecmoGameplayPhase expected_phase = foul_fixture
        ? TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION
        : TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION;
    uint16_t frame;

    if (!rules_launch_scene(scene, false, initial_possession)) return false;
    /* This is an explicit foul presentation fixture, not live foul detection. */
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    if (foul_fixture) {
        memset(&foul, 0, sizeof(foul));
        foul.fouling_team = rules_other_team(initial_possession);
        foul.free_throw_team = initial_possession;
        foul.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
        foul.player_index = initial_possession == TECMO_GAMEPLAY_TEAM_AWAY
            ? 0U : 5U;
        foul.free_throw_attempts = 2U;
        if (!tecmo_gameplay_request_foul(&scene->state, &foul)) {
            return false;
        }
    } else if (!tecmo_gameplay_request_violation(
                   &scene->state,
                   TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
                   rules_other_team(initial_possession))) {
        return false;
    }
    if (scene->state.phase != expected_phase ||
        scene->state.phase_frame != 0U || scene->audio_player.sfx_pending) {
        return false;
    }
    rules_snapshot_clock(scene, &clock);
    camera = scene->camera_state;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 1U; frame <= 24U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != expected_phase ||
            scene->state.phase_frame != frame ||
            !rules_presentation_frozen(scene, &clock, &camera)) {
            return false;
        }
        if (frame < 16U && scene->audio_player.sfx_pending) {
            return false;
        }
        if (frame == 16U) {
            if (!scene->audio_player.sfx_pending ||
                scene->audio_player.pending_sfx_id != 6U) {
                return false;
            }
            tecmo_gameplay_audio_render_samples(
                &scene->audio_player, NULL, 1024U);
            if (scene->audio_player.sfx_pending ||
                scene->audio_player.current_sfx_id != 6U) {
                return false;
            }
        } else if (frame > 16U && scene->audio_player.sfx_pending) {
            return false;
        }
    }
    return true;
}

static void rules_report_timing_failure(
    TecmoGameplaySceneTestContext *test,
    const TecmoGameplayScene *scene,
    bool foul_fixture,
    TecmoGameplayTeam initial_possession)
{
    if (test == NULL || scene == NULL || test->message == NULL ||
        test->message_size == 0U) {
        return;
    }
    (void)snprintf(
        test->message, test->message_size,
        "rules/restarts %s timing failed: team=%u phase=%u phase_frame=%u sfx=%u/%u current=%u clock=%u:%02u/%u shot=%u",
        foul_fixture ? "foul" : "violation",
        (unsigned)initial_possession,
        (unsigned)scene->state.phase,
        (unsigned)scene->state.phase_frame,
        scene->audio_player.sfx_pending ? 1U : 0U,
        (unsigned)scene->audio_player.pending_sfx_id,
        (unsigned)scene->audio_player.current_sfx_id,
        (unsigned)scene->state.clock_minutes,
        (unsigned)scene->state.clock_seconds,
        (unsigned)scene->state.clock_divider,
        (unsigned)scene->state.shot_clock);
}

static void rules_prepare_boundary_actor(TecmoGameplaySceneActor *actor,
                                         int16_t x,
                                         int16_t y,
                                         uint8_t direction)
{
    actor->position.x = x;
    actor->position.y = y;
    actor->anchor = actor->position;
    actor->movement_action_state = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    actor->movement_direction = direction;
    actor->movement_fractional_accumulator = 15U;
    actor->movement_animation_phase = 0U;
    actor->movement_boundary_latched = false;
}

static bool rules_trigger_out_of_bounds(TecmoGameplayScene *scene,
                                        TecmoGameplayTeam initial_possession)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t holder = scene_first_actor_for_team(initial_possession);
    TecmoGameplaySceneActor *actor = &scene->actors[holder];
    int16_t boundary_x = initial_possession == TECMO_GAMEPLAY_TEAM_AWAY
        ? 149 : 617;

    rules_prepare_boundary_actor(actor, boundary_x, 148,
                                 initial_possession ==
                                     TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U);
    if (!scene_attach_ball(scene)) return false;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (initial_possession == TECMO_GAMEPLAY_TEAM_AWAY) {
        p1.held.left = true;
    } else {
        p2.held.right = true;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        actor->movement_boundary_latched) {
        return false;
    }
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene->state.violation != TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        scene->state.restart_possession != rules_other_team(initial_possession) ||
        scene->state.possession != initial_possession ||
        scene->state.phase_frame != 0U ||
        scene->ball_holder != holder ||
        actor->movement_boundary_latched) {
        return false;
    }
    return scene->events.count == 0U;
}

static bool rules_trigger_backcourt(TecmoGameplayScene *scene,
                                    TecmoGameplayTeam initial_possession)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t holder = scene_first_actor_for_team(initial_possession);
    TecmoGameplaySceneActor *actor = &scene->actors[holder];
    bool away = initial_possession == TECMO_GAMEPLAY_TEAM_AWAY;

    actor->position.x = away ? 368 : 398;
    actor->position.y = 148;
    actor->anchor = actor->position;
    actor->facing_right = away;
    actor->movement_direction = away ? 0U : 1U;
    actor->movement_action_state = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    actor->movement_fractional_accumulator = 0U;
    actor->movement_boundary_latched = false;
    scene->ball_position.x_q8 = (away ? 375 : 392) * 256;
    scene->ball_position.y_q8 = 131 * 256;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !scene->backcourt_state.frontcourt_established) {
        return false;
    }
    actor->position.x = away ? 380 : 387;
    actor->anchor = actor->position;
    scene->ball_position.x_q8 = (away ? 386 : 383) * 256;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene->state.violation != TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        scene->state.restart_possession != rules_other_team(initial_possession) ||
        scene->state.possession != initial_possession ||
        scene->state.phase_frame != 0U ||
        scene->ball_holder != holder) {
        return false;
    }
    return scene->events.count == 0U;
}

static bool rules_restart_controls(TecmoControlFrame *p1,
                                   TecmoControlFrame *p2)
{
    if (p1 == NULL || p2 == NULL) return false;
    memset(p1, 0, sizeof(*p1));
    memset(p2, 0, sizeof(*p2));
    p1->held.up = true;
    p1->held.down = true;
    p1->held.left = true;
    p1->held.right = true;
    p1->held.confirm = true;
    p1->held.cancel = true;
    p1->held.shoot = true;
    p1->held.tab = true;
    p1->pressed = p1->held;
    p1->released = p1->held;
    p2->held.up = true;
    p2->held.down = true;
    p2->held.left = true;
    p2->held.right = true;
    p2->held.confirm = true;
    p2->held.cancel = true;
    p2->held.shoot = true;
    p2->held.tab = true;
    p2->pressed = p2->held;
    p2->released = p2->held;
    return true;
}

static bool rules_check_live_frame_coherence(const TecmoGameplayScene *scene,
                                             TecmoGameplayTeam possession,
                                             uint8_t holder)
{
    TecmoGameplaySceneCourtFrame frame;
    TecmoGameplayCourtCoordinateQ8 expected_ball = {0};
    uint8_t direction = rules_direction_for_team(possession);
    if (!tecmo_gameplay_scene_court_frame(scene, &frame) ||
        !scene_ball_position_for_actors(
            scene, scene->actors, holder, &expected_ball) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != possession ||
        scene->state.restart_possession != possession ||
        scene->state.violation != TECMO_GAMEPLAY_VIOLATION_NONE ||
        scene->ball_holder != holder ||
        scene->actors[holder].team != (uint8_t)possession ||
        scene->controlled_actor[possession == TECMO_GAMEPLAY_TEAM_AWAY ? 0U : 1U]
            != holder ||
        scene->orientation_state.current_direction != direction ||
        scene->orientation_state.tracked_possession_team != possession ||
        frame.contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        frame.scene_frame != scene->frame ||
        frame.slice.transition_serial !=
            scene->orientation_state.transition_serial ||
        frame.slice.possession != (uint8_t)possession ||
        frame.slice.direction != direction ||
        frame.slice.viewport.camera_x != scene->camera_state.camera_x ||
        frame.projection.camera_x != scene->camera_state.camera_x ||
        frame.slice.viewport.camera_x != frame.projection.camera_x ||
        expected_ball.x_q8 != scene->ball_position.x_q8 ||
        expected_ball.y_q8 != scene->ball_position.y_q8) {
        return false;
    }
    return true;
}

static bool rules_run_violation_restart(TecmoGameplayScene *scene,
                                        RulesRestartRoute route,
                                        TecmoGameplayTeam initial_possession,
                                        bool game_music_enabled)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    RulesRestartClock clock;
    TecmoGameplayCameraState camera;
    TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayTeam restart_possession = rules_other_team(initial_possession);
    uint8_t old_holder;
    uint8_t new_holder = scene_first_actor_for_team(restart_possession);
    uint32_t transition_before;
    uint16_t frame;
    size_t actor;

    if (!rules_launch_scene(scene, game_music_enabled, initial_possession)) {
        return false;
    }
    if (route == RULES_RESTART_ROUTE_OUT_OF_BOUNDS) {
        if (!rules_trigger_out_of_bounds(scene, initial_possession)) {
            return false;
        }
    } else if (!rules_trigger_backcourt(scene, initial_possession)) {
        return false;
    }
    if (!rules_audio_clear(scene)) return false;
    rules_snapshot_clock(scene, &clock);
    camera = scene->camera_state;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 1U; frame <=
         TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.phase_frame != frame ||
            !rules_presentation_frozen(scene, &clock, &camera)) {
            return false;
        }
        if (frame < 16U && scene->audio_player.sfx_pending) {
            return false;
        }
        if (frame == 16U) {
            if (!scene->audio_player.sfx_pending ||
                scene->audio_player.pending_sfx_id != 6U) {
                return false;
            }
            tecmo_gameplay_audio_render_samples(
                &scene->audio_player, NULL, 1024U);
            if (scene->audio_player.sfx_pending ||
                scene->audio_player.current_sfx_id != 6U) {
                return false;
            }
        } else if (frame > 16U && scene->audio_player.sfx_pending) {
            return false;
        }
    }
    if (scene->state.phase_frame !=
            TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES ||
        !rules_presentation_frozen(scene, &clock, &camera)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        positions[actor] = scene->actors[actor].position;
    }
    old_holder = scene->ball_holder;
    transition_before = scene->orientation_state.transition_serial;
    if (!rules_restart_controls(&p1, &p2) ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        return false;
    }
    if (scene->state.possession != restart_possession ||
        scene->state.restart_possession != restart_possession ||
        scene->state.phase_frame != 0U ||
        scene->state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        scene->state.clock_divider != TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        scene->events.count != 1U ||
        scene->events.events[0U].kind !=
            TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST ||
        scene->events.events[0U].value != TECMO_GAMEPLAY_RESTART_PLAY_ID ||
        scene->events.events[0U].detail != (uint16_t)restart_possession ||
        scene->orientation_state.transition_serial != transition_before + 1U ||
        old_holder == scene->ball_holder ||
        scene->ball_holder != new_holder ||
        scene->backcourt_state.frontcourt_established ||
        !rules_check_live_frame_coherence(scene, restart_possession, new_holder)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (scene->actors[actor].position.x != positions[actor].x ||
            scene->actors[actor].position.y != positions[actor].y ||
            scene->actors[actor].movement_boundary_latched) {
            return false;
        }
    }
    if (game_music_enabled) {
        if (!scene->audio_player.sfx_pending ||
            scene->audio_player.pending_sfx_id !=
                TECMO_GAMEPLAY_RESTART_PLAY_ID ||
            scene->audio_player.music == NULL ||
            !scene->audio_player.music->track_pending ||
            scene->audio_player.music->pending_track_id !=
                TECMO_MUSIC_TRACK_GAMEPLAY) {
            return false;
        }
        tecmo_gameplay_audio_render_samples(
            &scene->audio_player, NULL, 1024U);
        if (scene->audio_player.sfx_pending ||
            scene->audio_player.current_sfx_id !=
                TECMO_GAMEPLAY_RESTART_PLAY_ID ||
            !scene->audio_player.music->playing ||
            scene->audio_player.music->current_track_id !=
                TECMO_MUSIC_TRACK_GAMEPLAY ||
            scene->audio_player.music->track_pending) {
            return false;
        }
    } else if (scene->audio_player.sfx_pending ||
               scene->audio_player.current_sfx_id != 6U ||
               (scene->audio_player.music != NULL &&
                (scene->audio_player.music->playing ||
                 scene->audio_player.music->track_pending))) {
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->orientation_state.transition_serial != transition_before + 1U ||
        scene->audio_player.sfx_pending ||
        (scene->audio_player.music != NULL &&
         scene->audio_player.music->track_pending)) {
        return false;
    }
    return true;
}

static void rules_report_restart_failure(
    TecmoGameplaySceneTestContext *test,
    const TecmoGameplayScene *scene,
    const RulesRestartCase *restart_case)
{
    TecmoGameplaySceneCourtFrame frame;
    TecmoGameplayCourtCoordinateQ8 expected_ball = {0};
    bool frame_ok;
    bool ball_ok;
    if (test == NULL || scene == NULL || restart_case == NULL ||
        test->message == NULL || test->message_size == 0U) {
        return;
    }
    frame_ok = tecmo_gameplay_scene_court_frame(scene, &frame);
    ball_ok = scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
              scene_ball_position_for_actors(
                  scene, scene->actors, scene->ball_holder, &expected_ball);
    (void)snprintf(
        test->message, test->message_size,
        "rules/restarts %s restart failed: team=%u music=%u phase=%u frame=%u violation=%u possession=%u restart=%u holder=%u events=%u sfx=%u/%u current=%u music-playing=%u music-pending=%u TGBC=%u TGOR=%u frame-ok=%u scene-frame=%u camera=%u/%u/%u ball-ok=%u ball=%u/%u expected=%u/%u status=%s",
        restart_case->route == RULES_RESTART_ROUTE_OUT_OF_BOUNDS
            ? "out-of-bounds" : "backcourt",
        (unsigned)restart_case->initial_possession,
        restart_case->game_music_enabled ? 1U : 0U,
        (unsigned)scene->state.phase,
        (unsigned)scene->state.phase_frame,
        (unsigned)scene->state.violation,
        (unsigned)scene->state.possession,
        (unsigned)scene->state.restart_possession,
        (unsigned)scene->ball_holder,
        (unsigned)scene->events.count,
        scene->audio_player.sfx_pending ? 1U : 0U,
        (unsigned)scene->audio_player.pending_sfx_id,
        (unsigned)scene->audio_player.current_sfx_id,
        scene->audio_player.music != NULL &&
                scene->audio_player.music->playing ? 1U : 0U,
        scene->audio_player.music != NULL &&
                scene->audio_player.music->track_pending ? 1U : 0U,
        (unsigned)scene->backcourt_state.frontcourt_established,
        (unsigned)scene->orientation_state.transition_serial,
        frame_ok ? 1U : 0U,
        frame_ok ? (unsigned)frame.scene_frame : 0U,
        frame_ok ? (unsigned)frame.slice.viewport.camera_x : 0U,
        (unsigned)scene->camera_state.camera_x,
        frame_ok ? (unsigned)frame.projection.camera_x : 0U,
        ball_ok ? 1U : 0U,
        (unsigned)scene->ball_position.x_q8,
        (unsigned)scene->ball_position.y_q8,
        ball_ok ? (unsigned)expected_ball.x_q8 : 0U,
        ball_ok ? (unsigned)expected_ball.y_q8 : 0U,
        scene->status);
}

bool tecmo_gameplay_scene_test_rules_restarts(
    TecmoGameplaySceneTestContext *test)
{
    static const struct {
        bool foul_fixture;
        TecmoGameplayTeam initial_possession;
    } timing_cases[] = {
        { false, TECMO_GAMEPLAY_TEAM_AWAY },
        { false, TECMO_GAMEPLAY_TEAM_HOME },
        { true, TECMO_GAMEPLAY_TEAM_AWAY },
        { true, TECMO_GAMEPLAY_TEAM_HOME }
    };
    static const RulesRestartCase restart_cases[] = {
        { RULES_RESTART_ROUTE_OUT_OF_BOUNDS,
          TECMO_GAMEPLAY_TEAM_AWAY, false },
        { RULES_RESTART_ROUTE_OUT_OF_BOUNDS,
          TECMO_GAMEPLAY_TEAM_AWAY, true },
        { RULES_RESTART_ROUTE_OUT_OF_BOUNDS,
          TECMO_GAMEPLAY_TEAM_HOME, false },
        { RULES_RESTART_ROUTE_OUT_OF_BOUNDS,
          TECMO_GAMEPLAY_TEAM_HOME, true },
        { RULES_RESTART_ROUTE_BACKCOURT,
          TECMO_GAMEPLAY_TEAM_AWAY, false },
        { RULES_RESTART_ROUTE_BACKCOURT,
          TECMO_GAMEPLAY_TEAM_AWAY, true },
        { RULES_RESTART_ROUTE_BACKCOURT,
          TECMO_GAMEPLAY_TEAM_HOME, false },
        { RULES_RESTART_ROUTE_BACKCOURT,
          TECMO_GAMEPLAY_TEAM_HOME, true }
    };
    TecmoGameplayScene scene;
    size_t index;
    bool loaded = false;
    bool ok = false;

    if (test == NULL || test->project_root == NULL ||
        test->asset_pack_path == NULL || test->music_player == NULL) {
        return false;
    }
    memset(&scene, 0, sizeof(scene));
    tecmo_gameplay_scene_init(&scene);
    tecmo_gameplay_scene_test_set_skip_pretip(true);
    if (!tecmo_gameplay_scene_load(
            &scene, test->project_root, test->asset_pack_path,
            test->music_player)) {
        tecmo_gameplay_scene_test_message(
            test->message, test->message_size,
            "rules/restarts scene load failed");
        goto cleanup;
    }
    loaded = true;
    for (index = 0U; index < sizeof(timing_cases) / sizeof(timing_cases[0]);
         ++index) {
        if (!rules_run_timing_fixture(
                &scene, timing_cases[index].foul_fixture,
                timing_cases[index].initial_possession)) {
            rules_report_timing_failure(
                test, &scene, timing_cases[index].foul_fixture,
                timing_cases[index].initial_possession);
            goto cleanup;
        }
        tecmo_gameplay_scene_end(&scene);
    }
    for (index = 0U; index < sizeof(restart_cases) / sizeof(restart_cases[0]);
         ++index) {
        if (!rules_run_violation_restart(
                &scene, restart_cases[index].route,
                restart_cases[index].initial_possession,
                restart_cases[index].game_music_enabled)) {
            rules_report_restart_failure(test, &scene, &restart_cases[index]);
            goto cleanup;
        }
        tecmo_gameplay_scene_end(&scene);
    }
    tecmo_gameplay_scene_test_message(
        test->message, test->message_size,
        "GAMEPLAY RULES/RESTARTS SELF TEST PASS");
    ok = true;

cleanup:
    if (loaded) tecmo_gameplay_scene_end(&scene);
    tecmo_gameplay_scene_destroy(&scene);
    tecmo_gameplay_scene_test_set_skip_pretip(false);
    return ok;
}
