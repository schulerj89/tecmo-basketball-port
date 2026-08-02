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

/* Native actor locomotion, ball attachment, and CPU steering integration. */

bool scene_movement_pose_index(
    const TecmoGameplayScene *scene,
    const TecmoGameplayMovementState *movement,
    const TecmoGameplayCourtCoordinate *linked_position,
    uint16_t *pose_index_out)
{
    bool alternate_pose_half;
    if (scene == NULL || movement == NULL || linked_position == NULL ||
        pose_index_out == NULL ||
        !tecmo_gameplay_movement_pose_half(
            &scene->movement_assets, movement, linked_position,
            &alternate_pose_half)) {
        return false;
    }
    return tecmo_gameplay_movement_pose_index(
        &scene->movement_assets, movement, alternate_pose_half,
        pose_index_out);
}

bool scene_actor_movement_pose_index(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    size_t actor_index,
    const TecmoGameplayMovementState *movement,
    uint16_t *pose_index_out)
{
    uint8_t linked_actor;
    if (scene == NULL || actors == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    linked_actor = scene->cpu_actors[actor_index].linked_actor;
    return linked_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           linked_actor != actor_index &&
           scene_movement_pose_index(
               scene, movement, &actors[linked_actor].position,
               pose_index_out);
}

TecmoGameplayTeam scene_other_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? TECMO_GAMEPLAY_TEAM_HOME
               : TECMO_GAMEPLAY_TEAM_AWAY;
}

bool scene_actor_coordinate_valid(
    const TecmoGameplayCourtCoordinate *coordinate)
{
    int half_y;
    int left;
    int right;
    if (!tecmo_gameplay_court_coordinate_valid(coordinate) ||
        coordinate->y < TECMO_GAMEPLAY_MIN_Y ||
        coordinate->y > TECMO_GAMEPLAY_MAX_Y) {
        return false;
    }
    half_y = coordinate->y / 2;
    left = TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - half_y;
    right = TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + half_y;
    return coordinate->x >= left && coordinate->x <= right;
}

bool scene_actor_world_position_valid(
    const TecmoGameplaySceneActor *actor)
{
    return actor != NULL &&
           scene_actor_coordinate_valid(&actor->position);
}

void scene_clamp_actor_world(TecmoGameplaySceneActor *actor)
{
    int16_t left_boundary;
    int16_t right_boundary;
    uint16_t half_y;
    if (actor == NULL) return;
    if (actor->position.y < TECMO_GAMEPLAY_MIN_Y) {
        actor->position.y = TECMO_GAMEPLAY_MIN_Y;
    }
    if (actor->position.y > TECMO_GAMEPLAY_MAX_Y) {
        actor->position.y = TECMO_GAMEPLAY_MAX_Y;
    }
    half_y = (uint16_t)actor->position.y / 2U;
    left_boundary =
        (int16_t)(TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - half_y);
    right_boundary =
        (int16_t)(TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + half_y);
    /* Presentation staging retains this scene-safety helper. Ordinary human
       and CPU movement goes through TGMO-1, including the exact fixed-bank
       dispatcher exclusions and primary/secondary actor clamp rules. */
    if (actor->position.x < left_boundary) actor->position.x = left_boundary;
    if (actor->position.x > right_boundary) actor->position.x = right_boundary;
    if (actor->position.x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X) {
        actor->position.x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    }
}

static const TecmoTeamDataPlayer *scene_actor_player(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor)
{
    uint8_t team_id;
    if (scene == NULL || actor == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        actor->team > TECMO_GAMEPLAY_TEAM_HOME ||
        actor->roster_index >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        return NULL;
    }
    team_id = actor->team == TECMO_GAMEPLAY_TEAM_AWAY
                  ? scene->launch.away_team : scene->launch.home_team;
    if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT) return NULL;
    return &scene->pretip_team_data->players[team_id]
                                                [actor->roster_index];
}

