#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native shot, contact, and possession orchestration. */

static bool scene_shot_will_score(const TecmoGameplayScene *scene);

void scene_shot_clear_jump_playback(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    scene->jump_actor_altitude_q8 = 0U;
    scene->jump_actor_velocity_q8 = 0U;
    scene->jump_ball_altitude_q8 = 0U;
    scene->jump_ball_bounce_q8 = 0U;
    scene->jump_entry_pose_index = 0U;
    scene->jump_actor_state = 0U;
    scene->jump_ball_state = 0U;
    scene->jump_phase_counter = 0U;
    scene->jump_pose_frame = 0U;
    scene->shot_controller = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->jump_family = TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0;
    scene->jump_profile = TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0;
    scene->jump_direction = TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0;
    scene->jump_oracle_active = false;
    scene->jump_make_route = false;
    scene->jump_b_released = false;
    scene->jump_outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
    scene->jump_actor_landed = false;
    scene->jump_rim_rattle_debug = false;
    scene->jump_rim_rattle_raw_selector = 0U;
    scene->jump_rim_rattle_audio_repeats = 0U;
    memset(&scene->jump_rim_rattle, 0,
           sizeof(scene->jump_rim_rattle));
    memset(&scene->jump_made_settlement, 0,
           sizeof(scene->jump_made_settlement));
}

bool scene_shot_queue_result_audio(TecmoGameplayScene *scene,
                                   TecmoGameplayTeam shooting_team)
{
    TecmoGameplayAudioEvent side_result;
    if (scene == NULL ||
        (shooting_team != TECMO_GAMEPLAY_TEAM_AWAY &&
         shooting_team != TECMO_GAMEPLAY_TEAM_HOME) ||
        !tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE)) {
        return false;
    }

    /* Bank05 $AD01 requests ID 11 first. $B1D1 then overwrites the same
       one-byte mailbox with the pre-handoff shooting-side result when the
       clock is above 0:01. Only the final request is consumed. */
    if (scene->state.clock_minutes == 0U &&
        scene->state.clock_seconds < 2U) {
        return true;
    }
    side_result = shooting_team == TECMO_GAMEPLAY_TEAM_AWAY
                      ? TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_12
                      : TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_13;
    return tecmo_gameplay_audio_queue_event(&scene->audio_player, side_result);
}

static bool scene_jump_pose_for_context(const TecmoGameplayScene *scene,
                                        uint16_t *pose_index)
{
    if (scene == NULL || pose_index == NULL ||
        scene->jump_family != TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 ||
        scene->jump_profile != TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 ||
        scene->jump_direction != TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1) {
        return false;
    }
    return tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
        &scene->jump_shots, scene->jump_family, scene->jump_profile,
        scene->jump_direction, pose_index);
}

bool scene_shot_is_close(TecmoGameplaySceneShotKind kind)
{
    return kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
           kind == TECMO_GAMEPLAY_SCENE_SHOT_LAYUP;
}

static TecmoGameplayCloseShotVariant scene_close_variant(
    TecmoGameplaySceneShotKind kind)
{
    return kind == TECMO_GAMEPLAY_SCENE_SHOT_LAYUP
               ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2
               : TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
}

bool scene_close_pose_for_step(const TecmoGameplayScene *scene,
                                      uint8_t step,
                                      uint16_t *pose_index)
{
    TecmoGameplayCloseShotVariant variant;
    uint8_t phase;
    if (scene == NULL || pose_index == NULL ||
        scene->close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene->close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_shot_is_close(scene->shot_kind)) {
        return false;
    }
    variant = scene_close_variant(scene->shot_kind);
    return tecmo_gameplay_close_shots_phase_for_step(
               &scene->close_shots, variant, step, &phase) &&
           tecmo_gameplay_close_shots_resolve_pose_pointer_index(
               &scene->close_shots, variant, scene->close_shot_profile,
               scene->close_shot_direction, phase, pose_index);
}

static bool scene_resolve_actor_offensive_hoop(
    const TecmoGameplayScene *scene,
    uint8_t actor_index,
    TecmoGameplayCourtCoordinate *hoop_out,
    bool *facing_right_out)
{
    TecmoGameplayCourtCoordinate hoop;
    const TecmoGameplaySceneActor *actor;
    if (scene == NULL || hoop_out == NULL || facing_right_out == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.possession ||
        !tecmo_gameplay_court_orientation_hoop(
            &scene->court_orientation,
            scene->orientation_state.current_direction, &hoop)) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (!actor->active || actor->team != (uint8_t)scene->state.possession ||
        hoop.x != scene->orientation_state.offensive_hoop.x ||
        hoop.y != scene->orientation_state.offensive_hoop.y) {
        return false;
    }
    *hoop_out = hoop;
    *facing_right_out = actor->position.x == hoop.x
                            ? scene->orientation_state.current_direction != 0U
                            : actor->position.x < hoop.x;
    return true;
}

