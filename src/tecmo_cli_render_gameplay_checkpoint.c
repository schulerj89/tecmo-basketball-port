#include "tecmo_controls.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_gameplay_violation_referee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"


typedef struct TecmoCliGameplayCheckpointConfig {
    unsigned checkpoint;
    uint8_t away_team;
    uint8_t home_team;
    bool jump;
    bool jump_make;
    bool jump_rattle;
    bool dunk;
    bool pretip_checkpoint;
    bool live_start;
    bool facing_checkpoint;
    bool ball_bounce;
    bool cpu_steering;
    bool shot_clock_violation;
    bool out_of_bounds_violation;
    bool backcourt_violation;
    int possession_slice;
    int free_throw_orientation;
} TecmoCliGameplayCheckpointConfig;

static bool gameplay_checkpoint_goal_facing_right(
    const TecmoGameplayScene *scene,
    uint8_t team,
    bool *facing_right_out)
{
    TecmoGameplayCourtCoordinate hoop;
    if (scene == NULL || facing_right_out == NULL ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &scene->court_orientation, &scene->orientation_state,
            team, &hoop)) {
        return false;
    }
    if (hoop.x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X) {
        *facing_right_out = false;
    } else if (hoop.x == TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X) {
        *facing_right_out = true;
    } else {
        return false;
    }
    return true;
}

static bool parse_gameplay_render_checkpoint_mode(const char *mode_name, TecmoCliGameplayCheckpointConfig *config)
{
    unsigned checkpoint = 0U;
    uint8_t away_team = 0U;
    uint8_t home_team = 1U;
    bool jump = false;
    bool jump_make = false;
    bool jump_rattle = false;
    bool dunk = false;
    bool pretip_checkpoint = false;
    bool live_start = false;
    bool facing_checkpoint = false;
    bool ball_bounce = false;
    bool cpu_steering = false;
    bool shot_clock_violation = false;
    bool out_of_bounds_violation = false;
    bool backcourt_violation = false;
    int possession_slice = -1;
    int free_throw_orientation = -1;

    if (mode_name == NULL || config == NULL) return false;
    if (strcmp(mode_name, "gameplay-start") == 0) {
        checkpoint = 0U;
        pretip_checkpoint = true;
    } else if (strcmp(mode_name, "gameplay-pretip-bulls-pacers") == 0) {
        checkpoint = 661U;
        away_team = 3U;
        home_team = 10U;
        pretip_checkpoint = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-pretip-frame", &checkpoint)) {
        pretip_checkpoint = true;
    } else if (strcmp(mode_name, "gameplay-live-start") == 0) {
        checkpoint = 691U;
        live_start = true;
    } else if (strcmp(mode_name, "gameplay-facing-away-left") == 0) {
        checkpoint = 691U;
        live_start = true;
        facing_checkpoint = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-ball-bounce-frame", &checkpoint)) {
        ball_bounce = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-cpu-steering-frame", &checkpoint)) {
        cpu_steering = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-shot-clock-violation-frame",
                   &checkpoint)) {
        shot_clock_violation = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-out-of-bounds-frame",
                   &checkpoint)) {
        out_of_bounds_violation = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-backcourt-frame",
                   &checkpoint)) {
        backcourt_violation = true;
    } else if (strcmp(mode_name, "gameplay-uniform-pacers") == 0) {
        checkpoint = 691U;
        away_team = 3U;
        home_team = 10U;
        live_start = true;
    } else if (strcmp(mode_name, "gameplay-possession-left") == 0) {
        checkpoint = 691U;
        possession_slice = 0;
    } else if (strcmp(mode_name, "gameplay-possession-center") == 0) {
        checkpoint = 691U;
        possession_slice = 1;
    } else if (strcmp(mode_name, "gameplay-possession-right") == 0) {
        checkpoint = 691U;
        possession_slice = 2;
    } else if (strcmp(mode_name, "gameplay-free-throw-left") == 0) {
        checkpoint = 696U;
        free_throw_orientation = 0;
    } else if (strcmp(mode_name, "gameplay-free-throw-right") == 0) {
        checkpoint = 696U;
        free_throw_orientation = 1;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-frame", &checkpoint)) {
        jump = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-make-frame", &checkpoint)) {
        jump = true;
        jump_make = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-rattle-frame", &checkpoint)) {
        jump = true;
        jump_rattle = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-dunk-frame", &checkpoint)) {
        dunk = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-close-shot-frame", &checkpoint)) {
        /* Compatibility spelling for the former numeric-variant-0 mode. */
        dunk = true;
    } else {
        return false;
    }
    if (pretip_checkpoint && checkpoint >= 691U) return false;
    if (ball_bounce && (checkpoint == 0U || checkpoint > 15U)) {
        return false;
    }
    if (cpu_steering && (checkpoint == 0U || checkpoint > 240U)) {
        return false;
    }
    if ((shot_clock_violation || out_of_bounds_violation ||
         backcourt_violation) &&
        checkpoint >= TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES) {
        return false;
    }
    if ((jump && (checkpoint == 0U ||
                  checkpoint >
                      (jump_make ? 111U : (jump_rattle ? 103U : 87U)))) ||
        (dunk && (checkpoint == 0U || checkpoint > 132U))) {
        return false;
    }


    config->checkpoint = checkpoint;
    config->away_team = away_team;
    config->home_team = home_team;
    config->jump = jump;
    config->jump_make = jump_make;
    config->jump_rattle = jump_rattle;
    config->dunk = dunk;
    config->pretip_checkpoint = pretip_checkpoint;
    config->live_start = live_start;
    config->facing_checkpoint = facing_checkpoint;
    config->ball_bounce = ball_bounce;
    config->cpu_steering = cpu_steering;
    config->shot_clock_violation = shot_clock_violation;
    config->out_of_bounds_violation = out_of_bounds_violation;
    config->backcourt_violation = backcourt_violation;
    config->possession_slice = possession_slice;
    config->free_throw_orientation = free_throw_orientation;
    return true;
}