bool scene_actor_movement_state(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor,
    TecmoGameplayMovementState *state_out)
{
    TecmoGameplayMovementState state;
    const TecmoTeamDataPlayer *player;
    int adjusted_rating;
    if (scene == NULL || actor == NULL || state_out == NULL ||
        !scene->movement_assets.available ||
        actor->condition > 0x64U ||
        scene->launch.speed_value >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT) {
        return false;
    }
    player = scene_actor_player(scene, actor);
    if (player == NULL) return false;
    adjusted_rating = (int)player->profile[0] +
        scene->movement_assets.speed_adjustment[scene->launch.speed_value];
    if (adjusted_rating <
            (int)scene->movement_assets.minimum_movement_amount ||
        adjusted_rating > 0xFF) {
        return false;
    }
    memset(&state, 0, sizeof(state));
    state.contract_tag = TECMO_GAMEPLAY_MOVEMENT_STATE_TAG;
    state.position = actor->position;
    state.action_state = actor->movement_action_state;
    state.direction = actor->movement_direction;
    state.fractional_accumulator =
        actor->movement_fractional_accumulator;
    state.animation_phase = actor->movement_animation_phase;
    state.boundary_violation_latched =
        actor->movement_boundary_latched;
    if (!tecmo_gameplay_movement_state_valid(
            &scene->movement_assets, &state)) {
        return false;
    }
    *state_out = state;
    return true;
}

bool scene_live_ball_frame_for_actors(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    uint8_t holder_index,
    TecmoGameplayBallDribbleFrame *frame_out)
{
    TecmoGameplayMovementState movement;
    uint8_t linked_actor;
    if (scene == NULL || actors == NULL || frame_out == NULL ||
        !scene->ball_dribble_assets.available ||
        holder_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !actors[holder_index].active ||
        !scene_actor_movement_state(
            scene, &actors[holder_index], &movement)) {
        return false;
    }
    linked_actor = scene->cpu_actors[holder_index].linked_actor;
    return linked_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           linked_actor != holder_index && actors[linked_actor].active &&
           tecmo_gameplay_ball_dribble_resolve(
               &scene->ball_dribble_assets, &scene->movement_assets,
               &movement, &actors[linked_actor].position, frame_out);
}

static bool scene_actor_apply_movement(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    size_t actor_index,
    const TecmoGameplayMovementState *movement,
    uint8_t held_direction_bits)
{
    TecmoGameplaySceneActor *actor;
    uint16_t pose_index;
    if (scene == NULL || actors == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        movement == NULL ||
        !tecmo_gameplay_movement_input_valid(held_direction_bits) ||
        !tecmo_gameplay_movement_state_valid(
            &scene->movement_assets, movement) ||
        !scene_actor_movement_pose_index(
            scene, actors, actor_index, movement, &pose_index)) {
        return false;
    }
    actor = &actors[actor_index];
    actor->position = movement->position;
    actor->movement_action_state = movement->action_state;
    actor->movement_direction = movement->direction;
    actor->movement_fractional_accumulator =
        movement->fractional_accumulator;
    actor->movement_animation_phase = movement->animation_phase;
    actor->movement_boundary_latched =
        movement->boundary_violation_latched;
    actor->pose_index = pose_index;
    if ((held_direction_bits & TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT) != 0U) {
        actor->facing_right = true;
    } else if ((held_direction_bits &
                TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT) != 0U) {
        actor->facing_right = false;
    }
    return true;
}