bool scene_start_shot_actor(TecmoGameplayScene *scene,
                                   size_t controller,
                                   uint8_t actor_index)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayCourtCoordinate offensive_hoop;
    TecmoGameplayCourtCoordinate shot_start;
    TecmoGameplayCourtCoordinate shot_end;
    TecmoGameplayCourtCoordinateQ8 shot_start_q8;
    TecmoGameplayCourtCoordinateQ8 shot_end_q8;
    uint16_t target_x;
    int approach_distance_x;
    int distance_y;
    uint8_t classified_points;
    bool close;
    bool shot_facing_right;
    TecmoGameplayCloseShotVariantInfo close_info;
    uint16_t entry_pose;
    uint16_t initial_pose = 0U;
    bool predicted_make = false;
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[actor_index].team != scene->state.possession) {
        return false;
    }
    if (scene->ball_holder != actor_index) {
        return false;
    }
    actor = &scene->actors[actor_index];
    entry_pose = actor->pose_index;
    if (!scene_resolve_actor_offensive_hoop(
            scene, actor_index, &offensive_hoop,
            &shot_facing_right)) {
        return false;
    }
    target_x = (uint16_t)offensive_hoop.x;
    shot_start.x = (int16_t)(
        actor->position.x + (shot_facing_right ? 7 : -7));
    shot_start.y = (int16_t)(actor->position.y - 18);
    shot_end.x = offensive_hoop.x;
    shot_end.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
    if (!tecmo_gameplay_court_coordinate_to_q8(
            &shot_start, &shot_start_q8) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &shot_end, &shot_end_q8)) {
        return false;
    }
    approach_distance_x =
        scene->orientation_state.current_direction == 0U
            ? actor->position.x - (int)target_x
            : (int)target_x - actor->position.x;
    distance_y = TECMO_GAMEPLAY_SHOT_TARGET_Y - actor->position.y;
    if (!tecmo_gameplay_shot_resolution_classify_point_value(
            &scene->shot_resolution, (uint16_t)actor->position.x,
            (uint8_t)actor->position.y,
            scene->orientation_state.current_direction, 0U,
            &classified_points)) {
        return false;
    }
    close = approach_distance_x >= -8 &&
            approach_distance_x <= TECMO_GAMEPLAY_CLOSE_DISTANCE_X &&
            distance_y >= -64 && distance_y <= 80;
    if (close) {
        scene_shot_clear_jump_playback(scene);
        /* The numeric ROM families and pose timing are exact. The distance
           threshold selecting between them remains a native scene policy. */
        scene->shot_kind = approach_distance_x <= 24
                               ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                               : TECMO_GAMEPLAY_SCENE_SHOT_LAYUP;
        /* Live TGCS support is intentionally narrowed to the exact numeric
           profile-0/direction-0 slice. Actor-facing mirroring is a native
           scene approximation; it is not a ROM direction-table mapping. */
        scene->close_shot_profile = TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0;
        scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
        scene->close_shot_step = 0U;
        if (!tecmo_gameplay_close_shots_get_variant_info(
                &scene->close_shots, scene_close_variant(scene->shot_kind),
                &close_info) ||
            !scene_close_pose_for_step(scene, 0U, &initial_pose)) {
            scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
            return false;
        }
    } else {
        /* The numeric playback remains the one captured family/profile/
           direction slice. The scene mirrors that bounded presentation toward
           the TGOR-owned offensive hoop for either manually controlled team;
           this facing adapter is native policy, not another ROM direction. */
        if (scene->launch.controller_team[controller] != actor->team) {
            return false;
        }
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
        memset(&close_info, 0, sizeof(close_info));
        scene_shot_clear_jump_playback(scene);
        scene->shot_controller = (uint8_t)controller;
        scene->jump_family = TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0;
        scene->jump_profile = TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0;
        scene->jump_direction = TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1;
        if (!scene_jump_pose_for_context(scene, &initial_pose)) {
            scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
            scene_shot_clear_jump_playback(scene);
            return false;
        }
    }
    scene->shot_actor = actor_index;
    scene->shot_frame = close ? 0U : 1U;
    scene->shot_points = classified_points;
    scene->shot_start_position = shot_start_q8;
    /* Capture the TGOR-selected endpoint once at launch. A later possession
       transition may change orientation, but it must not retarget flight. */
    scene->shot_end_position = shot_end_q8;
    scene->ball_position = scene->shot_start_position;
    scene->ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    ++scene->action_serial;
    predicted_make = !close && scene_shot_will_score(scene);
    /* The make capture is specifically the non-close three-point route.
       Deterministic two-point makes still have no bounded ordinary-jump
       schedule and therefore fail closed. */
    if (predicted_make && scene->shot_points != 3U) {
        --scene->action_serial;
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
        scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        scene->shot_frame = 0U;
        scene->shot_duration = 0U;
        scene->ball_holder = actor_index;
        if (!scene_attach_ball(scene)) return false;
        scene_shot_clear_jump_playback(scene);
        return false;
    }
    /* A supported shot is a deliberate action override. Keep this assignment
       after the unsupported predicted-make gate so rejection is transactional
       for the actor's facing and attached-ball side. */
    actor->facing_right = shot_facing_right;
    scene->shot_duration = scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                               ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                               : (close ? close_info.step_count
                                        : (predicted_make
                                               ? (uint16_t)(
                                                     TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
                                                     scene->jump_shots.constants.made_update_count)
                                               : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION));
    if (predicted_make) {
        initial_pose = TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE;
    } else if (!close) {
        /* Bank05 state $1E leaves $0442/$044D untouched for the first four
           pose ticks. Preserve the actor's actual entry pose instead of
           substituting the later $01AA flight pose. */
        initial_pose = entry_pose;
    }
    actor->pose_index = initial_pose;
    if (!close) {
        scene->jump_entry_pose_index = initial_pose;
        scene->jump_pose_frame = 1U;
        scene->jump_oracle_active = true;
        scene->jump_make_route = predicted_make;
        scene->jump_outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
        if (predicted_make) {
            scene->jump_actor_state =
                scene->jump_shots.constants.actor_state_gather;
            scene->jump_ball_state =
                scene->jump_shots.constants.ball_state_neutral;
            scene->jump_phase_counter =
                scene->jump_shots.constants.phase_seed_gather;
            scene->jump_actor_altitude_q8 = 0U;
            scene->jump_actor_velocity_q8 =
                TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8;
        } else {
            scene->jump_actor_state =
                scene->jump_shots.constants.actor_state_held;
            scene->jump_ball_state =
                scene->jump_shots.constants.ball_state_route1;
            scene->jump_phase_counter =
                scene->jump_shots.constants.phase_seed_prepared;
            scene->jump_actor_altitude_q8 =
                TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8;
            scene->jump_actor_velocity_q8 =
                TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8;
        }
    }
    return true;
}