static bool run_gameplay_checkpoint_preflight(TecmoRuntime *runtime, const TecmoCliGameplayCheckpointConfig *config, bool *done_out)
{
    TecmoGameplaySceneLaunch launch;
    TecmoInput input;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const uint8_t away_team = config->away_team;
    const uint8_t home_team = config->home_team;
    const bool pretip_checkpoint = config->pretip_checkpoint;
    const bool live_start = config->live_start;
    const bool ball_bounce = config->ball_bounce;
    const bool cpu_steering = config->cpu_steering;

    *done_out = false;
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = away_team;
    launch.home_team = home_team;
    launch.regulation_minutes = 3U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0] = cpu_steering
        ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(&runtime->gameplay_scene, &launch)) {
        return false;
    }
    tecmo_runtime_set_mode(runtime, TECMO_MODE_COURT);
    if (pretip_checkpoint) {
        memset(&input, 0, sizeof(input));
        for (update = 0U; update < checkpoint; ++update)
            tecmo_runtime_update(runtime, &input);
        *done_out = true; return runtime->mode == TECMO_MODE_COURT &&
               runtime->gameplay_scene.active &&
               tecmo_gameplay_scene_in_pretip(
                   &runtime->gameplay_scene);
    }
    memset(&input, 0, sizeof(input));
    for (update = 0U; update < 691U; ++update)
        tecmo_runtime_update(runtime, &input);
    if (live_start) {
        *done_out = true;
        return runtime->mode == TECMO_MODE_COURT &&
               runtime->gameplay_scene.active &&
               !tecmo_gameplay_scene_in_pretip(
                   &runtime->gameplay_scene);
    }
    if (ball_bounce) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        *done_out = true; return runtime->mode == TECMO_MODE_COURT && scene->active &&
               scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
               scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               scene->frame == 691U + checkpoint &&
               scene->ball_holder == 0U;
    }
    if (cpu_steering) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        *done_out = true; return runtime->mode == TECMO_MODE_COURT && scene->active &&
               scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               scene->frame == 691U + checkpoint &&
               scene->ball_holder == 0U &&
               scene->actors[0].position.x < 0x0160 &&
               scene->cpu_actors[0].decision_serial == checkpoint &&
               scene->cpu_actors[0].target_valid &&
               scene->cpu_actors[0].target_kind ==
                   TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
               scene->cpu_actors[0].target_position.x == 208 &&
               scene->cpu_actors[0].target_position.y == 148;
    }
    return true;
}

static bool run_gameplay_facing_checkpoint(
    const TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config)
{
    const TecmoGameplayScene *scene;
    TecmoGameplaySceneCourtFrame court_frame;
    size_t actor;
    size_t visible_away = 0U;
    (void)config;
    if (runtime == NULL || !runtime->gameplay_scene.active) return false;
    scene = &runtime->gameplay_scene;
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->orientation_state.current_direction != 0U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene->orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_scene_court_frame(scene, &court_frame)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        bool expected_facing;
        if (!item->active ||
            !gameplay_checkpoint_goal_facing_right(
                scene, item->team, &expected_facing) ||
            item->facing_right != expected_facing) {
            return false;
        }
        if (item->team == TECMO_GAMEPLAY_TEAM_AWAY &&
            court_frame.projection.players[actor].visible) {
            ++visible_away;
        }
    }
    return visible_away > 0U;
}