bool scene_move_controlled_actor(TecmoGameplayScene *scene,
                                        size_t controller,
                                        const TecmoControlFrame *controls)
{
    uint8_t actor_index;
    uint8_t direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    TecmoGameplayMovementState movement;
    TecmoGameplayMovementStepInput input;
    if (scene == NULL || controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        return false;
    }
    if (scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM) {
        return scene->controlled_actor[controller] ==
               TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    }
    actor_index = scene->controlled_actor[controller];
    if (actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    actor = &scene->actors[actor_index];
    if (!actor->active ||
        actor->team != scene->launch.controller_team[controller] ||
        actor->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        !scene_actor_movement_state(scene, actor, &movement)) {
        return false;
    }
    player = scene_actor_player(scene, actor);
    if (player == NULL) return false;
    if (controls != NULL) {
        if (controls->held.right && !controls->held.left) {
            direction_bits |= TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
        } else if (controls->held.left && !controls->held.right) {
            direction_bits |= TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT;
        }
        if (controls->held.down && !controls->held.up) {
            direction_bits |= TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN;
        } else if (controls->held.up && !controls->held.down) {
            direction_bits |= TECMO_GAMEPLAY_MOVEMENT_INPUT_UP;
        }
    }
    memset(&input, 0, sizeof(input));
    input.held_direction_bits = direction_bits;
    input.player_movement_rating = player->profile[0];
    input.condition = actor->condition;
    input.speed_value = scene->launch.speed_value;
    /* Ordinary live control currently maps to the ROM's state-0 selected
       actor path. TGMO retains the other dispatcher cases for later actions. */
    input.global_object_state = 0U;
    input.movement_flags = 0U;
    input.primary_selected_actor = actor_index == scene->ball_holder;
    if (!tecmo_gameplay_movement_step(
            &scene->movement_assets, &movement, &input) ||
        !scene_actor_apply_movement(
            scene, scene->actors, actor_index, &movement,
            direction_bits)) {
        return false;
    }
    return true;
}

uint8_t scene_first_actor_for_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? 0U
               : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
}

bool scene_attached_ball_position(
    const TecmoGameplaySceneActor *holder,
    TecmoGameplayCourtCoordinateQ8 *position_out)
{
    TecmoGameplayCourtCoordinate attached;
    TecmoGameplayCourtCoordinateQ8 attached_q8;
    if (holder == NULL || position_out == NULL) return false;
    attached.x = (int16_t)(
        holder->position.x + (holder->facing_right ? 7 : -7));
    attached.y = (int16_t)(holder->position.y - 17);
    if (!tecmo_gameplay_court_coordinate_to_q8(
            &attached, &attached_q8)) {
        return false;
    }
    *position_out = attached_q8;
    return true;
}

bool scene_attach_ball(TecmoGameplayScene *scene)
{
    TecmoGameplayCourtCoordinateQ8 attached;
    TecmoGameplayBallDribbleFrame dribble;
    if (scene == NULL ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        if (!scene_live_ball_frame_for_actors(
                scene, scene->actors, scene->ball_holder, &dribble) ||
            !tecmo_gameplay_court_coordinate_to_q8(
                &dribble.visible_position, &attached)) {
            return false;
        }
    } else if (!scene_attached_ball_position(
                   &scene->actors[scene->ball_holder], &attached)) {
        return false;
    }
    scene->ball_position = attached;
    return true;
}

bool scene_settle_boundary_latch(TecmoGameplayScene *scene,
                                        bool *settled_out)
{
    TecmoGameplayPenaltyPresentation presentation;
    TecmoGameplayViolation violation;
    TecmoGameplayState next_state;
    TecmoGameplayCourtCoordinateQ8 next_ball;
    TecmoGameplayBallDribbleFrame dribble;
    TecmoGameplayTeam restart;
    size_t actor;
    bool holder_latched = false;
    if (scene == NULL || settled_out == NULL ||
        !scene->penalty_assets.available ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    *settled_out = false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene->actors[actor].movement_boundary_latched) continue;
        if (actor != scene->ball_holder) return false;
        holder_latched = true;
    }
    if (!holder_latched) return true;
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->actors[scene->ball_holder].team != scene->state.possession ||
        !tecmo_gameplay_penalties_get_violation(
            &scene->penalty_assets, 1U, &violation, &presentation) ||
        violation != TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        presentation.kind !=
            TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION ||
        presentation.lead_in_frames !=
            TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES ||
        presentation.maximum_wait_frames !=
            TECMO_GAMEPLAY_VIOLATION_WAIT_FRAMES ||
        presentation.presentation_sfx_id !=
            TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID ||
        presentation.presentation_sfx_delay_frames != 16U ||
        presentation.release_button_mask != 0x80U ||
        presentation.controller_count != TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        presentation.live_restart_sfx_id !=
            TECMO_GAMEPLAY_RESTART_PLAY_ID ||
        presentation.live_restart_music_id !=
            TECMO_GAMEPLAY_RESTART_PLAY_ID ||
        !presentation.live_restart_requires_game_music ||
        !scene_live_ball_frame_for_actors(
            scene, scene->actors, scene->ball_holder, &dribble) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &dribble.visible_position, &next_ball)) {
        return false;
    }
    restart = scene_other_team(scene->state.possession);
    next_state = scene->state;
    if (!tecmo_gameplay_request_violation(
            &next_state, violation, restart)) {
        return false;
    }
    scene->state = next_state;
    scene->ball_position = next_ball;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        scene->actors[actor].movement_boundary_latched = false;
    }
    *settled_out = true;
    return true;
}