bool scene_start_shot(TecmoGameplayScene *scene,
                             size_t controller)
{
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[controller] != scene->state.possession) {
        return false;
    }
    return scene_start_shot_actor(scene, controller,
                                  scene->controlled_actor[controller]);
}

bool tecmo_gameplay_scene_start_rim_rattle_debug(
    TecmoGameplayScene *scene)
{
    TecmoGameplayScene candidate;
    uint8_t actor;
    if (scene == NULL || !scene->available || !scene->active ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->launch.controller_team[0U] != TECMO_GAMEPLAY_TEAM_AWAY) {
        return false;
    }
    actor = scene->controlled_actor[0U];
    if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->ball_holder != actor ||
        scene->actors[actor].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        !scene->actors[actor].facing_right) {
        return false;
    }

    /* This is an explicit deterministic diagnostic setup, not a live selector
       or make/miss policy. Serial 2 is the already-covered native miss branch.
       Shot setup has several fail-closed branches after it starts mutating
       scene state, so stage the diagnostic in a shallow candidate. The setup
       performs no allocation and owns no external writes. */
    candidate = *scene;
    candidate.action_serial = 1U;
    if (!scene_start_shot_actor(&candidate, 0U, actor) ||
        candidate.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        candidate.jump_make_route ||
        candidate.shot_duration != TECMO_GAMEPLAY_JUMP_SLOT0_DURATION) {
        return false;
    }
    candidate.jump_rim_rattle_debug = true;
    candidate.jump_rim_rattle_raw_selector = 0x71U;
    candidate.shot_duration = TECMO_GAMEPLAY_JUMP_RATTLE_DURATION;
    *scene = candidate;
    return true;
}

static bool scene_shot_will_score(const TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *actor;
    uint32_t distance;
    uint32_t roll;
    uint32_t threshold;
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    /* Deterministic native shot-outcome policy. Distance, serial mixing, and
       thresholds are implementation-owned approximations, not ROM-derived
       make/miss behavior. */
    actor = &scene->actors[scene->shot_actor];
    distance = (uint32_t)(
        abs((int)scene->orientation_state.offensive_hoop.x - actor->position.x) +
        abs(TECMO_GAMEPLAY_SHOT_TARGET_Y - actor->position.y));
    roll = ((uint32_t)scene->action_serial * 37U +
            (uint32_t)scene->shot_actor * 11U + distance +
            (uint32_t)scene->state.score[0] * 3U +
            (uint32_t)scene->state.score[1] * 5U) % 100U;
    threshold = scene_shot_is_close(scene->shot_kind)
                    ? 82U
                    : (scene->shot_points == 3U ? 48U : 62U);
    return roll < threshold;
}

bool scene_handoff_possession(TecmoGameplayScene *scene,
                                     TecmoGameplayTeam possession,
                                     uint8_t preferred_actor)
{
    TecmoGameplayState state_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayBackcourtState backcourt_before;
    TecmoGameplayBackcourtState backcourt_reset;
    TecmoGameplayCourtCoordinateQ8 ball_before;
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t controlled_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t holder_before;
    uint8_t first = scene_first_actor_for_team(possession);
    uint8_t holder = preferred_actor;
    size_t controller;
    if (scene == NULL ||
        (possession != TECMO_GAMEPLAY_TEAM_AWAY &&
         possession != TECMO_GAMEPLAY_TEAM_HOME) ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state) ||
        !tecmo_gameplay_backcourt_state_initialize(
            &scene->backcourt_assets, &backcourt_reset)) {
        return false;
    }
    if (holder < first ||
        holder >= first + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        holder = first;
    }
    state_before = scene->state;
    orientation_before = scene->orientation_state;
    camera_before = scene->camera_state;
    backcourt_before = scene->backcourt_state;
    ball_before = scene->ball_position;
    holder_before = scene->ball_holder;
    memcpy(controlled_before, scene->controlled_actor,
           sizeof(controlled_before));
    if (scene->state.possession != possession &&
        !tecmo_gameplay_reset_possession(&scene->state, possession)) {
        scene->state = state_before;
        scene->orientation_state = orientation_before;
        return false;
    }
    if (scene->state.possession != possession ||
        !tecmo_gameplay_court_orientation_synchronize(
            &scene->court_orientation, &scene->orientation_state,
            (uint8_t)possession)) {
        scene->state = state_before;
        scene->orientation_state = orientation_before;
        return false;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    /* Possession changes establish a fresh effective baseline for every
       actor. Any later horizontal movement or shot pose can override it. */
    if (!scene_apply_goal_facing(scene, candidate_actors) ||
        !scene_ball_position_for_actors(
            scene, candidate_actors, holder, &candidate_ball)) {
        scene->state = state_before;
        scene->orientation_state = orientation_before;
        scene->camera_state = camera_before;
        scene->backcourt_state = backcourt_before;
        scene->ball_position = ball_before;
        scene->ball_holder = holder_before;
        memcpy(scene->controlled_actor, controlled_before,
               sizeof(controlled_before));
        return false;
    }
    if (scene->orientation_state.transition_serial !=
        orientation_before.transition_serial) {
        /* Preserve camera position/stream ownership across possession. The
           next single live follow recomputes direction-specific thresholds
           and may establish the opposite endpoint latch. */
        scene->camera_state.thresholds_valid = false;
        scene->camera_state.endpoint_latched = false;
    }
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    scene->ball_holder = holder;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == possession) {
            scene->controlled_actor[controller] = holder;
        }
    }
    scene->ball_position = candidate_ball;
    scene->backcourt_state = backcourt_reset;
    return true;
}