static bool run_gameplay_violation_checkpoint(
    TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config,
    bool *handled_out)
{
    TecmoInput input;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const bool shot_clock_violation = config->shot_clock_violation;
    const bool out_of_bounds_violation = config->out_of_bounds_violation;
    const bool backcourt_violation = config->backcourt_violation;

    *handled_out = false;
    memset(&input, 0, sizeof(input));
    if (shot_clock_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        *handled_out = true;
        scene->state.shot_clock = 1U;
        scene->state.clock_divider = 1U;
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation !=
                TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK ||
            scene->state.phase_frame != 0U) {
            return false;
        }
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK &&
               scene->state.phase_frame == checkpoint;
    }
    if (out_of_bounds_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *holder = &scene->actors[0U];

        *handled_out = true;
        /* Drive the production TGMO -> TPNL -> TGVR path instead of injecting
           a violation phase. At Y=148, X=149 is the exact page-0 clamp. The
           first held-left update takes TGMO's action-state latency; the second
           attempts X=148, clamps back to 149, and raises selector $0742=1. */
        holder->position.x = 149;
        holder->position.y = 148;
        holder->anchor = holder->position;
        holder->movement_action_state =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
        holder->movement_fractional_accumulator = 15U;
        holder->movement_boundary_latched = false;
        scene->ball_position.x_q8 = (int32_t)(holder->position.x + 7) * 256;
        scene->ball_position.y_q8 = (int32_t)(holder->position.y - 17) * 256;
        input.left = true;
        tecmo_runtime_update(runtime, &input);
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation !=
                TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
            scene->state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
            scene->state.phase_frame != 0U ||
            holder->position.x != 149 || holder->movement_boundary_latched) {
            return false;
        }
        memset(&input, 0, sizeof(input));
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS &&
               scene->state.restart_possession == TECMO_GAMEPLAY_TEAM_HOME &&
               scene->state.phase_frame == checkpoint;
    }
    if (backcourt_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *holder = &scene->actors[0U];

        *handled_out = true;
        /* Drive the production held-ball -> TGBC -> TPNL -> TGVR route.
           Orientation 0 establishes frontcourt with ball X<=375, preserves
           the original eight-pixel neutral band, then calls BACKCOURT at
           ball X>=386. These idle checkpoints account for the resolved +6
           TGBD attachment at this animation phase. */
        holder->position.x = 368;
        holder->position.y = 148;
        holder->anchor = holder->position;
        /* Deliberate diagnostic horizontal-facing override: this checkpoint
           positions the held ball in the frontcourt band before TGBC/TGVR. */
        holder->facing_right = true;
        holder->movement_action_state =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
        holder->movement_fractional_accumulator = 0U;
        holder->movement_boundary_latched = false;
        scene->ball_position.x_q8 = 375 * 256;
        scene->ball_position.y_q8 = 131 * 256;
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
            scene->backcourt_state.frontcourt_established != 1U) {
            return false;
        }
        holder->position.x = 380;
        holder->anchor = holder->position;
        scene->ball_position.x_q8 = 386 * 256;
        scene->ball_position.y_q8 = 131 * 256;
        tecmo_runtime_update(runtime, &input);
        if (scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation != TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
            scene->state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
            scene->state.phase_frame != 0U) {
            return false;
        }
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_BACKCOURT &&
               scene->state.restart_possession == TECMO_GAMEPLAY_TEAM_HOME &&
               scene->state.phase_frame == checkpoint;
    }
    return true;
}