bool scene_settle_backcourt(TecmoGameplayScene *scene,
                                   bool *settled_out)
{
    TecmoGameplayBackcourtState next_backcourt;
    TecmoGameplayBackcourtStepInput input;
    TecmoGameplayPenaltyPresentation presentation;
    TecmoGameplayViolation violation;
    TecmoGameplayState next_state;
    TecmoGameplayTeam restart;
    bool detected = false;
    if (scene == NULL || settled_out == NULL ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->actors[scene->ball_holder].team != scene->state.possession ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state) ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        !tecmo_gameplay_court_coordinate_q8_floor(
            &scene->ball_position, &input.ball_position)) {
        return false;
    }
    *settled_out = false;
    input.orientation = scene->orientation_state.current_direction;
    input.global_object_state = 0U;
    next_backcourt = scene->backcourt_state;
    if (!tecmo_gameplay_backcourt_step(
            &scene->backcourt_assets, &next_backcourt, &input,
            &detected)) {
        return false;
    }
    if (!detected) {
        scene->backcourt_state = next_backcourt;
        return true;
    }
    if (!tecmo_gameplay_penalties_get_violation(
            &scene->penalty_assets,
            scene->backcourt_assets.violation_selector,
            &violation, &presentation) ||
        violation != TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        presentation.kind !=
            TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION ||
        presentation.lead_in_frames !=
            TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES ||
        presentation.maximum_wait_frames !=
            TECMO_GAMEPLAY_VIOLATION_WAIT_FRAMES ||
        presentation.presentation_sfx_id !=
            TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID ||
        presentation.presentation_sfx_delay_frames != 16U ||
        presentation.release_button_mask != 0x80U ||
        presentation.controller_count != TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        presentation.live_restart_sfx_id !=
            TECMO_GAMEPLAY_RESTART_PLAY_ID ||
        presentation.live_restart_music_id !=
            TECMO_GAMEPLAY_RESTART_PLAY_ID ||
        !presentation.live_restart_requires_game_music) {
        return false;
    }
    restart = scene_other_team(scene->state.possession);
    next_state = scene->state;
    if (!tecmo_gameplay_request_violation(
            &next_state, violation, restart)) {
        return false;
    }
    scene->state = next_state;
    scene->backcourt_state = next_backcourt;
    *settled_out = true;
    return true;
}

uint32_t scene_distance_squared(const TecmoGameplaySceneActor *a,
                                       const TecmoGameplaySceneActor *b)
{
    int32_t dx = (int32_t)a->position.x - b->position.x;
    int32_t dy = (int32_t)a->position.y - b->position.y;
    return (uint32_t)(dx * dx + dy * dy);
}