static bool scene_close_step_for_frame(const TecmoGameplayScene *scene,
                                       uint16_t frame,
                                       uint8_t *step)
{
    TecmoGameplayCloseShotVariantInfo info;
    uint16_t selected;
    if (scene == NULL || step == NULL ||
        !scene_shot_is_close(scene->shot_kind) ||
        !tecmo_gameplay_close_shots_get_variant_info(
            &scene->close_shots, scene_close_variant(scene->shot_kind),
            &info) || info.step_count == 0U) {
        return false;
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK) {
        selected = frame < info.step_count ? frame : info.step_count - 1U;
    } else if (frame <= 22U) {
        selected = frame;
    } else if (frame < TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME) {
        selected = 22U;
    } else {
        selected = (uint16_t)(22U + frame -
                              (TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME - 1U));
    }
    if (selected >= info.step_count) selected = info.step_count - 1U;
    *step = (uint8_t)selected;
    return true;
}

static bool scene_finish_shot(TecmoGameplayScene *scene,
                              TecmoGameplaySceneActor *actor,
                              TecmoGameplayTeam shooting_team,
                              bool made,
                              bool queue_side_result)
{
    TecmoGameplayMovementState movement;
    TecmoGameplayTeam next_team;
    uint16_t idle_pose;
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        actor != &scene->actors[scene->shot_actor] ||
        !scene_actor_movement_state(scene, actor, &movement) ||
        !scene_actor_movement_pose_index(
            scene, scene->actors, scene->shot_actor, &movement,
            &idle_pose)) {
        return false;
    }
    if (made) {
        if (!tecmo_gameplay_award_points(&scene->state, shooting_team,
                                         scene->shot_points)) {
            return false;
        }
        if (queue_side_result) {
            if (!scene_shot_queue_result_audio(scene, shooting_team)) return false;
        } else {
            /* The exact side-result ordering is proved for the dunk. Layups
               retain the crowd-only behavior. */
            (void)tecmo_gameplay_audio_queue_event(
                &scene->audio_player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE);
        }
    }
    actor->pose_index = idle_pose;
    next_team = scene_other_team(shooting_team);
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);
    if (scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        return scene_handoff_possession(
            scene, scene->state.possession,
            scene_first_actor_for_team(scene->state.possession));
    }
    return scene_handoff_possession(
        scene, next_team, scene_first_actor_for_team(next_team));
}

static bool scene_finish_jump_miss(TecmoGameplayScene *scene,
                                   TecmoGameplaySceneActor *actor,
                                   TecmoGameplayTeam shooting_team)
{
    TecmoGameplayShotSettlementDecision decision;
    TecmoGameplayTeam next_team;
    uint8_t claimant;
    uint8_t shooting_actor;
    bool period_expiry;
    if (scene == NULL || actor == NULL ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }

    period_expiry = scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
    shooting_actor = (uint8_t)(actor - scene->actors);
    next_team = scene_other_team(shooting_team);
    /* TGSR proves the claimant relation and handler decision, not the native
       scene's actor geometry. Nearest opposing actor is an explicit temporary
       claimant approximation. */
    claimant = scene_nearest_actor_for_team(
        scene, next_team, scene->shot_actor);
    if (!period_expiry &&
        (!tecmo_gameplay_shot_resolution_decide_claimant_settlement(
             &scene->shot_resolution, false,
             TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM, &decision) ||
         !decision.select_claimant ||
         !decision.replace_other_handler_with_previous ||
         !decision.change_possession)) {
        return false;
    }

    actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);

    if (period_expiry) {
        /* The caller queues the post-miss result after state events, so the
           zero-clock crowd request remains the final audio mailbox write. */
        return scene_handoff_possession(
            scene, scene->state.possession,
            shooting_actor);
    }
    return scene_handoff_possession(scene, next_team, claimant);
}

static int32_t scene_lerp_q8(int32_t start, int32_t end,
                             unsigned step, unsigned duration)
{
    if (duration == 0U || step >= duration) return end;
    return start + (int32_t)(
        ((int64_t)(end - start) * (int64_t)step) / (int64_t)duration);
}

static void scene_update_jump_ball_position(TecmoGameplayScene *scene)
{
    uint16_t frame = scene->shot_frame;
    int32_t apex_y =
        (scene->shot_start_position.y_q8 < scene->shot_end_position.y_q8
             ? scene->shot_start_position.y_q8
             : scene->shot_end_position.y_q8) -
                     34 * 256;
    if (frame <= 4U) {
        scene->ball_position.x_q8 = scene->shot_start_position.x_q8;
        scene->ball_position.y_q8 = scene->shot_start_position.y_q8;
    } else if (frame <= 32U) {
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(frame - 4U), 69U);
        scene->ball_position.y_q8 = scene_lerp_q8(
            scene->shot_start_position.y_q8, apex_y,
            (unsigned)(frame - 4U), 28U);
    } else if (frame == 33U) {
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            29U, 69U);
        scene->ball_position.y_q8 = apex_y;
    } else if (frame <= 73U) {
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(frame - 4U), 69U);
        scene->ball_position.y_q8 = scene_lerp_q8(
            apex_y, scene->shot_end_position.y_q8,
            (unsigned)(frame - 33U), 40U);
    } else {
        scene->ball_position.x_q8 = scene->shot_end_position.x_q8;
        scene->ball_position.y_q8 = scene->shot_end_position.y_q8;
    }
}