static bool run_gameplay_camera_checkpoint(
    TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config,
    bool *handled_out)
{
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const int possession_slice = config->possession_slice;
    const int free_throw_orientation = config->free_throw_orientation;
    *handled_out = false;
    if (possession_slice >= 0) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *actor;
        TecmoGameplaySceneCourtFrame court_frame;
        uint8_t actor_index;
        *handled_out = true;
        if (possession_slice == 1) {
            return runtime->mode == TECMO_MODE_COURT &&
                   scene->active &&
                   tecmo_gameplay_scene_court_frame(
                       scene, &court_frame) &&
                    court_frame.slice.viewport.camera_x == 0x0084U &&
                    court_frame.projection.camera_x == 0x0084U &&
                   court_frame.slice.possession ==
                       TECMO_GAMEPLAY_TEAM_AWAY &&
                   court_frame.slice.direction == 0U;
        }
        actor_index =
            possession_slice == 0
                ? 0U
                : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        if (possession_slice == 2 &&
            (!tecmo_gameplay_reset_possession(
                 &scene->state, TECMO_GAMEPLAY_TEAM_HOME) ||
             !tecmo_gameplay_court_orientation_synchronize(
                 &scene->court_orientation, &scene->orientation_state,
                 TECMO_GAMEPLAY_TEAM_HOME))) {
            return false;
        }
        actor = &scene->actors[actor_index];
        actor->position.x =
            (int16_t)(possession_slice == 0 ? 0x00F3 : 0x020D);
        actor->anchor = actor->position;
        if (!gameplay_checkpoint_goal_facing_right(
                scene, actor->team, &actor->facing_right)) {
            return false;
        }
        scene->ball_holder = actor_index;
        scene->ball_position.x_q8 =
            (int32_t)(actor->position.x +
                      (actor->facing_right ? 7 : -7)) * 256;
        scene->ball_position.y_q8 =
            (int32_t)(actor->position.y - 17) * 256;
        scene->camera_state.thresholds_valid = false;
        scene->camera_state.endpoint_latched = false;
        if (!tecmo_gameplay_camera_settle_court(
                &scene->camera_assets, &scene->camera_state,
                &scene->ball_position,
                scene->orientation_state.current_direction, false) ||
            !tecmo_gameplay_camera_state_live_valid(
                &scene->camera_assets, &scene->camera_state)) {
            return false;
        }
        return tecmo_gameplay_scene_court_frame(
                   scene, &court_frame) &&
               court_frame.slice.viewport.camera_x ==
                   (uint16_t)(possession_slice == 0
                                  ? 0x0066U
                                  : 0x0198U) &&
               court_frame.projection.camera_x ==
                   court_frame.slice.viewport.camera_x &&
               court_frame.slice.possession ==
                   (uint8_t)(possession_slice == 0
                                 ? TECMO_GAMEPLAY_TEAM_AWAY
                                 : TECMO_GAMEPLAY_TEAM_HOME) &&
               court_frame.slice.direction ==
                   (uint8_t)(possession_slice == 0 ? 0U : 1U);
    }
    if (free_throw_orientation >= 0) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplayFoulRequest request;
        TecmoGameplayFreeThrowLineup lineup;
        TecmoGameplaySceneCourtFrame court_frame;
        TecmoControlFrame player_one;
        TecmoControlFrame player_two;
        uint8_t shooter =
            free_throw_orientation == 0 ? 0U
                                        : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        uint8_t secondary =
            free_throw_orientation == 0
                ? TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                : 0U;
        uint16_t camera_x =
            (uint16_t)(free_throw_orientation == 0 ? 0x0066U : 0x0198U);
        size_t actor;
        *handled_out = true;
        memset(&request, 0, sizeof(request));
        request.fouling_team =
            free_throw_orientation == 0 ? TECMO_GAMEPLAY_TEAM_HOME
                                        : TECMO_GAMEPLAY_TEAM_AWAY;
        request.free_throw_team =
            free_throw_orientation == 0 ? TECMO_GAMEPLAY_TEAM_AWAY
                                        : TECMO_GAMEPLAY_TEAM_HOME;
        request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
        request.free_throw_attempts = 2U;
        if (!tecmo_gameplay_request_foul(&scene->state, &request)) {
            return false;
        }
        memset(&player_one, 0, sizeof(player_one));
        memset(&player_two, 0, sizeof(player_two));
        for (update = 0U;
             update < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
             ++update) {
            if (!tecmo_gameplay_scene_update(
                    scene, &player_one, &player_two) ||
                scene->state.phase !=
                    TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
                return false;
            }
        }
        player_one.released.shoot = true;
        if (!tecmo_gameplay_scene_update(
                scene, &player_one, &player_two) ||
            scene->frame != checkpoint ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            !tecmo_gameplay_scene_free_throw_lineup(scene, &lineup) ||
            !tecmo_gameplay_scene_court_frame(scene, &court_frame) ||
            lineup.orientation != (uint8_t)free_throw_orientation ||
            lineup.shooter_slot != shooter ||
            lineup.secondary_slot != secondary ||
            lineup.actors[shooter].raw_world_x !=
                (uint16_t)(free_throw_orientation == 0 ? 250U : 518U) ||
            lineup.actors[shooter].raw_world_y != 148U ||
            court_frame.slice.direction !=
                (uint8_t)free_throw_orientation ||
            court_frame.slice.viewport.camera_x != camera_x ||
            court_frame.projection.camera_x != camera_x) {
            return false;
        }
        for (actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            if (!lineup.actors[actor].position_defined ||
                scene->actors[actor].position.x !=
                    (int16_t)lineup.actors[actor].raw_world_x ||
                scene->actors[actor].position.y !=
                    (int16_t)lineup.actors[actor].raw_world_y) {
                return false;
            }
        }
        return runtime->mode == TECMO_MODE_COURT && scene->active;
    }
    return true;
}