uint8_t scene_next_teammate(const TecmoGameplayScene *scene,
                                   uint8_t actor_index)
{
    uint8_t first;
    uint8_t local;
    if (actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    }
    first = scene_first_actor_for_team(
        (TecmoGameplayTeam)scene->actors[actor_index].team);
    local = (uint8_t)(actor_index - first);
    return (uint8_t)(first + (local + 1U) %
                     TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
}

uint8_t scene_nearest_actor_for_team(const TecmoGameplayScene *scene,
                                            TecmoGameplayTeam team,
                                            uint8_t target)
{
    uint8_t first = scene_first_actor_for_team(team);
    uint8_t best = first;
    uint32_t best_distance = UINT32_MAX;
    size_t offset;
    if (target >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return first;
    for (offset = 0U; offset < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
         ++offset) {
        uint8_t actor = (uint8_t)(first + offset);
        uint32_t distance = scene_distance_squared(&scene->actors[actor],
                                                   &scene->actors[target]);
        if (distance < best_distance) {
            best = actor;
            best_distance = distance;
        }
    }
    return best;
}

bool scene_pass_or_switch(TecmoGameplayScene *scene,
                                 size_t controller)
{
    TecmoGameplayTeam team;
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM) {
        return true;
    }
    team = (TecmoGameplayTeam)scene->launch.controller_team[controller];
    if (team == scene->state.possession &&
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        uint8_t next = scene_next_teammate(scene, scene->ball_holder);
        if (next < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            scene->ball_holder = next;
            scene->controlled_actor[controller] = next;
            return scene_attach_ball(scene);
        }
    } else {
        scene->controlled_actor[controller] =
            scene_nearest_actor_for_team(scene, team, scene->ball_holder);
    }
    return true;
}

size_t scene_controller_for_team(const TecmoGameplayScene *scene,
                                        TecmoGameplayTeam team)
{
    size_t controller;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == team) {
            return controller;
        }
    }
    return TECMO_GAMEPLAY_CONTROLLER_COUNT;
}

static bool scene_team_has_controller(const TecmoGameplayScene *scene,
                                      TecmoGameplayTeam team)
{
    return scene_controller_for_team(scene, team) <
           TECMO_GAMEPLAY_CONTROLLER_COUNT;
}

static bool scene_actor_is_controlled(const TecmoGameplayScene *scene,
                                      size_t actor)
{
    size_t controller;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] !=
                TECMO_GAMEPLAY_SCENE_NO_TEAM &&
            scene->controlled_actor[controller] == actor) {
            return true;
        }
    }
    return false;
}

/* Native, non-ROM-exact target policy for ordinary CPU locomotion. Offensive
   non-holders use mirrored per-slot formation points. Defenders retain their
   opposing roster-slot link for pose/facing, but stand goal-side of the linked
   offensive snapshot with a small slot-specific depth split so the fixed
   pairs cannot collapse onto one coordinate. */