static bool scene_map_rim_rattle_ball_position(
    TecmoGameplayScene *scene,
    const TecmoGameplayShotRimRattle *rattle)
{
    const TecmoGameplayCloseShotSourceSpan *source;
    uint16_t source_target_x;
    uint16_t source_snap_x;
    int32_t relative_x;
    int32_t relative_y;
    if (scene == NULL || rattle == NULL ||
        rattle->orientation >=
            TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT) {
        return false;
    }
    source = tecmo_gameplay_close_shots_find_source(
        &scene->close_shots,
        TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BDEF_BDF6);
    if (source == NULL || source->bytes == NULL ||
        source->byte_count != 8U || source->cpu_start != 0xBDEFU ||
        source->cpu_end != 0xBDF6U) {
        return false;
    }
    source_target_x = (uint16_t)(
        (uint16_t)source->bytes[rattle->orientation] |
        ((uint16_t)source->bytes[2U + rattle->orientation] << 8U));
    source_snap_x = (uint16_t)(
        (uint16_t)source->bytes[4U + rattle->orientation] |
        ((uint16_t)source->bytes[6U + rattle->orientation] << 8U));
    if (source_snap_x !=
            scene->shot_resolution.rim_rattle.orientation_start_x[
                rattle->orientation] ||
        scene->shot_resolution.rim_rattle.start_y !=
            TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y + 4U) {
        return false;
    }
    relative_x = (int32_t)rattle->x - (int32_t)source_target_x;
    relative_y = (int32_t)rattle->y -
                 TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y;
    scene->ball_position.x_q8 =
        scene->shot_end_position.x_q8 + relative_x * 256;
    scene->ball_position.y_q8 =
        scene->shot_end_position.y_q8 + relative_y * 256;
    return true;
}

static void scene_update_jump_make_ball_position(TecmoGameplayScene *scene)
{
    uint16_t frame = scene->shot_frame;
    int32_t apex_y =
        (scene->shot_start_position.y_q8 < scene->shot_end_position.y_q8
             ? scene->shot_start_position.y_q8
             : scene->shot_end_position.y_q8) -
                     34 * 256;
    if (frame <= TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME) {
        scene->ball_position.x_q8 = scene->shot_start_position.x_q8;
        scene->ball_position.y_q8 = scene->shot_start_position.y_q8;
    } else if (frame <= 47U) {
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(frame - TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME),
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME -
                TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME);
        scene->ball_position.y_q8 = scene_lerp_q8(
            scene->shot_start_position.y_q8, apex_y,
            (unsigned)(frame - TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME),
            38U);
    } else if (frame < TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) {
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(frame - TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME),
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME -
                TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME);
        scene->ball_position.y_q8 = scene_lerp_q8(
            apex_y, scene->shot_end_position.y_q8,
            (unsigned)(frame - 47U),
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME - 47U);
    } else {
        scene->ball_position.x_q8 = scene->shot_end_position.x_q8;
        scene->ball_position.y_q8 = scene->shot_end_position.y_q8;
    }
}

static bool scene_finish_jump_make(TecmoGameplayScene *scene,
                                   TecmoGameplaySceneActor *actor,
                                   TecmoGameplayTeam shooting_team)
{
    TecmoGameplayTeam next_team;
    uint8_t shooting_actor;
    bool period_expiry;
    if (scene == NULL || actor == NULL ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
        scene->shot_frame != (uint16_t)(
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
            scene->jump_made_settlement.updates) ||
        !scene->jump_made_settlement.complete ||
        (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
         scene->state.phase !=
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) ||
        !tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE)) {
        return false;
    }
    period_expiry = scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
    shooting_actor = (uint8_t)(actor - scene->actors);
    next_team = scene_other_team(shooting_team);
    actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);
    if (period_expiry) {
        return scene_handoff_possession(
            scene, scene->state.possession, shooting_actor);
    }
    return scene_handoff_possession(
        scene, next_team, scene_first_actor_for_team(next_team));
}

static void scene_release_jump_make(TecmoGameplayScene *scene,
                                    TecmoGameplaySceneActor *actor)
{
    scene->jump_b_released = true;
    scene->shot_frame = TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME;
    scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME;
    scene->jump_phase_counter =
        scene->jump_shots.constants.phase_seed_gather;
    actor->pose_index = TECMO_GAMEPLAY_JUMP_RELEASE_POSE;
    scene_update_jump_make_ball_position(scene);
}