static bool run_gameplay_shot_checkpoint(TecmoRuntime *runtime, const TecmoCliGameplayCheckpointConfig *config)
{
    TecmoInput input;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const bool jump = config->jump;
    const bool jump_make = config->jump_make;
    const bool jump_rattle = config->jump_rattle;
    const bool dunk = config->dunk;
    runtime->gameplay_scene.frame = 0U;

    if (dunk) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];

        actor->position.x = 0x00AEU;
        actor->position.y = 160;
        actor->anchor.x = actor->position.x;
        actor->anchor.y = actor->position.y;
        /* Deliberate shot checkpoint setup; launch immediately replaces this
           with the validated offensive-hoop facing override. */
        actor->facing_right = true;
        runtime->gameplay_scene.ball_holder = 0U;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        runtime->gameplay_scene.ball_position.y_q8 =
            (int32_t)(actor->position.y - 18) * 256;
        runtime->gameplay_scene.camera_state.thresholds_valid = false;
        runtime->gameplay_scene.camera_state.endpoint_latched = false;
        if (!tecmo_gameplay_camera_settle_court(
                &runtime->gameplay_scene.camera_assets,
                &runtime->gameplay_scene.camera_state,
                &runtime->gameplay_scene.ball_position,
                runtime->gameplay_scene.orientation_state.current_direction,
                false)) {
            return false;
        }
    } else if (jump) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];
        actor->position.x = 0x013CU;
        actor->anchor.x = actor->position.x;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        if (jump_make) {
            actor->position.y = 180;
            actor->anchor.y = 180;
            runtime->gameplay_scene.ball_position.y_q8 =
                (int32_t)(180 - 18) * 256;
            runtime->gameplay_scene.action_serial = 0U;
        } else {
            if (!tecmo_gameplay_set_score(
                    &runtime->gameplay_scene.state,
                    TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
                return false;
            }
            runtime->gameplay_scene.action_serial = 1U;
        }
    }
    if (jump_rattle) {
        /* Explicit diagnostic selector setup; the production shot launch
           immediately resolves the actual offensive-hoop facing. */
        runtime->gameplay_scene.actors[0].facing_right = true;
        runtime->gameplay_scene.actors[0].movement_direction = 0U;
        if (!tecmo_gameplay_scene_start_rim_rattle_debug(
                &runtime->gameplay_scene)) {
            return false;
        }
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    for (update = 1U; update < checkpoint; ++update) {
        memset(&input, 0, sizeof(input));
        if (jump_make && update < 8U) input.cancel = true;
        tecmo_runtime_update(runtime, &input);
    }
    return runtime->mode == TECMO_MODE_COURT &&
           runtime->gameplay_scene.active &&
           runtime->gameplay_scene.shot_kind ==
               (jump &&
                        ((!jump_make && !jump_rattle &&
                          checkpoint == 87U) ||
                         (jump_rattle && checkpoint == 103U) ||
                         (jump_make && checkpoint == 111U))
                    ? TECMO_GAMEPLAY_SCENE_SHOT_NONE
                    : (dunk ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                            : TECMO_GAMEPLAY_SCENE_SHOT_JUMP));
}

bool tecmo_cli_setup_gameplay_render_checkpoint(TecmoRuntime *runtime, const char *mode_name)
{
    TecmoCliGameplayCheckpointConfig config;
    bool done;
    bool handled;

    if (runtime == NULL || mode_name == NULL ||
        !parse_gameplay_render_checkpoint_mode(mode_name, &config)) {
        return false;
    }
    if (!run_gameplay_checkpoint_preflight(runtime, &config, &done)) {
        return false;
    }
    if (done) {
        return !config.facing_checkpoint ||
               run_gameplay_facing_checkpoint(runtime, &config);
    }
    if (!run_gameplay_violation_checkpoint(runtime, &config, &handled)) {
        return false;
    }
    if (handled) return true;
    if (!run_gameplay_camera_checkpoint(runtime, &config, &handled)) {
        return false;
    }
    if (handled) return true;
    return run_gameplay_shot_checkpoint(runtime, &config);
}