static bool scene_cpu_policy_target(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCpuSteeringHarnessInput *snapshot,
    size_t actor,
    TecmoGameplayCourtCoordinate *target_out)
{
    static const TecmoGameplayCourtCoordinate formation_targets[5] = {
        {256, 148}, {288, 112}, {288, 184}, {352, 96}, {352, 200}
    };
    static const int8_t defender_depth_split[5] = {
        0, -10, 10, -14, 14
    };
    const TecmoGameplaySceneActor *item;
    uint8_t linked_actor;
    int32_t target_x;
    int32_t target_y;
    int32_t goal_side;
    if (scene == NULL || snapshot == NULL || target_out == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->state.possession > TECMO_GAMEPLAY_TEAM_HOME ||
        scene->orientation_state.current_direction >=
            TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT) {
        return false;
    }
    item = &scene->actors[actor];
    linked_actor = scene->cpu_actors[actor].linked_actor;
    if (item->team > TECMO_GAMEPLAY_TEAM_HOME ||
        item->roster_index >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        linked_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        linked_actor == actor ||
        !tecmo_gameplay_court_coordinate_valid(
            &snapshot->actor_position[linked_actor])) {
        return false;
    }

    if (item->team == (uint8_t)scene->state.possession) {
        const TecmoGameplayCourtCoordinate *formation =
            &formation_targets[item->roster_index];
        target_x = scene->orientation_state.current_direction == 0U
            ? formation->x
            : TECMO_GAMEPLAY_COURT_WORLD_MAX_X - formation->x;
        target_y = formation->y;
    } else {
        const TecmoGameplayCourtCoordinate *linked =
            &snapshot->actor_position[linked_actor];
        /* Orientation is the attacking side: orientation 0 attacks left,
           so its defender's goal-side offset decreases X; orientation 1
           attacks right, so its defender's goal-side offset increases X. */
        goal_side = scene->orientation_state.current_direction == 0U
            ? -1
            : 1;
        target_x = (int32_t)linked->x + goal_side * 32;
        target_y = (int32_t)linked->y +
            defender_depth_split[item->roster_index];
        /* Keep the bounded native policy from collapsing a boundary defender
           onto its fixed link. If the goal-side point is outside the shaped
           court, use the same 32-pixel distance on the court side. The final
           full-world bounds clamp below remains the safety check. */
        {
            bool goal_side_outside =
                target_x < TECMO_GAMEPLAY_COURT_WORLD_MIN_X ||
                target_x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
            int32_t boundary_y = target_y;
            if (boundary_y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y) {
                boundary_y = TECMO_GAMEPLAY_COURT_WORLD_MIN_Y;
            } else if (boundary_y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
                boundary_y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
            }
            if (!goal_side_outside) {
                int32_t half_y = boundary_y / 2;
                int32_t left_boundary =
                    TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE - half_y;
                int32_t right_boundary =
                    TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE + half_y;
                goal_side_outside = target_x < left_boundary ||
                    target_x > right_boundary;
            }
            if (goal_side_outside) {
                target_x = (int32_t)linked->x - goal_side * 32;
            }
        }
    }
    if (target_x < TECMO_GAMEPLAY_COURT_WORLD_MIN_X) {
        target_x = TECMO_GAMEPLAY_COURT_WORLD_MIN_X;
    } else if (target_x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X) {
        target_x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    }
    if (target_y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y) {
        target_y = TECMO_GAMEPLAY_COURT_WORLD_MIN_Y;
    } else if (target_y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
        target_y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
    }
    target_out->x = (int16_t)target_x;
    target_out->y = (int16_t)target_y;
    return tecmo_gameplay_court_coordinate_valid(target_out);
}