static bool scene_update_jump_make(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    static const uint8_t release_phases[8] = {
        0x31U, 0x21U, 0x11U, 0x01U,
        0x32U, 0x22U, 0x12U, 0x02U
    };
    TecmoGameplaySceneActor *actor;
    TecmoGameplayShotOutcome outcome =
        TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
    uint16_t next_frame;
    bool landed = false;
    if (!scene->jump_oracle_active || !scene->jump_make_route ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration != (uint16_t)(
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
            scene->jump_shots.constants.made_update_count) ||
        scene->shot_frame == 0U ||
        scene->shot_frame > scene->shot_duration ||
        scene->shot_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[scene->shot_controller] !=
            scene->actors[scene->shot_actor].team) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];

    if (!scene->jump_b_released) {
        if (scene->shot_frame < 8U) {
            if (shooting_controls == NULL ||
                !shooting_controls->held.cancel) {
                /* The only bounded make capture holds B through frame 8.
                   Normalize an earlier release onto its frame-9 transition
                   so ordinary controller input cannot strand live play. */
                scene_release_jump_make(scene, actor);
                return true;
            }
            ++scene->shot_frame;
            scene->jump_pose_frame = (uint8_t)scene->shot_frame;
            if (scene->shot_frame == 5U) {
                actor->pose_index = TECMO_GAMEPLAY_JUMP_TURN_POSE;
                scene->jump_phase_counter =
                    scene->jump_shots.constants.phase_seed_gather;
            } else {
                scene->jump_phase_counter =
                    (uint8_t)(scene->jump_phase_counter - 0x10U);
            }
            return true;
        }
        if (scene->shot_frame != 8U) return false;
        if (shooting_controls != NULL &&
            shooting_controls->held.cancel) {
            return true;
        }
        scene_release_jump_make(scene, actor);
        return true;
    }

    next_frame = (uint16_t)(scene->shot_frame + 1U);
    if (next_frame > scene->shot_duration) return false;
    scene->shot_frame = next_frame;
    if (next_frame >= 10U && next_frame <= 17U) {
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_prepared;
        scene->jump_phase_counter = release_phases[next_frame - 10U];
        actor->pose_index = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE;
    } else if (next_frame == 18U) {
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_held;
        scene->jump_phase_counter = 0x34U;
    } else if (next_frame == TECMO_GAMEPLAY_JUMP_MAKE_DECISION_FRAME) {
        if (scene->jump_shots.constants.outcome_flag_mask !=
                scene->shot_resolution.outcome_flag_mask ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                &scene->shot_resolution, true, 0U, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) {
            return false;
        }
        scene->jump_outcome = outcome;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_airborne;
        scene->jump_phase_counter = 0x35U;
    }

    if (next_frame >= TECMO_GAMEPLAY_JUMP_MAKE_FLIGHT_FRAME &&
        next_frame <= TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME &&
        !scene->jump_actor_landed) {
        if (!tecmo_gameplay_jump_shots_step_q8(
                &scene->jump_shots, &scene->jump_actor_altitude_q8,
                &scene->jump_actor_velocity_q8, &landed)) {
            return false;
        }
        scene->jump_actor_landed = landed;
    }
    if (next_frame == TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME) {
        if (!scene->jump_actor_landed ||
            scene->jump_actor_altitude_q8 != 0U ||
            scene->jump_actor_velocity_q8 != 0U) {
            return false;
        }
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_recovery;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_recovery_counter;
    } else if (next_frame >= 58U && next_frame <= 62U) {
        if (scene->jump_actor_state !=
                scene->jump_shots.constants.actor_state_recovery ||
            scene->jump_phase_counter < 0x10U) {
            return false;
        }
        scene->jump_phase_counter =
            (uint8_t)(scene->jump_phase_counter - 0x10U);
    } else if (next_frame == TECMO_GAMEPLAY_JUMP_MAKE_NEUTRAL_FRAME) {
        if (scene->jump_phase_counter != 0x06U) return false;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_neutral;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_gather;
        actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
        scene->jump_pose_frame = 0U;
    }

    scene_update_jump_make_ball_position(scene);
    if (next_frame == TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) {
        TecmoGameplayState state_before = scene->state;
        TecmoGameplayJumpShotMadeSettlement settlement;
        bool period_expiry = scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
        if (scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
            scene->shot_points != 3U ||
            (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
             !period_expiry) ||
            !tecmo_gameplay_jump_shots_made_settlement_begin(
                &scene->jump_shots, &settlement) ||
            !tecmo_gameplay_award_points(
                &scene->state, (TecmoGameplayTeam)actor->team,
                scene->shot_points) ||
            (!period_expiry &&
             !tecmo_gameplay_reset_possession(
                 &scene->state, (TecmoGameplayTeam)actor->team))) {
            scene->state = state_before;
            return false;
        }
        scene->jump_made_settlement = settlement;
    } else if (next_frame > TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) {
        if (!tecmo_gameplay_jump_shots_made_settlement_step(
                &scene->jump_shots, &scene->jump_made_settlement, false)) {
            return false;
        }
    }
    if (!scene->jump_made_settlement.complete) return true;
    if (scene->shot_duration != (uint16_t)(
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
            scene->jump_shots.constants.made_update_count) ||
        scene->jump_made_settlement.updates !=
            scene->jump_shots.constants.made_update_count) {
        return false;
    }
    return scene_finish_jump_make(
        scene, actor, (TecmoGameplayTeam)actor->team);
}

static bool scene_update_jump_miss_rim_rattle(
    TecmoGameplayScene *scene,
    uint16_t next_frame,
    uint16_t *route_frame,
    bool *rattle_position_owned);