bool scene_cpu_actor_state_valid(
    const TecmoGameplayScene *scene,
    size_t actor,
    const TecmoGameplaySceneCpuActor *cpu)
{
    uint8_t actor_team;
    uint8_t linked_team;
    if (scene == NULL || cpu == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        cpu->contract_tag != TECMO_GAMEPLAY_SCENE_CPU_ACTOR_TAG ||
        cpu->command_offset !=
            TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET ||
        cpu->command_advance_pending ||
        cpu->linked_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    actor_team = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ? 0U : 1U;
    linked_team = cpu->linked_actor <
                          TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                      ? 0U : 1U;
    if (actor_team == linked_team) return false;
    if (!cpu->target_valid) {
        return cpu->decision_serial == 0U &&
               cpu->snapshot_fingerprint == 0U &&
               cpu->target_position.x == 0 &&
               cpu->target_position.y == 0 &&
               cpu->target_kind ==
                   TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT &&
               cpu->direction ==
                   TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION &&
               cpu->held_direction_bits ==
                   TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL &&
               !cpu->writes_direction;
    }
    if (cpu->decision_serial == 0U ||
        !tecmo_gameplay_court_coordinate_valid(&cpu->target_position) ||
        cpu->target_kind >=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT ||
        !tecmo_gameplay_movement_input_valid(
            cpu->held_direction_bits)) {
        return false;
    }
    if (!cpu->writes_direction) {
        return cpu->direction ==
                   TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION &&
               cpu->held_direction_bits ==
                   TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    }
    return cpu->direction < TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT &&
           scene->movement_assets.direction_map[
               cpu->held_direction_bits] == cpu->direction;
}

static bool scene_cpu_result_coherent(
    const TecmoGameplayScene *scene,
    size_t actor,
    const TecmoGameplayCpuSteeringMovementResult *result)
{
    const TecmoGameplaySceneCpuActor *cpu;
    if (scene == NULL || result == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        result->contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_RESULT_TAG ||
        result->steering.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_RESULT_TAG ||
        result->steering.actor != actor ||
        result->steering.possession != (uint8_t)scene->state.possession ||
        result->steering.orientation !=
            scene->orientation_state.current_direction ||
        result->steering.ball_holder != scene->ball_holder ||
        result->steering.difficulty != scene->launch.difficulty) {
        return false;
    }
    cpu = &scene->cpu_actors[actor];
    if (result->steering.matchup_actor != cpu->linked_actor) return false;
    if (actor == scene->ball_holder) {
        return result->steering.target_kind ==
                   TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
               result->steering.target_actor ==
                   TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    }
    return result->steering.target_kind ==
               TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET &&
           result->steering.target_actor == cpu->linked_actor;
}

bool scene_update_ai(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneCpuShotRequest *shot_request_out)
{
    TecmoGameplayCpuSteeringHarnessInput steering_snapshot;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        candidate_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplayBallDribbleFrame candidate_dribble;
    size_t actor;
    if (scene == NULL || shot_request_out == NULL ||
        !scene->cpu_steering_assets.available ||
        !scene->movement_assets.available ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->launch.difficulty >=
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT) {
        return false;
    }
    shot_request_out->requested = false;
    shot_request_out->actor_index = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    memset(&steering_snapshot, 0, sizeof(steering_snapshot));
    steering_snapshot.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    steering_snapshot.possession = (uint8_t)scene->state.possession;
    steering_snapshot.orientation =
        scene->orientation_state.current_direction;
    steering_snapshot.ball_holder = scene->ball_holder;
    steering_snapshot.difficulty = scene->launch.difficulty;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene->actors[actor].active ||
            !scene_actor_world_position_valid(&scene->actors[actor]) ||
            !scene_cpu_actor_state_valid(
                scene, actor, &scene->cpu_actors[actor])) {
            return false;
        }
        steering_snapshot.actor_position[actor] =
            scene->actors[actor].position;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    memcpy(candidate_cpu, scene->cpu_actors, sizeof(candidate_cpu));
    candidate_ball = scene->ball_position;

    /* All ten decisions consume one immutable post-human-input court
       snapshot. Successful TGAI -> TGMO steps are committed together, so
       iteration order cannot alter another CPU actor's target this frame. */
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplayCpuSteeringMovementInput input;
        TecmoGameplayCpuSteeringMovementResult result;
        TecmoGameplaySceneCpuActor *cpu;
        const TecmoTeamDataPlayer *player;
        if (scene_actor_is_controlled(scene, actor) ||
            actor == scene->shot_actor) {
            continue;
        }
        player = scene_actor_player(scene, &scene->actors[actor]);
        cpu = &candidate_cpu[actor];
        if (player == NULL || cpu->decision_serial == UINT32_MAX) {
            return false;
        }
        memset(&input, 0, sizeof(input));
        input.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
        input.steering = steering_snapshot;
        input.steering.actor = (uint8_t)actor;
        input.steering.matchup_actor = cpu->linked_actor;
        if (actor != scene->ball_holder &&
            !scene_cpu_policy_target(
                scene, &steering_snapshot, actor,
                &input.steering.explicit_target)) {
            return false;
        }
        input.steering.has_explicit_target = actor != scene->ball_holder;
        if (!scene_actor_movement_state(
                scene, &scene->actors[actor], &input.movement)) {
            return false;
        }
        input.player_movement_rating = player->profile[0];
        input.condition = scene->actors[actor].condition;
        input.speed_value = scene->launch.speed_value;
        input.global_object_state = 0U;
        input.movement_flags = 0U;
        input.primary_selected_actor = actor == scene->ball_holder;
        if (!tecmo_gameplay_cpu_steering_movement_step(
                &scene->cpu_steering_assets, &scene->movement_assets,
                &input, &result) ||
            !scene_cpu_result_coherent(scene, actor, &result) ||
            !scene_actor_apply_movement(
                scene, candidate_actors, actor, &result.movement,
                result.held_direction_bits) ||
            !scene_actor_world_position_valid(&candidate_actors[actor])) {
            return false;
        }
        ++cpu->decision_serial;
        cpu->snapshot_fingerprint = result.steering.input_fingerprint;
        cpu->target_position = result.steering.target_position;
        cpu->target_kind = (uint8_t)result.steering.target_kind;
        cpu->direction = result.steering.direction;
        cpu->held_direction_bits = result.held_direction_bits;
        cpu->target_valid = true;
        cpu->writes_direction = result.steering.writes_direction;
        if (!scene_cpu_actor_state_valid(scene, actor, cpu)) return false;
    }

    if (!scene_live_ball_frame_for_actors(
            scene, candidate_actors, scene->ball_holder,
            &candidate_dribble) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &candidate_dribble.visible_position, &candidate_ball)) {
        return false;
    }
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    memcpy(scene->cpu_actors, candidate_cpu, sizeof(candidate_cpu));
    scene->ball_position = candidate_ball;

    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene_team_has_controller(scene, scene->state.possession)) {
        TecmoGameplaySceneActor *holder = &scene->actors[scene->ball_holder];
        const TecmoGameplaySceneCpuActor *cpu =
            &scene->cpu_actors[scene->ball_holder];
        int32_t target_dx = (int32_t)holder->position.x -
                            cpu->target_position.x;
        int32_t target_dy = (int32_t)holder->position.y -
                            cpu->target_position.y;
        uint32_t shot_cadence = 60U -
            (uint32_t)scene->launch.difficulty * 15U;
        if (target_dx < 0) target_dx = -target_dx;
        if (target_dy < 0) target_dy = -target_dy;
        /* Shot choice/cadence is still native approximate policy, kept
           separate from the now TGAI-directed/TGMO-moved ordinary actor. */
        if (!holder->movement_boundary_latched && cpu->target_valid &&
            cpu->target_kind ==
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
            target_dx <= 2 && target_dy <= 2 &&
            scene->frame % shot_cadence == 0U) {
            /* CPU close shots remain available. Report the launch decision
               to the scene orchestrator; actor policy must not depend on the
               shot playback module. */
            shot_request_out->requested = true;
            shot_request_out->actor_index = scene->ball_holder;
        }
    }
    return true;
}

bool scene_tick_fatigue(TecmoGameplayScene *scene)
{
    TecmoGameplayFatigueState next;
    TecmoGameplayFatigueStepInput input;
    if (scene == NULL ||
        !tecmo_gameplay_fatigue_state_valid(
            &scene->fatigue_assets, &scene->fatigue_state) ||
        scene->launch.difficulty >=
            TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.difficulty = scene->launch.difficulty;
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        size_t first = team * TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        for (size_t slot = 0U;
             slot < TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT; ++slot) {
            const TecmoGameplaySceneActor *actor =
                &scene->actors[first + slot];
            if (!actor->active || actor->team != team ||
                actor->roster_index >=
                    TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT) {
                return false;
            }
            input.active_roster[team][slot] = actor->roster_index;
        }
    }
    next = scene->fatigue_state;
    if (!tecmo_gameplay_fatigue_step(
            &scene->fatigue_assets, &next, &input)) {
        return false;
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplaySceneActor *item = &scene->actors[actor];
        if (item->team >= TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT ||
            item->roster_index >= TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT) {
            return false;
        }
    }
    scene->fatigue_state = next;
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplaySceneActor *item = &scene->actors[actor];
        item->condition = next.condition[item->team][item->roster_index];
    }
    return true;
}