bool scene_update_jump_miss(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayShotOutcome outcome;
    uint16_t next_frame;
    uint16_t route_frame;
    bool landed = false;
    bool rattle_position_owned = false;
    if (!scene->jump_oracle_active || scene->jump_make_route ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration !=
            (scene->jump_rim_rattle_debug
                 ? TECMO_GAMEPLAY_JUMP_RATTLE_DURATION
                 : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION) ||
        scene->shot_frame == 0U ||
        scene->shot_frame > scene->shot_duration ||
        scene->shot_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[scene->shot_controller] !=
            scene->actors[scene->shot_actor].team ||
        (!scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) ||
        (scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS)) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    if (!scene->jump_b_released) {
        uint16_t expected_pose;
        if (scene->jump_pose_frame == 0U ||
            scene->jump_pose_frame >
                TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME) {
            return false;
        }
        expected_pose = scene->jump_pose_frame <=
                                TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME
                            ? scene->jump_entry_pose_index
                            : TECMO_GAMEPLAY_JUMP_TURN_POSE;
        if (actor->pose_index != expected_pose) return false;
        /* Bank05 tests the current NES B level. No previous/released edge and
           no DMC request participates in this transition. */
        if (shooting_controls != NULL && shooting_controls->held.cancel) {
            if (scene->shot_frame != 1U ||
                scene->jump_actor_state !=
                    scene->jump_shots.constants.actor_state_held ||
                scene->jump_ball_state !=
                    scene->jump_shots.constants.ball_state_route1) {
                return false;
            }
            if (scene->jump_pose_frame <
                TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME) {
                ++scene->jump_pose_frame;
                actor->pose_index = scene->jump_pose_frame <
                                            TECMO_GAMEPLAY_JUMP_TURN_POSE_FIRST_FRAME
                                        ? scene->jump_entry_pose_index
                                        : TECMO_GAMEPLAY_JUMP_TURN_POSE;
            }
            return true;
        }
        if (scene->shot_frame != 1U) return false;
        outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
        if (scene->jump_shots.constants.outcome_flag_mask !=
                scene->shot_resolution.outcome_flag_mask ||
            !tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                &scene->shot_resolution, true,
                scene->jump_shots.constants.outcome_flag_mask, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
            return false;
        }
        scene->jump_b_released = true;
        scene->jump_outcome = outcome;
        scene->shot_frame = 2U;
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME;
        actor->pose_index = TECMO_GAMEPLAY_JUMP_RELEASE_POSE;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_airborne;
        scene->jump_ball_state = scene->jump_shots.constants.ball_state_route5;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_airborne;
        scene->jump_actor_velocity_q8 =
            TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8;
        scene_update_jump_ball_position(scene);
        return true;
    }

    next_frame = (uint16_t)(scene->shot_frame + 1U);
    if (next_frame > scene->shot_duration) return false;
    if ((scene->shot_frame == 2U &&
         (scene->jump_pose_frame !=
              TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME ||
          actor->pose_index != TECMO_GAMEPLAY_JUMP_RELEASE_POSE)) ||
        (scene->shot_frame >= 3U && scene->shot_frame < 46U &&
         (scene->jump_pose_frame !=
              TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME ||
          actor->pose_index != TECMO_GAMEPLAY_JUMP_FLIGHT_POSE)) ||
        (scene->shot_frame >= 46U &&
         (scene->jump_pose_frame != 0U ||
          actor->pose_index != TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE))) {
        return false;
    }
    scene->shot_frame = next_frame;
    route_frame = next_frame;

    if (next_frame == 3U) {
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
        actor->pose_index = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE;
    }

    if (next_frame >= 4U && next_frame <= 40U &&
        !scene->jump_actor_landed) {
        if (!tecmo_gameplay_jump_shots_step_q8(
                &scene->jump_shots, &scene->jump_actor_altitude_q8,
                &scene->jump_actor_velocity_q8, &landed)) {
            return false;
        }
        scene->jump_actor_landed = landed;
    }
    if (next_frame == 40U) {
        if (!scene->jump_actor_landed ||
            scene->jump_actor_altitude_q8 != 0U ||
            scene->jump_actor_velocity_q8 != 0U) {
            return false;
        }
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_recovery;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_recovery_counter;
    } else if (next_frame >= 41U && next_frame <= 45U) {
        if (scene->jump_actor_state !=
                scene->jump_shots.constants.actor_state_recovery ||
            scene->jump_phase_counter < 0x10U) {
            return false;
        }
        scene->jump_phase_counter =
            (uint8_t)(scene->jump_phase_counter - 0x10U);
    } else if (next_frame == 46U) {
        if (scene->jump_phase_counter != 0x06U) return false;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_neutral;
        scene->jump_phase_counter = 0U;
        actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
        scene->jump_pose_frame = 0U;
    }

    if (!scene_update_jump_miss_rim_rattle(
            scene, next_frame, &route_frame, &rattle_position_owned)) {
        return false;
    }

    if (route_frame == 5U) {
        scene->jump_ball_state =
            scene->jump_shots.constants.ball_state_route17;
    } else if (route_frame == 73U) {
        scene->jump_ball_state =
            scene->jump_shots.constants.ball_state_route10;
    } else if (route_frame == 74U) {
        scene->jump_ball_altitude_q8 = 0U;
        scene->jump_ball_bounce_q8 =
            scene->jump_shots.constants.bounce_decay_q8;
    } else if (route_frame == 75U) {
        if (scene->jump_ball_state ==
                scene->jump_shots.constants.ball_state_route10 &&
            scene->jump_ball_altitude_q8 == 0U &&
            scene->jump_ball_bounce_q8 != 0U) {
            (void)tecmo_gameplay_audio_queue_event(
                &scene->audio_player,
                TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE);
            scene->jump_ball_bounce_q8 = (uint16_t)(
                scene->jump_ball_bounce_q8 -
                scene->jump_shots.constants.bounce_decay_q8);
        }
    }
    if (!rattle_position_owned) {
        scene_update_jump_ball_position(scene);
    }

    if (next_frame < scene->shot_duration) return true;
    if (scene->jump_actor_state !=
            scene->jump_shots.constants.actor_state_neutral ||
        scene->jump_ball_state !=
            scene->jump_shots.constants.ball_state_route10) {
        return false;
    }
    return scene_finish_jump_miss(
        scene, actor, (TecmoGameplayTeam)actor->team);
}

static bool scene_update_jump_miss_rim_rattle(
    TecmoGameplayScene *scene,
    uint16_t next_frame,
    uint16_t *route_frame,
    bool *rattle_position_owned)
{
    bool repeat_dmc = false;
    bool rattle_completed = false;

    if (scene->jump_rim_rattle_debug &&
        next_frame == TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME) {
        TecmoGameplayShotRimRoute route;
        if (!tecmo_gameplay_shot_resolution_resolve_rim_route(
                &scene->shot_resolution,
                scene->jump_rim_rattle_raw_selector, &route) ||
            route.kind != TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 ||
            route.source_target_cpu != 0xA7A9U ||
            !tecmo_gameplay_shot_rim_rattle_begin(
                &scene->shot_resolution, &scene->jump_rim_rattle,
                0U, 3U, scene->jump_phase_counter,
                TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6,
                0)) {
            return false;
        }
        scene->jump_ball_state =
            scene->jump_rim_rattle.object_state;
        scene->jump_ball_altitude_q8 =
            (uint16_t)scene->jump_rim_rattle.altitude << 8U;
        if (!scene_map_rim_rattle_ball_position(
                scene, &scene->jump_rim_rattle)) {
            return false;
        }
        *route_frame = 0U;
        *rattle_position_owned = true;
    } else if (scene->jump_rim_rattle_debug &&
               scene->jump_rim_rattle.active) {
        if (!tecmo_gameplay_shot_rim_rattle_step(
                &scene->shot_resolution, &scene->jump_rim_rattle,
                &repeat_dmc, &rattle_completed)) {
            return false;
        }
        if (!scene_map_rim_rattle_ball_position(
                scene, &scene->jump_rim_rattle)) {
            return false;
        }
        scene->jump_ball_altitude_q8 =
            (uint16_t)scene->jump_rim_rattle.altitude << 8U;
        *rattle_position_owned = true;
        if (repeat_dmc) {
            if (scene->shot_resolution.rim_rattle.repeat_dmc_length !=
                    0x0AU ||
                !tecmo_gameplay_audio_queue_dmc_clip(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT)) {
                return false;
            }
            ++scene->jump_rim_rattle_audio_repeats;
        }
        if (rattle_completed) {
            /* The canonical diagnostic uses observed raw $6A=$71, so $A2DF's
               raw-selector >= $18 predicate selects the existing state-$10
               path. Other terminal predicates remain unsupported here. */
            if (next_frame != TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME ||
                scene->jump_rim_rattle_raw_selector < 0x18U ||
                scene->jump_rim_rattle.horizontal_velocity_q6 !=
                    scene->jump_rim_rattle.saved_horizontal_velocity_q6 ||
                scene->jump_rim_rattle.vertical_velocity_q6 !=
                    scene->jump_rim_rattle.saved_vertical_velocity_q6) {
                return false;
            }
            *route_frame = TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME;
        }
    } else if (scene->jump_rim_rattle_debug &&
               next_frame > TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        *route_frame = (uint16_t)(
            next_frame - TECMO_GAMEPLAY_JUMP_RATTLE_FRAME_SHIFT);
    }
    return true;
}

static bool scene_update_jump_shot(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    if (scene == NULL) return false;
    return scene->jump_make_route
               ? scene_update_jump_make(scene, shooting_controls)
               : scene_update_jump_miss(scene, shooting_controls);
}

bool scene_update_shot(TecmoGameplayScene *scene,
                              const TecmoControlFrame *shooting_controls)
{
    int64_t duration;
    int64_t frame;
    int64_t arc;
    TecmoGameplaySceneActor *actor;
    TecmoGameplayTeam shooting_team;
    TecmoGameplaySceneShotKind shot_kind;
    bool made;
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration == 0U) {
        return false;
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        return scene_update_jump_shot(scene, shooting_controls);
    }
    actor = &scene->actors[scene->shot_actor];
    shooting_team = (TecmoGameplayTeam)actor->team;
    shot_kind = scene->shot_kind;
    ++scene->shot_frame;
    duration = scene->shot_duration;
    frame = scene->shot_frame < scene->shot_duration
                ? scene->shot_frame
                : scene->shot_duration;
    scene->ball_position.x_q8 = scene->shot_start_position.x_q8 +
        (int32_t)(((int64_t)(scene->shot_end_position.x_q8 -
                             scene->shot_start_position.x_q8) * frame) / duration);
    scene->ball_position.y_q8 = scene->shot_start_position.y_q8 +
        (int32_t)(((int64_t)(scene->shot_end_position.y_q8 -
                             scene->shot_start_position.y_q8) * frame) / duration);
    arc = (4LL * frame * (duration - frame) *
           (scene_shot_is_close(scene->shot_kind)
                ? 18LL
                : 34LL) * 256LL) /
          (duration * duration);
    scene->ball_position.y_q8 -= (int32_t)arc;

    {
        uint16_t pose_index;
        if (!scene_close_step_for_frame(scene, scene->shot_frame,
                                        &scene->close_shot_step)) {
            return false;
        }
        if (!scene_close_pose_for_step(scene, scene->close_shot_step,
                                       &pose_index)) {
            return false;
        }
        actor->pose_index = pose_index;
        /* TGCS supplies one exact numeric pose phase per native scene step.
           Do not advance the unrelated bounded rightward trace on an invented
           cadence; the pure-state semantic chain remains untouched. */
    }

    if (shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK &&
        scene->shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME) {
        (void)tecmo_gameplay_audio_queue_dmc_clip(
            &scene->audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A9C5);
    }

    if (scene->shot_frame < scene->shot_duration) return true;
    made = scene_shot_will_score(scene);
    return scene_finish_shot(
        scene, actor, shooting_team, made,
        shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK);
}

bool scene_try_defense_action(TecmoGameplayScene *scene,
                                     size_t controller)
{
    uint8_t defender;
    TecmoGameplayTeam defending_team;
    const TecmoGameplaySceneActor *holder;
    const TecmoGameplaySceneActor *defender_actor;
    uint32_t distance;
    /* Deterministic native contact/steal/foul policy. Distance and action-
       serial branches are implementation-owned approximations, not ROM-exact
       collision or penalty detection. */
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    defending_team =
        (TecmoGameplayTeam)scene->launch.controller_team[controller];
    if (defending_team == scene->state.possession) return false;
    defender = scene->controlled_actor[controller];
    if (defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    holder = &scene->actors[scene->ball_holder];
    defender_actor = &scene->actors[defender];
    distance = scene_distance_squared(defender_actor, holder);
    ++scene->action_serial;
    if (distance > 22U * 22U) return true;
    if (scene->action_serial % 4U == 0U) {
        TecmoGameplayFoulRequest request;
        request.fouling_team = defending_team;
        request.free_throw_team = scene_other_team(defending_team);
        request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
        request.player_index = defender_actor->roster_index;
        request.free_throw_attempts = 2U;
        if (!tecmo_gameplay_request_foul(&scene->state, &request)) {
            return false;
        }
        scene->free_throw_frame = 0U;
    } else if (scene->action_serial % 2U == 0U) {
        if (!scene_handoff_possession(scene, defending_team, defender)) {
            return false;
        }
    }
    return true;
}
