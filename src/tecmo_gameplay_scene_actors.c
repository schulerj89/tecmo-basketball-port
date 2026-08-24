#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_gameplay_candidate_selection.h"
#include "tecmo_gameplay_cpu_opcode_workspaces.h"
#include "tecmo_gameplay_cpu_route_profile.h"
#include "tecmo_gameplay_defense_contact.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native actor locomotion, ball attachment, and CPU steering integration. */

static bool scene_actor_in_pretip_recovery(const TecmoGameplayScene *scene,
                                           size_t actor);

static bool scene_cpu_route_direction_bits(
    const TecmoGameplayScene *scene,
    uint8_t direction,
    uint8_t *held_direction_bits_out)
{
    uint8_t bits;
    if (scene == NULL || held_direction_bits_out == NULL ||
        direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
        return false;
    }
    for (bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
         bits <= (TECMO_GAMEPLAY_MOVEMENT_INPUT_UP |
                  TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN |
                  TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT |
                  TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT); ++bits) {
        if (tecmo_gameplay_movement_input_valid(bits) &&
            scene->movement_assets.direction_map[bits] == direction) {
            *held_direction_bits_out = bits;
            return true;
        }
    }
    return false;
}

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

bool scene_goal_facing_right_for_team(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam team,
    bool *facing_right_out)
{
    TecmoGameplayCourtCoordinate hoop;
    bool facing_right;
    if (scene == NULL || facing_right_out == NULL ||
        (team != TECMO_GAMEPLAY_TEAM_AWAY &&
         team != TECMO_GAMEPLAY_TEAM_HOME) ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &scene->court_orientation, &scene->orientation_state,
            (uint8_t)team, &hoop)) {
        return false;
    }
    if (hoop.x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X) {
        facing_right = false;
    } else if (hoop.x == TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X) {
        facing_right = true;
    } else {
        return false;
    }
    *facing_right_out = facing_right;
    return true;
}

bool scene_apply_goal_facing(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT])
{
    bool facing_right[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    size_t actor;
    if (scene == NULL || actors == NULL) return false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!actors[actor].active) continue;
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)actors[actor].team,
                &facing_right[actor])) {
            return false;
        }
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (actors[actor].active) {
            actors[actor].facing_right = facing_right[actor];
        }
    }
    return true;
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

bool scene_actor_position_valid_for_scene(
    const TecmoGameplayScene *scene, size_t actor)
{
    static const TecmoGameplayCourtCoordinate seed_position[2U] = {
        {0x027B, 0x0094}, {0x0085, 0x0094}
    };
    const TecmoGameplaySceneActor *item;
    uint8_t orientation;
    if (scene == NULL || actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    item = &scene->actors[actor];
    if (scene_actor_world_position_valid(item)) return true;
    orientation = scene->live_foundation.orientation;
    /* `$85EA` seeds the selected primary 16/17 pixels beyond the ordinary
       `$F106` trapezoid at depth `$94`, while `$0588&$08` makes selected
       movement bypass that clamp. Admit only the typed one-shot owner and
       its narrow source-to-boundary corridor. Human movement retires the tag
       transactionally after natural re-entry. The automatic opcode-5/pass
       staging remains stationary and retires it when the catch changes the
       primary during foundation sync. */
    return scene->live_foundation.regulation_entry_clamp_exemption_active &&
           orientation < 2U &&
           actor == scene->live_foundation
               .regulation_entry_clamp_exempt_actor &&
           item->position.y == seed_position[orientation].y &&
           item->anchor.x == seed_position[orientation].x &&
           item->anchor.y == seed_position[orientation].y &&
           (orientation == 0U
                ? item->position.x >= 0x026AU &&
                  item->position.x <= seed_position[0U].x
                : item->position.x >= seed_position[1U].x &&
                  item->position.x <= 0x0095);
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

const TecmoTeamDataPlayer *scene_actor_player(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor)
{
    uint8_t team_id;
    if (scene == NULL || actor == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        actor->team > TECMO_GAMEPLAY_TEAM_HOME ||
        actor->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return NULL;
    }
    team_id = actor->team == TECMO_GAMEPLAY_TEAM_AWAY
                  ? scene->launch.away_team : scene->launch.home_team;
    if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT) return NULL;
    return &scene->pretip_team_data->players[team_id]
                                                [actor->roster_index];
}

bool scene_sync_live_foundation(TecmoGameplayScene *scene)
{
    TecmoGameplaySceneActor candidate_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t actor_team[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation candidate;
    size_t actor;
    if (scene == NULL || scene->state.possession > TECMO_GAMEPLAY_TEAM_HOME) {
        return false;
    }
    /* shots.c deliberately clears the slot holder during playback and the
       retained loose-ball phase. Preserve the last validated LIVE binding
       until an existing typed handoff restores a holder; no dynamic matchup
       is fabricated while ownership is physically open. */
    if (scene->ball_holder == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
        return scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
               scene->loose_ball_state.active;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    if (scene->live_foundation.regulation_entry_clamp_exemption_active &&
        scene->live_foundation.regulation_entry_clamp_exempt_actor <
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene->ball_holder != scene->live_foundation
            .regulation_entry_clamp_exempt_actor) {
        uint8_t old_primary = scene->live_foundation
            .regulation_entry_clamp_exempt_actor;
        /* A primary change ends `$0588&$08`; the old primary immediately
           becomes a secondary actor and therefore takes the ordinary exact
           trapezoid clamp before the new LIVE binding is published. */
        scene_clamp_actor_world(&candidate_actors[old_primary]);
        candidate_actors[old_primary].anchor =
            candidate_actors[old_primary].position;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplayScene validation_scene = *scene;
        validation_scene.actors[actor] = candidate_actors[actor];
        if (!candidate_actors[actor].active ||
            !scene_actor_position_valid_for_scene(
                &validation_scene, actor)) {
            return false;
        }
        positions[actor] = candidate_actors[actor].position;
        actor_team[actor] = candidate_actors[actor].team;
    }
    candidate = scene->live_foundation;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, positions,
            scene->orientation_state.attack_direction,
            (uint8_t)scene->state.possession, scene->ball_holder,
            actor_team, scene->launch.controller_team,
            scene->controlled_actor, &candidate)) {
        return false;
    }
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    scene->live_foundation = candidate;
    return true;
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

bool scene_ball_position_for_actors(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    uint8_t holder_index,
    TecmoGameplayCourtCoordinateQ8 *position_out)
{
    TecmoGameplayCourtCoordinateQ8 attached;
    TecmoGameplayBallDribbleFrame dribble;
    if (scene == NULL || actors == NULL || position_out == NULL ||
        holder_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !actors[holder_index].active) {
        return false;
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        if (!scene_live_ball_frame_for_actors(
                scene, actors, holder_index, &dribble) ||
            !tecmo_gameplay_court_coordinate_to_q8(
                &dribble.visible_position, &attached)) {
            return false;
        }
    } else if (!scene_attached_ball_position(
                   &actors[holder_index], &attached)) {
        return false;
    }
    *position_out = attached;
    return true;
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
    /* TGMO supplies locomotion/pose state; only an actual horizontal held
       direction is an explicit movement-facing override. Neutral and
       vertical-only input preserve the goal baseline (or the last deliberate
       horizontal override). */
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
    if (scene_actor_in_pretip_recovery(scene, actor_index)) return true;
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
    input.movement_flags =
        scene->live_foundation.regulation_entry_clamp_exemption_active &&
        actor_index == scene->live_foundation
            .regulation_entry_clamp_exempt_actor
            ? 0x08U : 0U;
    input.primary_selected_actor = actor_index == scene->ball_holder;
    if (!tecmo_gameplay_movement_step(
            &scene->movement_assets, &movement, &input) ||
        !scene_actor_apply_movement(
            scene, scene->actors, actor_index, &movement,
            direction_bits)) {
        return false;
    }
    if (scene->live_foundation.regulation_entry_clamp_exemption_active &&
        actor_index == scene->live_foundation
            .regulation_entry_clamp_exempt_actor &&
        scene_actor_world_position_valid(&scene->actors[actor_index])) {
        scene->actors[actor_index].anchor =
            scene->actors[actor_index].position;
        scene->live_foundation.regulation_entry_clamp_exemption_active =
            false;
        scene->live_foundation.regulation_entry_clamp_exempt_actor =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    }
    return true;
}

bool scene_move_actor_toward_loose_ball(
    TecmoGameplayScene *scene, uint8_t actor_index)
{
    TecmoGameplaySceneActor candidate_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayMovementState movement;
    TecmoGameplayMovementStepInput input;
    const TecmoTeamDataPlayer *player;
    TecmoGameplayCourtCoordinate target;
    int16_t horizontal;
    int16_t depth;
    uint8_t direction;
    uint8_t direction_bits;
    if (scene == NULL || actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->loose_ball_state.active ||
        scene->loose_ball_state.chase_actor != actor_index ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        !scene->actors[actor_index].active ||
        !tecmo_gameplay_court_coordinate_q8_valid(&scene->ball_position) ||
        !scene_actor_movement_state(
            scene, &scene->actors[actor_index], &movement)) {
        return false;
    }
    if (scene->loose_ball_state.airborne_interaction) {
        /* `$A0DD` writes its constructed landing target to `$038D-$0390`
           before `$A023`; opcode 20 chases that immutable target, not each
           intermediate object-10 coordinate. */
        target.x = (int16_t)scene->loose_ball_state
            .launch_a0dd.target_x_95_94;
        target.y = (int16_t)scene->loose_ball_state
            .launch_a0dd.target_depth_97_96;
    } else {
        target.x = (int16_t)(scene->ball_position.x_q8 /
                             TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE);
        target.y = (int16_t)(scene->ball_position.y_q8 /
                             TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE);
    }
    horizontal = (int16_t)(target.x - scene->actors[actor_index].position.x);
    depth = (int16_t)(target.y - scene->actors[actor_index].position.y);
    if (horizontal == 0 && depth == 0) return true;
    if (!tecmo_gameplay_cpu_steering_direction_for_delta(
            &scene->cpu_steering_assets, horizontal, depth, &direction) ||
        !scene_cpu_route_direction_bits(
            scene, direction, &direction_bits) ||
        (player = scene_actor_player(
            scene, &scene->actors[actor_index])) == NULL) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.held_direction_bits = direction_bits;
    input.player_movement_rating = player->profile[0];
    input.condition = scene->actors[actor_index].condition;
    input.speed_value = scene->launch.speed_value;
    input.global_object_state = 0U;
    input.movement_flags = 0U;
    input.primary_selected_actor = false;
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    if (!tecmo_gameplay_movement_step(
            &scene->movement_assets, &movement, &input) ||
        !scene_actor_apply_movement(
            scene, candidate_actors, actor_index, &movement,
            direction_bits) ||
        !scene_actor_world_position_valid(&candidate_actors[actor_index])) {
        return false;
    }
    /* Grounded misses retain the older adapter-owned state-0 inputs. The
       interaction route's actor and target are selected by the exact A023
       transaction, while its remaining movement-state composition is shared
       with the existing TGMO executor. */
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
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
    if (scene == NULL ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    if (!scene_ball_position_for_actors(
            scene, scene->actors, scene->ball_holder,
            &scene->ball_position)) {
        return false;
    }
    return true;
}

#define TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES 2U
#define TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES 72U
#define TECMO_GAMEPLAY_PASS_PLANAR_SUBSTEPS 4U

bool scene_pass_active(const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_NONE;
}

bool scene_pass_bank05_bd6e_step(
    uint16_t *x_accumulator_io,
    uint16_t x_delta,
    uint16_t *y_accumulator_io,
    uint16_t y_delta,
    uint16_t *x_position_out,
    uint8_t *y_position_out)
{
    uint16_t x_accumulator;
    uint16_t y_accumulator;
    if (x_accumulator_io == NULL || y_accumulator_io == NULL ||
        x_position_out == NULL || y_position_out == NULL) {
        return false;
    }
    /* Bank05 $BD6E-$BDC6 adds each low byte then each high byte with carry,
       preserving ordinary 6502 uint16 wrap. Six LSR/ROR pairs expose Q10.6
       coordinates: $E8/$73 retain both X bytes, while $F3 retains only the
       low Y result byte. Use unsigned words so a high delta such as $FF is
       never subjected to implementation-defined signed right shift. */
    x_accumulator = (uint16_t)(*x_accumulator_io + x_delta);
    y_accumulator = (uint16_t)(*y_accumulator_io + y_delta);
    *x_accumulator_io = x_accumulator;
    *y_accumulator_io = y_accumulator;
    *x_position_out = (uint16_t)(x_accumulator >> 6U);
    *y_position_out = (uint8_t)(y_accumulator >> 6U);
    return true;
}

static int16_t scene_pass_signed_half_16(int16_t value)
{
    /* $AA84/$AA93 use CMP #$80 followed by ROR high/low, which is an
       arithmetic right shift and therefore rounds negative odd words down. */
    if (value >= 0) return (int16_t)(value / 2);
    return (int16_t)-(((-(int32_t)value) + 1) / 2);
}

bool scene_pass_bank05_b42f_duration(uint16_t source_x,
                                     uint8_t source_depth,
                                     uint16_t target_x,
                                     uint8_t target_depth,
                                     uint16_t *base_duration_out)
{
    uint16_t absolute_x;
    uint16_t absolute_depth;
    uint16_t larger;
    uint16_t smaller;
    uint16_t table_index;
    uint16_t table_value;
    if (base_duration_out == NULL) return false;
    absolute_x = source_x >= target_x
        ? (uint16_t)(source_x - target_x)
        : (uint16_t)(target_x - source_x);
    absolute_depth = source_depth >= target_depth
        ? (uint16_t)(source_depth - target_depth)
        : (uint16_t)(target_depth - source_depth);
    larger = absolute_x >= absolute_depth ? absolute_x : absolute_depth;
    smaller = absolute_x >= absolute_depth ? absolute_depth : absolute_x;
    /* $B432-$B468: halve the smaller magnitude, add, then halve the sum. */
    table_index = (uint16_t)(larger + (smaller >> 1U));
    table_index >>= 1U;
    /* Rev-1 Bank05 $BBA1-$BCA0 is exactly max(1,floor(index/7)) for all
       256 entries. The source's high-index pointer path is outside legal
       gameplay geometry and is rejected instead of reading invented bytes. */
    if (table_index > UINT8_MAX) return false;
    table_value = (uint16_t)(table_index / 7U);
    if (table_value == 0U) table_value = 1U;
    *base_duration_out = (uint16_t)(table_value << 1U);
    return *base_duration_out >= TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES &&
           *base_duration_out <= TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES;
}

static bool scene_pass_bank05_launch_trajectory(
    TecmoGameplayScenePassState *pass)
{
    int32_t delta_x;
    int32_t delta_depth;
    int16_t velocity_x;
    int16_t velocity_depth;
    uint16_t source_x;
    uint8_t source_depth;
    uint16_t target_x;
    uint8_t target_depth;
    uint16_t base_duration;
    if (pass == NULL || pass->start_position.x_q8 < 0 ||
        pass->start_position.y_q8 < 0 || pass->target_position.x_q8 < 0 ||
        pass->target_position.y_q8 < 0) return false;
    source_x = (uint16_t)(pass->start_position.x_q8 / 256);
    source_depth = (uint8_t)(pass->start_position.y_q8 / 256);
    target_x = (uint16_t)(pass->target_position.x_q8 / 256);
    target_depth = (uint8_t)(pass->target_position.y_q8 / 256);
    if (!scene_pass_bank05_b42f_duration(
            source_x, source_depth, target_x, target_depth,
            &base_duration)) return false;
    delta_x = (int32_t)target_x - (int32_t)source_x;
    delta_depth = (int32_t)target_depth - (int32_t)source_depth;
    /* $BCF4 shifts each signed delta six times and $80A9 divides toward
       zero by the pre-$B074 base duration. $9A69 then invokes $AA84 twice. */
    velocity_x = (int16_t)((delta_x * 64) / (int32_t)base_duration);
    velocity_depth =
        (int16_t)((delta_depth * 64) / (int32_t)base_duration);
    velocity_x = scene_pass_signed_half_16(
        scene_pass_signed_half_16(velocity_x));
    velocity_depth = scene_pass_signed_half_16(
        scene_pass_signed_half_16(velocity_depth));
    pass->source_x_q6 = (uint16_t)(source_x << 6U);
    pass->source_depth_q6 = (uint16_t)((uint16_t)source_depth << 6U);
    pass->source_velocity_x_q6 = velocity_x;
    pass->source_velocity_depth_q6 = velocity_depth;
    pass->source_duration_remaining =
        (uint16_t)(base_duration * TECMO_GAMEPLAY_PASS_PLANAR_SUBSTEPS);
    pass->source_height_q8 = 0U;
    /* $B48D-$B4AD invokes $8006 with 8 and the base duration. Its low
       product word seeds $049A/$04A5. */
    pass->source_velocity_height_q8 =
        (int16_t)(uint16_t)(base_duration * 8U);
    pass->flight_frame = 0U;
    pass->flight_duration = base_duration;
    return true;
}

static void scene_pass_bank05_height_step(uint16_t *height_io,
                                          int16_t *velocity_io,
                                          uint8_t gravity)
{
    uint16_t velocity;
    uint16_t height;
    uint8_t height_high;
    if (height_io == NULL || velocity_io == NULL) return;
    velocity = (uint16_t)*velocity_io;
    velocity = (uint16_t)(velocity - gravity);
    height = (uint16_t)(*height_io + velocity);
    height_high = (uint8_t)(height >> 8U);
    /* $B69C/$B6D0 treats high byte 0 and the signed-underflow range F6-FF
       as ground, clearing both workspaces atomically. */
    if (height_high == 0U || height_high >= 0xF6U) {
        height = 0U;
        velocity = 0U;
    }
    *height_io = height;
    *velocity_io = (int16_t)velocity;
}

static uint16_t scene_pass_bank05_b13f_metric(uint16_t defender_x,
                                               uint8_t defender_depth,
                                               uint16_t ball_x,
                                               uint8_t ball_depth)
{
    uint16_t absolute_x = defender_x >= ball_x
        ? (uint16_t)(defender_x - ball_x)
        : (uint16_t)(ball_x - defender_x);
    uint16_t absolute_depth = defender_depth >= ball_depth
        ? (uint16_t)(defender_depth - ball_depth)
        : (uint16_t)(ball_depth - defender_depth);
    uint16_t larger = absolute_x >= absolute_depth
        ? absolute_x : absolute_depth;
    uint16_t smaller = absolute_x >= absolute_depth
        ? absolute_depth : absolute_x;
    /* $9E0A supplies absolute planar deltas and $A184 combines them as
       max + floor(min/2), preserving the 16-bit result. */
    return (uint16_t)(larger + (smaller >> 1U));
}

static bool scene_pass_bank05_rate_bias(const TecmoGameplayScene *scene,
                                        uint8_t *rate_out,
                                        uint8_t *bias_out)
{
    if (scene == NULL || rate_out == NULL || bias_out == NULL) return false;
    if (scene->launch.source == TECMO_GAMEPLAY_SCENE_PRESEASON) {
        if (scene->launch.difficulty >= 3U) return false;
        *rate_out = scene->launch.difficulty;
        *bias_out = 0U;
        return true;
    }
    if (scene->launch.source == TECMO_GAMEPLAY_SCENE_SEASON) {
        *rate_out = 2U;
        *bias_out = (uint8_t)(scene->launch.game_index >> 5U);
        return true;
    }
    return false;
}

static bool scene_pass_bank05_try_interception(
    TecmoGameplayScene *scene,
    bool *intercepted_out)
{
    static const uint8_t threshold_table_b1b9[24U] = {
        0x28U, 0x20U, 0x10U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x38U, 0x24U, 0x18U, 0x04U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x40U, 0x38U, 0x30U, 0x28U, 0x20U, 0x00U, 0x00U, 0x00U
    };
    TecmoGameplayScene candidate;
    const TecmoTeamDataPlayer *defender_player;
    uint8_t defender;
    uint8_t rate;
    uint8_t bias;
    uint8_t metric;
    uint16_t metric_word;
    uint8_t threshold;
    uint8_t next_random;
    int32_t defender_x;
    int32_t defender_depth;
    int32_t ball_x;
    int32_t ball_depth;
    if (scene == NULL || intercepted_out == NULL ||
        !scene_pass_active(scene) ||
        scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_FLIGHT ||
        scene->legacy_direct_launch) return false;
    *intercepted_out = false;
    /* Ordinary live pass ownership proves source $05A1==0. A nonzero $07E9
       seed is retained explicitly from B074 and returns before proximity. */
    if (scene->pass_state.interception_inhibited) return true;
    defender = scene->live_foundation.defender_actor;
    if (defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        defender == scene->pass_state.passer ||
        defender == scene->pass_state.receiver ||
        scene->actors[defender].team == (uint8_t)scene->state.possession ||
        (defender_player = scene_actor_player(
            scene, &scene->actors[defender])) == NULL ||
        !scene_pass_bank05_rate_bias(scene, &rate, &bias) ||
        !tecmo_gameplay_fixed_rng_valid(&scene->fixed_rng)) {
        return false;
    }
    defender_x = scene->actors[defender].position.x;
    defender_depth = scene->actors[defender].position.y;
    ball_x = scene->ball_position.x_q8 / 256;
    ball_depth = scene->ball_position.y_q8 / 256;
    if (defender_x < 0 || defender_x > UINT16_MAX ||
        defender_depth < 0 || defender_depth > UINT8_MAX ||
        ball_x < 0 || ball_x > UINT16_MAX ||
        ball_depth < 0 || ball_depth > UINT8_MAX) return false;
    metric_word = scene_pass_bank05_b13f_metric(
        (uint16_t)defender_x, (uint8_t)defender_depth,
        (uint16_t)ball_x, (uint8_t)ball_depth);
    if (metric_word >= 8U) return true;
    metric = (uint8_t)metric_word;
    threshold = (uint8_t)(
        (uint8_t)(bias << 1U) +
        threshold_table_b1b9[(size_t)rate * 8U + metric]);
    if (threshold != 0U &&
        scene->live_foundation.control_mode[
            scene->live_foundation.defense_side] != 0U) {
        if (threshold < 0x18U) return true;
        threshold = (uint8_t)(threshold - 0x18U);
    }
    if (threshold < scene->fixed_rng.raw_006a) return true;
    candidate = *scene;
    if (!tecmo_gameplay_fixed_rng_c05d(
            &candidate.fixed_rng, TECMO_GAMEPLAY_FIXED_RNG_CALL_B13F,
            &next_random)) return false;
    /* Bank02 $A8CC-$A8D0 copies profile byte 4 into actor-local $0533. */
    if (defender_player->profile[4U] < next_random) {
        scene->fixed_rng = candidate.fixed_rng;
        return true;
    }
    scene_pass_clear(&candidate);
    if (!scene_handoff_claimant_settlement(
            &candidate, (TecmoGameplayTeam)candidate.actors[defender].team,
            defender) || !scene_pass_state_valid(&candidate)) {
        return false;
    }
    *scene = candidate;
    *intercepted_out = true;
    return true;
}

void scene_pass_clear(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    memset(&scene->pass_state, 0, sizeof(scene->pass_state));
    scene->pass_state.passer = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->pass_state.receiver = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->pass_state.controller = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
}

bool scene_pass_state_valid(const TecmoGameplayScene *scene)
{
    const TecmoGameplayScenePassState *pass;
    bool cpu_transport;
    if (scene == NULL) return false;
    pass = &scene->pass_state;
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_NONE) {
        return pass->passer == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               pass->receiver == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               pass->controller == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               pass->packed_animation_state == 0U &&
               !pass->receiver_locked && !pass->interception_inhibited &&
               pass->reserved[0U] == 0U && pass->reserved[1U] == 0U &&
               pass->flight_frame == 0U && pass->flight_duration == 0U &&
               pass->start_position.x_q8 == 0 &&
               pass->start_position.y_q8 == 0 &&
               pass->target_position.x_q8 == 0 &&
               pass->target_position.y_q8 == 0 &&
               pass->source_x_q6 == 0U && pass->source_depth_q6 == 0U &&
               pass->source_velocity_x_q6 == 0 &&
               pass->source_velocity_depth_q6 == 0 &&
               pass->source_duration_remaining == 0U &&
               pass->source_height_q8 == 0U &&
               pass->source_velocity_height_q8 == 0;
    }
    if (pass->phase >= TECMO_GAMEPLAY_SCENE_PASS_PHASE_COUNT ||
        pass->passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        pass->receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        pass->passer == pass->receiver ||
        (pass->controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT &&
         pass->controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR) ||
        scene->actors[pass->passer].team != scene->state.possession ||
        scene->actors[pass->receiver].team != scene->state.possession ||
        (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_STATE18
             ? scene->ball_holder != pass->receiver
             : scene->ball_holder != pass->passer) ||
        pass->flight_frame > pass->flight_duration) {
        return false;
    }
    cpu_transport = pass->controller == TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (cpu_transport) {
        for (size_t controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (scene->launch.controller_team[controller] ==
                    scene->state.possession ||
                scene->controlled_actor[controller] == pass->passer) {
                return false;
            }
        }
    } else if (scene->controlled_actor[pass->controller] !=
                   (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_STATE18
                        ? pass->receiver : pass->passer) ||
               scene->launch.controller_team[pass->controller] !=
                   scene->state.possession) {
        return false;
    }
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_GATHER) {
        return !pass->receiver_locked && !pass->interception_inhibited &&
               pass->flight_frame == 0U && pass->flight_duration == 0U &&
               pass->source_x_q6 == 0U && pass->source_depth_q6 == 0U &&
               pass->source_velocity_x_q6 == 0 &&
               pass->source_velocity_depth_q6 == 0 &&
               pass->source_duration_remaining == 0U &&
               pass->source_height_q8 == 0U &&
               pass->source_velocity_height_q8 == 0 &&
               (pass->packed_animation_state == 0x32U ||
                pass->packed_animation_state == 0x22U ||
                pass->packed_animation_state == 0x12U ||
                pass->packed_animation_state == 0x02U ||
                pass->packed_animation_state == 0x03U);
    }
    if ((pass->phase != TECMO_GAMEPLAY_SCENE_PASS_FLIGHT &&
         pass->phase != TECMO_GAMEPLAY_SCENE_PASS_STATE18) ||
        pass->packed_animation_state != 0x04U || !pass->receiver_locked ||
        pass->flight_duration < TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES ||
        pass->flight_duration > TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES ||
        pass->source_duration_remaining !=
            (uint16_t)((pass->flight_duration - pass->flight_frame) *
                       TECMO_GAMEPLAY_PASS_PLANAR_SUBSTEPS) ||
        pass->reserved[1U] !=
            (uint8_t)(pass->source_height_q8 >> 8U)) {
        return false;
    }
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_STATE18) {
        return pass->reserved[0U] <= 1U &&
            pass->flight_frame == pass->flight_duration &&
            pass->target_position.x_q8 % 256 == 0 &&
            pass->target_position.y_q8 % 256 == 0 &&
            pass->target_position.x_q8 >= 0 &&
            pass->target_position.x_q8 / 256 <= INT16_MAX &&
            pass->target_position.y_q8 >= 0 &&
            pass->target_position.y_q8 / 256 <= UINT8_MAX &&
            (scene->legacy_direct_launch ||
             scene->live_foundation.primary_actor == pass->receiver);
    }
    if (!scene->legacy_direct_launch &&
        (scene->live_foundation.primary_actor != pass->passer ||
         scene->live_foundation.selected_actor_by_side[
             scene->live_foundation.offense_side] != pass->receiver ||
         scene->live_foundation.candidate_actor_by_side[
             scene->live_foundation.offense_side] != pass->passer)) {
        return false;
    }
    return true;
}

bool scene_begin_actor_pass(TecmoGameplayScene *scene, uint8_t passer,
                            uint8_t receiver, uint8_t controller_or_none)
{
    TecmoGameplayScene candidate;
    TecmoGameplayScenePassState pass;
    TecmoGameplayCourtCoordinateQ8 start;
    TecmoGameplayCourtCoordinateQ8 target;
    if (scene == NULL || scene_pass_active(scene) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        passer != scene->ball_holder ||
        receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        receiver == passer ||
        scene->actors[receiver].team != scene->state.possession ||
        !scene_attached_ball_position(&scene->actors[passer], &start) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &scene->actors[receiver].position, &target)) {
        return false;
    }
    if (controller_or_none == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
        for (size_t controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (scene->launch.controller_team[controller] ==
                    scene->state.possession ||
                scene->controlled_actor[controller] == passer) {
                return false;
            }
        }
    } else if (controller_or_none >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
               scene->launch.controller_team[controller_or_none] !=
                   scene->state.possession ||
               scene->controlled_actor[controller_or_none] != passer) {
        return false;
    }
    memset(&pass, 0, sizeof(pass));
    pass.phase = TECMO_GAMEPLAY_SCENE_PASS_GATHER;
    pass.passer = passer;
    pass.receiver = receiver;
    pass.controller = controller_or_none;
    /* Bank05 $89D7 begins the shared passer gather: it writes state $0F and
       packed $0458=$32. State $0F dispatches through $8695 and $8999/$9C29;
       CPU admission to $89D7 is documented at the exact action-$21 helper
       below without imposing that upstream route on human NES-A passing. */
    pass.packed_animation_state = 0x32U;
    pass.start_position = start;
    pass.target_position = target;
    candidate = *scene;
    candidate.pass_state = pass;
    candidate.ball_position = start;
    if (!candidate.legacy_direct_launch) {
        candidate.live_foundation.play_state
            .action_state_046e[passer] = 0x0FU;
        candidate.live_foundation.play_state
            .actor_state[receiver] = 0x0CU;
        /* The remaining $89D7 writes seed slot-10 $0478=$13 plus receiver
           pose/direction workspaces. Pass phase GATHER owns the former
           semantically; the raw pose-pointer workspaces are not retained and
           must not be fabricated in C. */
        if (!tecmo_gameplay_live_foundation_valid(
                &candidate.cpu_steering_assets,
                &candidate.live_foundation)) {
            return false;
        }
    }
    if (!scene_pass_state_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

bool scene_begin_pass(TecmoGameplayScene *scene, size_t controller,
                      uint8_t receiver)
{
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT) return false;
    return scene_begin_actor_pass(
        scene, scene != NULL ? scene->ball_holder
                             : TECMO_GAMEPLAY_SCENE_NO_ACTOR,
        receiver, (uint8_t)controller);
}

bool scene_begin_cpu_pass_from_action21(TecmoGameplayScene *scene,
                                        uint8_t passer)
{
    uint8_t receiver;
    if (scene == NULL || scene->legacy_direct_launch ||
        passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.play_state.action_state_046e[passer] !=
            0x21U ||
        passer != scene->ball_holder ||
        scene->live_foundation.primary_actor != passer) {
        return false;
    }
    receiver = scene->live_foundation.candidate_actor_by_side[
        scene->live_foundation.offense_side];
    /* Bank06 $8374-$83F3 dispatches the typed automatic selected primary
       before the ordinary actor loop. State 4 reaches $8491->$8B90; opcode 9
       at $8FC5-$8FE7 copies C9=$21 to $046E[X] at $8FCA/$8FCC. Bank05 then
       consumes selected-primary index $21 at $89D7. This seam never forces a
       Bank04 cursor or routes CPU intent through human NES A. */
    return scene_begin_actor_pass(
        scene, passer, receiver, TECMO_GAMEPLAY_SCENE_NO_ACTOR);
}

static bool scene_begin_cpu_pass_from_action10(TecmoGameplayScene *scene,
                                               uint8_t passer)
{
    TecmoGameplayCourtCoordinateQ8 attached;
    TecmoGameplayCourtCoordinateQ8 prior_ball_position;
    TecmoGameplayLiveFoundation prior_foundation;
    uint8_t prior_ball_holder;
    uint8_t receiver;
    if (scene == NULL || scene->legacy_direct_launch ||
        passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.play_state.actor_state[passer] != 0x04U ||
        scene->live_foundation.play_state.action_state_046e[passer] !=
            0x10U ||
        scene->live_foundation.primary_actor != passer) {
        return false;
    }
    for (size_t controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] ==
                scene->state.possession ||
            scene->controlled_actor[controller] == passer) {
            return false;
        }
    }
    prior_ball_holder = scene->ball_holder;
    prior_ball_position = scene->ball_position;
    prior_foundation = scene->live_foundation;
    if (scene->live_foundation.score_restart_selection_active) {
        if (scene->ball_holder !=
                scene->live_foundation.score_restart_passer ||
            scene->actors[passer].team != (uint8_t)scene->state.possession ||
            !scene_attached_ball_position(
                &scene->actors[passer], &attached)) {
            return false;
        }
        /* The score-restart presentation keeps the ball on the inbound
           passer while the off-ball `$0168` play develops. At `$C711->$89DB`
           gather, source selection becomes the physical passer. */
        scene->ball_holder = passer;
        scene->ball_position = attached;
        receiver = scene->live_foundation.candidate_actor_by_side[
            scene->live_foundation.offense_side];
        if (!tecmo_gameplay_live_foundation_score_restart_gather(
                &scene->cpu_steering_assets,
                TECMO_GAMEPLAY_LIVE_SCORE_RESTART_GATHER_AUTOMATIC_SELECTED,
                passer, receiver,
                &scene->live_foundation)) {
            scene->ball_holder = prior_ball_holder;
            scene->ball_position = prior_ball_position;
            scene->live_foundation = prior_foundation;
            return false;
        }
    } else if (passer != scene->ball_holder) {
        return false;
    }
    receiver = scene->live_foundation.candidate_actor_by_side[
        scene->live_foundation.offense_side];
    /* Fixed `$C711` selector 1 reaches Bank05 `$89DB`, which enters the
       existing `$89D7` gather owner. This consumes the action-$10 pending
       marker on the update after opcode 6; it does not skip the preceding
       canonical opcode-23 record or claim natural stream reachability. */
    if (!scene_begin_actor_pass(
            scene, passer, receiver, TECMO_GAMEPLAY_SCENE_NO_ACTOR)) {
        scene->ball_holder = prior_ball_holder;
        scene->ball_position = prior_ball_position;
        scene->live_foundation = prior_foundation;
        return false;
    }
    return true;
}

static bool scene_selected_primary_automatic_dispatch_owned(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveFoundation *foundation,
    uint8_t actor)
{
    size_t controller;
    if (scene == NULL || foundation == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] ==
            scene->state.possession) return false;
    }
    return actor == scene->ball_holder ||
        (foundation->score_restart_selection_active &&
         scene->ball_holder == foundation->score_restart_passer &&
         actor == foundation->primary_actor);
}

bool scene_update_pass(TecmoGameplayScene *scene)
{
    TecmoGameplayScene candidate;
    TecmoGameplayScenePassState *pass;
    bool receiver_facing_right;
    size_t substep;
    if (scene == NULL || !scene_pass_active(scene) ||
        !scene_pass_state_valid(scene)) return false;
    candidate = *scene;
    pass = &candidate.pass_state;
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_STATE18) {
        TecmoGameplayObject10DispatchResult dispatch;
        TecmoGameplayActorCommandAssignmentInput input;
        TecmoGameplayActorCommandAssignmentResult assignment;
        TecmoGameplayActorCommandAssignmentSameFrameLatch latch;
        TecmoGameplaySceneA023LatchFrameContext context;
        TecmoGameplayLiveFoundation foundation;
        /* $B7B6 branches to $B7F7 while the live $0499 high byte is
           nonzero. That path retains state $18, performs the now-exhausted
           $B500 planar step, then applies the steeper $B678 gravity. Even a
           landing update returns; $B783 assignment begins next update. */
        if (pass->reserved[1U] != 0U) {
            scene_pass_bank05_height_step(
                &pass->source_height_q8,
                &pass->source_velocity_height_q8, 0x28U);
            pass->reserved[1U] =
                (uint8_t)(pass->source_height_q8 >> 8U);
            if (!scene_pass_state_valid(&candidate)) return false;
            *scene = candidate;
            return true;
        }
        memset(&dispatch, 0, sizeof(dispatch));
        memset(&input, 0, sizeof(input));
        memset(&assignment, 0, sizeof(assignment));
        memset(&latch, 0, sizeof(latch));
        memset(&context, 0, sizeof(context));
        if (candidate.legacy_direct_launch ||
            candidate.a023_latch_frame_context.available ||
            candidate.actor_command_assignment_assets == NULL ||
            !candidate.actor_command_assignment_assets->available ||
            !tecmo_gameplay_object10_dispatch_resolve(
                candidate.actor_command_assignment_assets, 0x18U,
                &dispatch) ||
            dispatch.contract_tag !=
                TECMO_GAMEPLAY_OBJECT10_DISPATCH_RESULT_TAG ||
            dispatch.handler_cpu != 0xB7B6U) {
            return false;
        }
        input.contract_tag =
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_INPUT_TAG;
        input.caller =
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE18_B7B6;
        input.raw_object_state = 0x18U;
        input.raw_0499 = pass->reserved[1U];
        input.object10_target_valid = true;
        input.object10_target.x =
            (int16_t)(candidate.ball_position.x_q8 / 256);
        input.object10_target.y =
            (int16_t)(candidate.ball_position.y_q8 / 256);
        input.object10_raw_target_valid = true;
        input.object10_raw_target.x =
            (uint16_t)(candidate.ball_position.x_q8 / 256);
        input.object10_raw_target.depth =
            (uint16_t)(candidate.ball_position.y_q8 / 256);
        foundation = candidate.live_foundation;
        if (!tecmo_gameplay_actor_command_assignment_apply_and_capture_same_frame_latch(
                candidate.actor_command_assignment_assets,
                &candidate.cpu_steering_assets, &input, &foundation,
                &assignment, &latch) || !assignment.applied ||
            assignment.caller != input.caller ||
            latch.producer_kind !=
                TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B783 ||
            !latch.b783_bit20_clear_follows_assignment) {
            return false;
        }
        context.contract_tag =
            TECMO_GAMEPLAY_SCENE_A023_LATCH_FRAME_CONTEXT_TAG;
        context.latch = latch;
        context.available = true;
        candidate.live_foundation = foundation;
        candidate.a023_latch_frame_context = context;
        scene_pass_clear(&candidate);
        if (!scene_attach_ball(&candidate) ||
            !scene_pass_state_valid(&candidate)) return false;
        *scene = candidate;
        return true;
    }
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_GATHER) {
        bool release_now = false;
        if (pass->packed_animation_state == 0x32U)
            pass->packed_animation_state = 0x22U;
        else if (pass->packed_animation_state == 0x22U)
            pass->packed_animation_state = 0x12U;
        else if (pass->packed_animation_state == 0x12U)
            pass->packed_animation_state = 0x02U;
        else if (pass->packed_animation_state == 0x02U)
            pass->packed_animation_state = 0x03U;
        else if (pass->packed_animation_state == 0x03U) {
            pass->packed_animation_state = 0x04U;
            release_now = true;
        } else {
            return false;
        }
        /* Bank05 $8999 jumps to $9C29 while the packed byte is >=$10,
           subtracting $10 without changing its low nibble. Below $10 it
           advances that nibble under raw $0385/$0391. The captured pass is
           $32->$22->$12->$02->$03->$04; those raw animation-global owners
           are not retained, so this is deliberately capture-bounded rather
           than a claim that every possible $8999 cadence is implemented. */
        if (release_now) {
            /* Bank05 $86A8-$86B7 jumps directly to shared $B074; this route
               arrives with slot-10 $0478=$13, not a required state $03.
               $B074-$B0FD locks $037F[$030A] as receiver and swaps its
               $000E-shaped role now while the $0308-shaped holder stays the
               passer until Bank05 $B24F. */
            if (!candidate.legacy_direct_launch &&
                !tecmo_gameplay_live_foundation_pass_launch_lock(
                    &candidate.cpu_steering_assets, pass->receiver,
                    &candidate.live_foundation)) {
                return false;
            }
            /* $B074 samples both slot-10's current attached coordinate and
               the receiver's current raw actor coordinate at release—not
               their positions from the start of the gather animation. */
            if (!scene_attached_ball_position(
                    &candidate.actors[pass->passer],
                    &pass->start_position) ||
                !tecmo_gameplay_court_coordinate_to_q8(
                    &candidate.actors[pass->receiver].position,
                    &pass->target_position) ||
                !scene_pass_bank05_launch_trajectory(pass)) {
                return false;
            }
            /* $B0B4-$B0E5 seeds nonzero $07E9 only for long passes moving
               in the orientation-selected horizontal direction while the
               live random byte is below $14. The later direction-state read
               changes 1 to 2 but cannot change B13F's zero/nonzero gate. */
            pass->interception_inhibited =
                (((uint16_t)pass->source_velocity_x_q6 >> 8U) & 0x80U) ==
                    (candidate.orientation_state.attack_direction == 0U
                         ? 0x80U : 0x00U) &&
                candidate.fixed_rng.raw_006a < 0x14U &&
                pass->source_duration_remaining >= 0x0078U;
            pass->receiver_locked = true;
            pass->phase = TECMO_GAMEPLAY_SCENE_PASS_FLIGHT;
        }
        if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_GATHER) {
            if (!scene_attached_ball_position(
                    &candidate.actors[pass->passer],
                    &candidate.ball_position) ||
                !scene_pass_state_valid(&candidate)) return false;
            *scene = candidate;
            return true;
        }
    }
    /* $B1E7 executes exactly four B500->BD6E substeps and tests B13F after
       each one. B13F interception is wired separately; retaining each exact
       intermediate coordinate here makes that later owner insertion local. */
    for (substep = 0U; substep < TECMO_GAMEPLAY_PASS_PLANAR_SUBSTEPS;
         ++substep) {
        bool intercepted;
        uint16_t raw_x;
        uint8_t raw_depth;
        if (pass->source_duration_remaining == 0U ||
            !scene_pass_bank05_bd6e_step(
                &pass->source_x_q6,
                (uint16_t)pass->source_velocity_x_q6,
                &pass->source_depth_q6,
                (uint16_t)pass->source_velocity_depth_q6,
                &raw_x, &raw_depth)) return false;
        --pass->source_duration_remaining;
        candidate.ball_position.x_q8 = (int32_t)raw_x * 256;
        candidate.ball_position.y_q8 = (int32_t)raw_depth * 256;
        if (!scene_pass_bank05_try_interception(
                &candidate, &intercepted)) return false;
        if (intercepted) {
            if (scene_pass_active(&candidate) ||
                candidate.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                !scene_pass_state_valid(&candidate)) return false;
            *scene = candidate;
            return true;
        }
    }
    ++pass->flight_frame;
    if (pass->source_duration_remaining != 0U) {
        /* Nonterminal state-$04 flight tails through $B2F2->$B6B1. */
        scene_pass_bank05_height_step(
            &pass->source_height_q8,
            &pass->source_velocity_height_q8, 0x12U);
        pass->reserved[1U] =
            (uint8_t)(pass->source_height_q8 >> 8U);
        if (!scene_pass_state_valid(&candidate)) return false;
        *scene = candidate;
        return true;
    }
    /* Genuine Bank05 $B24F begins AC 0A 03 and is the sole ownership
       transfer (Bank06 $B24F is unrelated geometry). The actor-2 -> actor-4
       capture reads $000E[0]=$04, stores actor 4 to $0308, clears its action/
       animation/actor-state workspaces, and only then calls $B2FA. $B2FA-
       $B300 clears raw $BA bit 2; no broader meaning is inferred here. Reuse
       the typed transaction only after reaching the locked receiver. */
    if (!candidate.legacy_direct_launch &&
        !tecmo_gameplay_live_foundation_pass_handoff(
            &candidate.cpu_steering_assets, pass->receiver,
            &candidate.live_foundation)) return false;
    candidate.ball_holder = pass->receiver;
    if (pass->controller < TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        candidate.controlled_actor[pass->controller] = pass->receiver;
    }
    /* $B24F->$88B6 installs a direction-selected catch pose. The scene does
       not retain that raw pose-pointer workspace, so use the validated TGOR
       attack-facing baseline rather than copying the passer's orientation. */
    if (!scene_goal_facing_right_for_team(
            &candidate, candidate.state.possession,
            &receiver_facing_right)) return false;
    candidate.actors[pass->receiver].facing_right = receiver_facing_right;
    /* `$B23B` compares literal 1 with the live `$6A`. Values 0/1 call the
       same `$B24F` handoff above and then fall through `$B244`, storing
       object state `$18`; all other values finish the catch immediately.
       State $18 retains the exact live `$0484/$048F` height and continues
       with `$B678` until it lands; `$B783` then runs on the following update. */
    if (!candidate.legacy_direct_launch &&
        candidate.fixed_rng.raw_006a <= 1U) {
        int32_t raw_x = pass->target_position.x_q8 / 256;
        int32_t raw_depth = pass->target_position.y_q8 / 256;
        if (raw_x < 0 || raw_x > INT16_MAX || raw_depth < 0 ||
            raw_depth > UINT8_MAX) return false;
        pass->reserved[0U] = candidate.fixed_rng.raw_006a;
        pass->reserved[1U] =
            (uint8_t)(pass->source_height_q8 >> 8U);
        pass->target_position = candidate.ball_position;
        pass->phase = TECMO_GAMEPLAY_SCENE_PASS_STATE18;
        if (!scene_pass_state_valid(&candidate)) return false;
        *scene = candidate;
        return true;
    }
    scene_pass_clear(&candidate);
    if (!scene_attach_ball(&candidate) ||
        !scene_pass_state_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

bool scene_inbound_active(const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->inbound_state.phase != TECMO_GAMEPLAY_SCENE_INBOUND_NONE;
}

void scene_inbound_clear(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    memset(&scene->inbound_state, 0, sizeof(scene->inbound_state));
    scene->inbound_state.passer = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->inbound_state.receiver = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->inbound_state.defender = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->inbound_state.restart_team = TECMO_GAMEPLAY_SCENE_NO_TEAM;
}

bool scene_inbound_state_valid(const TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneInboundState *inbound;
    if (scene == NULL) return false;
    inbound = &scene->inbound_state;
    if (inbound->phase == TECMO_GAMEPLAY_SCENE_INBOUND_NONE) {
        return inbound->passer == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               inbound->receiver == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               inbound->defender == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               inbound->restart_team == TECMO_GAMEPLAY_SCENE_NO_TEAM &&
               inbound->packed_animation_state == 0U &&
               inbound->reserved[0U] == 0U && inbound->reserved[1U] == 0U &&
               inbound->flight_frame == 0U && inbound->flight_duration == 0U &&
               inbound->start_position.x_q8 == 0 &&
               inbound->start_position.y_q8 == 0 &&
               inbound->target_position.x_q8 == 0 &&
               inbound->target_position.y_q8 == 0;
    }
    if (inbound->phase >= TECMO_GAMEPLAY_SCENE_INBOUND_PHASE_COUNT ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        inbound->restart_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        (uint8_t)scene->state.possession != inbound->restart_team ||
        inbound->passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        inbound->receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        inbound->defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        inbound->passer == inbound->receiver ||
        inbound->passer == inbound->defender ||
        inbound->receiver == inbound->defender ||
        scene->actors[inbound->passer].team != inbound->restart_team ||
        scene->actors[inbound->receiver].team != inbound->restart_team ||
        scene->actors[inbound->defender].team == inbound->restart_team ||
        scene->ball_holder != inbound->passer ||
        inbound->flight_duration < TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES ||
        inbound->flight_duration > TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES ||
        inbound->flight_frame > inbound->flight_duration) {
        return false;
    }
    if (inbound->phase == TECMO_GAMEPLAY_SCENE_INBOUND_SETUP) {
        return inbound->packed_animation_state == 0x32U;
    }
    if (inbound->phase == TECMO_GAMEPLAY_SCENE_INBOUND_GATHER) {
        return inbound->packed_animation_state == 0x32U ||
               inbound->packed_animation_state == 0x22U ||
               inbound->packed_animation_state == 0x12U ||
               inbound->packed_animation_state == 0x04U;
    }
    return inbound->phase == TECMO_GAMEPLAY_SCENE_INBOUND_FLIGHT &&
           inbound->packed_animation_state == 0x04U;
}

static bool scene_inbound_passer_for_restart(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam restart_team,
    uint8_t *passer_out)
{
    uint8_t selected;
    if (scene == NULL || passer_out == NULL ||
        restart_team >= TECMO_GAMEPLAY_TEAM_COUNT) {
        return false;
    }
    if (scene->live_foundation.score_restart_selection_active &&
        scene->live_foundation.score_restart_passer <
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene->actors[scene->live_foundation.score_restart_passer].team ==
            (uint8_t)restart_team) {
        *passer_out = scene->live_foundation.score_restart_passer;
        return true;
    }
    selected = scene->live_foundation.selected_actor_by_side[restart_team];
    if (selected < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene->actors[selected].team == (uint8_t)restart_team) {
        *passer_out = selected;
        return true;
    }
    /* The source captures the selected primary in $0308 before Bank06
       $9621. LIVE lacks a restart-specific raw owner for that selection, so
       this is an explicit identity fallback, not a claimed `$0308` policy. */
    *passer_out = scene_first_actor_for_team(restart_team);
    return *passer_out < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           scene->actors[*passer_out].team == (uint8_t)restart_team;
}

static bool scene_inbound_receiver_for_setup(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam restart_team,
    uint8_t passer,
    uint8_t *receiver_out)
{
    uint8_t candidate;
    if (scene == NULL || receiver_out == NULL ||
        restart_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    candidate = scene->live_foundation.candidate_actor_by_side[
        scene->live_foundation.offense_side];
    if (scene->live_foundation.offense_side == (uint8_t)restart_team &&
        candidate < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        candidate != passer &&
        scene->actors[candidate].team == (uint8_t)restart_team) {
        *receiver_out = candidate;
        return true;
    }
    /* Bank05 $B074 obtains a launch candidate through `$037F[$030A]`.
       If the typed candidate is unavailable for the current side, choose the
       existing nearest-teammate adapter explicitly instead of relabeling
       `$0309` or inventing a source candidate. */
    candidate = scene_nearest_actor_for_team(scene, restart_team, passer);
    if (candidate >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT || candidate == passer ||
        scene->actors[candidate].team != (uint8_t)restart_team) {
        return false;
    }
    *receiver_out = candidate;
    return true;
}

bool scene_begin_inbound(TecmoGameplayScene *scene,
                         TecmoGameplayTeam restart_team)
{
    TecmoGameplayState state_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayBackcourtState backcourt_before;
    TecmoGameplaySceneActor actors_before[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation foundation_before;
    TecmoGameplayCourtCoordinateQ8 ball_before;
    TecmoGameplaySceneInboundState inbound_before;
    uint8_t controlled_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayRoundSetup setup;
    TecmoGameplaySceneInboundState inbound;
    TecmoGameplayCourtCoordinateQ8 start;
    TecmoGameplayCourtCoordinateQ8 target;
    uint8_t passer;
    uint8_t receiver;
    uint8_t defender;
    uint16_t duration;
    int32_t dx;
    int32_t dy;
    uint32_t distance;

    if (scene == NULL || restart_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != restart_team ||
        scene_pass_active(scene) || scene_inbound_active(scene) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene_inbound_passer_for_restart(scene, restart_team, &passer)) {
        return false;
    }
    state_before = scene->state;
    orientation_before = scene->orientation_state;
    camera_before = scene->camera_state;
    backcourt_before = scene->backcourt_state;
    memcpy(actors_before, scene->actors, sizeof(actors_before));
    foundation_before = scene->live_foundation;
    ball_before = scene->ball_position;
    inbound_before = scene->inbound_state;
    memcpy(controlled_before, scene->controlled_actor,
           sizeof(controlled_before));

    /* Bank07 reaches Bank06 $9621 after presentation/reset. The native
       ownership/orientation transaction remains explicit, then only the
       exact TGFL base-branch positions are applied below. */
    if (((scene->state.possession != restart_team ||
          scene->ball_holder != passer) &&
         !scene_handoff_possession(scene, restart_team, passer)) ||
        !scene_sync_live_foundation(scene)) {
        goto reject;
    }
    defender = scene->live_foundation.defender_actor;
    if (defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT || defender == passer ||
        scene->actors[defender].team == (uint8_t)restart_team ||
        !tecmo_gameplay_free_throw_lineup_derive_round_setup(
            &scene->free_throw_lineup_assets,
            scene->orientation_state.attack_direction, passer, defender,
            &setup)) {
        goto reject;
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplayCourtCoordinate position;
        if (!setup.actors[actor].position_defined) goto reject;
        position.x = (int16_t)setup.actors[actor].raw_world_x;
        position.y = (int16_t)setup.actors[actor].raw_world_y;
        if (!scene_actor_coordinate_valid(&position)) goto reject;
        scene->actors[actor].position = position;
        scene->actors[actor].anchor = position;
    }
    if (!scene_attach_ball(scene) || !scene_sync_live_foundation(scene) ||
        ((!scene->live_foundation.score_restart_selection_active &&
          scene->live_foundation.primary_actor != passer) ||
         (scene->live_foundation.score_restart_selection_active &&
          scene->live_foundation.score_restart_passer != passer)) ||
        scene->live_foundation.defender_actor != defender ||
        !scene_inbound_receiver_for_setup(scene, restart_team, passer,
                                          &receiver) ||
        !scene_attached_ball_position(&scene->actors[passer], &start) ||
        !scene_attached_ball_position(&scene->actors[receiver], &target)) {
        goto reject;
    }
    dx = target.x_q8 - start.x_q8;
    dy = target.y_q8 - start.y_q8;
    distance = (uint32_t)((dx < 0 ? -dx : dx) +
                          (dy < 0 ? -dy : dy)) / 256U;
    /* $B42F consumes a table-derived trajectory ratio, but that duration
       table is not a strict pass asset yet. Reuse the existing bounded native
       trajectory adapter while retaining the source-selected setup/catch
       ordering and mode as distinct inbound state. */
    duration = (uint16_t)(TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES +
                          distance / 12U);
    if (duration > TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES) {
        duration = TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES;
    }
    memset(&inbound, 0, sizeof(inbound));
    inbound.phase = TECMO_GAMEPLAY_SCENE_INBOUND_SETUP;
    inbound.passer = passer;
    inbound.receiver = receiver;
    inbound.defender = defender;
    inbound.restart_team = (uint8_t)restart_team;
    inbound.packed_animation_state = 0x32U;
    inbound.flight_duration = duration;
    inbound.start_position = start;
    inbound.target_position = target;
    scene->inbound_state = inbound;
    scene->ball_position = start;
    return scene_inbound_state_valid(scene);

reject:
    scene->state = state_before;
    scene->orientation_state = orientation_before;
    scene->camera_state = camera_before;
    scene->backcourt_state = backcourt_before;
    memcpy(scene->actors, actors_before, sizeof(actors_before));
    scene->live_foundation = foundation_before;
    scene->ball_position = ball_before;
    scene->inbound_state = inbound_before;
    memcpy(scene->controlled_actor, controlled_before,
           sizeof(controlled_before));
    return false;
}

bool scene_begin_scored_inbound(TecmoGameplayScene *scene,
                                TecmoGameplayTeam restart_team)
{
    TecmoGameplayScene candidate;
    TecmoGameplayCourtCoordinate positions[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t directions[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveAutoPassSelection selection;
    uint8_t restart_primary;
    uint8_t presentation_receiver;
    size_t actor;
    if (scene == NULL || restart_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->live_foundation.defender_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    candidate = *scene;
    restart_primary = candidate.live_foundation.defender_actor;

    /* A made-score transition is not Bank05's miss-claimant path. Canonical
       $8FAD-$8FB9 swaps the side and selected actor pairs, $8FE8 clears both
       selected lifecycles, and $9042 toggles all role bits before Bank07
       reaches the $9621 restart setup. Stage the existing orientation/holder
       handoff with the source-selected prior defender, apply that typed LIVE
       transaction, and enter the existing inbound immediately. No ordinary
       Bank06 AI update can consume the old primary's formation cursor between
       the score and the eventual $B24F->$96B6 catch transaction. */
    if (!scene_handoff_possession(
            &candidate, restart_team, restart_primary)) return false;
    if (!tecmo_gameplay_live_foundation_score_restart_transition(
            &candidate.cpu_steering_assets, (uint8_t)restart_team,
            &candidate.live_foundation)) return false;
    /* The scene orientation handoff precedes Bank05's scored-restart AI
       transaction. Publish that already-committed attack direction before
       `$901F` evaluates its orientation-specific unsigned distance. */
    candidate.live_foundation.orientation =
        candidate.orientation_state.attack_direction;
    if (candidate.live_foundation.primary_actor != restart_primary ||
        candidate.ball_holder != restart_primary) return false;
    presentation_receiver = candidate.live_foundation.candidate_actor_by_side[
        restart_team];
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        positions[actor] = candidate.actors[actor].position;
        directions[actor] = candidate.actors[actor].movement_direction;
        if (directions[actor] >=
            TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
            /* Scene actors do not retain raw `$0463` while standing. Its
               only observable reset distinction here is horizontal facing;
               map that retained presentation to the matching cardinal slot
               before the exact `$88B0` tables are applied. */
            directions[actor] = candidate.actors[actor].facing_right
                ? 0U : 4U;
        }
    }
    if (!tecmo_gameplay_live_foundation_score_restart_auto_pass_select(
            &candidate.cpu_steering_assets, positions, directions,
            &candidate.live_foundation, &selection) ||
        selection.contract_tag !=
            TECMO_GAMEPLAY_LIVE_AUTO_PASS_SELECTION_TAG ||
        !candidate.live_foundation.score_restart_selection_active ||
        candidate.live_foundation.score_restart_passer != restart_primary ||
        candidate.live_foundation.play_state.stream_offset[
            candidate.live_foundation.primary_actor] != 0x0168U) {
        return false;
    }
    if (candidate.live_foundation.control_mode[restart_team] == 0U) {
        uint8_t selected_candidate =
            candidate.live_foundation.candidate_actor_by_side[restart_team];
        /* Human control still receives the exact state-1 selection, but its
           already-owned inbound presentation retains the receiver that was
           published by the scored-restart role swap. Restore the state-1
           candidate after setup; gather atomically publishes the locked
           inbound receiver. */
        candidate.live_foundation.candidate_actor_by_side[restart_team] =
            presentation_receiver;
        candidate.live_foundation.play_state.candidate_actor =
            presentation_receiver;
        if (!scene_begin_inbound(&candidate, restart_team)) return false;
        candidate.live_foundation.candidate_actor_by_side[restart_team] =
            selected_candidate;
        candidate.live_foundation.play_state.candidate_actor =
            selected_candidate;
    }
    if (!scene_inbound_state_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

bool scene_update_inbound(TecmoGameplayScene *scene)
{
    TecmoGameplaySceneInboundState inbound;
    TecmoGameplayLiveFoundation foundation_candidate;
    TecmoGameplayCourtCoordinateQ8 ball;
    bool receiver_facing_right;
    uint32_t step;

    if (scene == NULL || !scene_inbound_active(scene) ||
        !scene_inbound_state_valid(scene)) {
        return false;
    }
    inbound = scene->inbound_state;
    if (inbound.phase == TECMO_GAMEPLAY_SCENE_INBOUND_SETUP) {
        inbound.phase = TECMO_GAMEPLAY_SCENE_INBOUND_GATHER;
        if (!scene_attached_ball_position(&scene->actors[inbound.passer],
                                          &ball)) {
            return false;
        }
        scene->inbound_state = inbound;
        scene->ball_position = ball;
        return scene_inbound_state_valid(scene);
    }
    if (inbound.phase == TECMO_GAMEPLAY_SCENE_INBOUND_GATHER) {
        if (inbound.packed_animation_state == 0x32U) {
            inbound.packed_animation_state = 0x22U;
        } else if (inbound.packed_animation_state == 0x22U) {
            inbound.packed_animation_state = 0x12U;
        } else if (inbound.packed_animation_state == 0x12U) {
            inbound.packed_animation_state = 0x04U;
        } else {
            /* Bank05 $86A8 only launches the shared route at full-byte $04. */
            inbound.phase = TECMO_GAMEPLAY_SCENE_INBOUND_FLIGHT;
            inbound.flight_frame = 0U;
        }
        if (inbound.phase == TECMO_GAMEPLAY_SCENE_INBOUND_GATHER) {
            if (!scene_attached_ball_position(&scene->actors[inbound.passer],
                                              &ball)) {
                return false;
            }
            scene->inbound_state = inbound;
            scene->ball_position = ball;
            return scene_inbound_state_valid(scene);
        }
    }
    if (inbound.phase != TECMO_GAMEPLAY_SCENE_INBOUND_FLIGHT) return false;
    if (inbound.flight_frame < inbound.flight_duration) ++inbound.flight_frame;
    step = inbound.flight_frame;
    ball.x_q8 = inbound.start_position.x_q8 + (int32_t)(
        ((int64_t)(inbound.target_position.x_q8 - inbound.start_position.x_q8) *
         step) / inbound.flight_duration);
    ball.y_q8 = inbound.start_position.y_q8 + (int32_t)(
        ((int64_t)(inbound.target_position.y_q8 - inbound.start_position.y_q8) *
         step) / inbound.flight_duration);
    if (inbound.flight_frame < inbound.flight_duration) {
        scene->inbound_state = inbound;
        scene->ball_position = ball;
        return scene_inbound_state_valid(scene);
    }
    if (!scene_goal_facing_right_for_team(
            scene, (TecmoGameplayTeam)inbound.restart_team,
            &receiver_facing_right)) {
        return false;
    }
    /* Human presentation retains the score-restart marker and its physical
       passer provenance through flight. Retire it atomically with the shared
       `$B24F` catch transaction, never during setup. */
    foundation_candidate = scene->live_foundation;
    if ((foundation_candidate.score_restart_selection_active &&
         !tecmo_gameplay_live_foundation_score_restart_gather(
             &scene->cpu_steering_assets,
             TECMO_GAMEPLAY_LIVE_SCORE_RESTART_GATHER_HUMAN_INBOUND,
             inbound.passer, inbound.receiver, &foundation_candidate)) ||
        (!scene->legacy_direct_launch &&
         !tecmo_gameplay_live_foundation_pass_handoff(
             &scene->cpu_steering_assets, inbound.receiver,
             &foundation_candidate))) {
        return false;
    }
    scene->live_foundation = foundation_candidate;
    scene->actors[inbound.receiver].facing_right = receiver_facing_right;
    scene->ball_holder = inbound.receiver;
    for (size_t controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] == inbound.restart_team) {
            scene->controlled_actor[controller] = inbound.receiver;
        }
    }
    scene_inbound_clear(scene);
    /* Match the shared `$B24F` endpoint. Once the typed catch assigns the
       holder and goal-facing, resolve its normal live dribble/held-ball
       coordinate; do not leave the linear flight endpoint visible. */
    if (!scene_attach_ball(scene)) return false;
    return scene_inbound_state_valid(scene);
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
    input.orientation = scene->orientation_state.attack_direction;
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

static uint8_t scene_candidate_direction_nibble(
    const TecmoControlFrame *controls)
{
    uint8_t value = 0U;
    if (controls == NULL) return 0U;
    if (controls->held.right) value |= 0x01U;
    if (controls->held.left) value |= 0x02U;
    if (controls->held.down) value |= 0x04U;
    if (controls->held.up) value |= 0x08U;
    return value;
}

static const TecmoControlFrame *scene_candidate_controls_for_side(
    const TecmoGameplayScene *scene,
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT],
    uint8_t side)
{
    size_t controller;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == side)
            return controls[controller];
    }
    return NULL;
}

static bool scene_directional_candidate(
    const TecmoGameplayScene *scene,
    uint8_t side, uint8_t reference_actor, uint8_t excluded_actor,
    uint8_t sector, uint8_t polarity,
    uint8_t *actor_out, uint16_t *score_out)
{
    TecmoGameplayCandidateInput input;
    TecmoGameplayCandidateResult result;
    size_t actor;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
    input.direction_sector = sector;
    input.excluded_actor = excluded_actor;
    input.required_polarity = polarity;
    input.reference_actor = reference_actor;
    input.viewport_x = scene->camera_state.camera_x;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        input.actor_x[actor] = (uint16_t)scene->actors[actor].position.x;
        input.actor_depth[actor] =
            (uint8_t)scene->actors[actor].position.y;
        input.actor_flags[actor] =
            scene->live_foundation.actor_selector_flags[actor];
    }
    if (!tecmo_gameplay_candidate_directional_select(&input, &result))
        return false;
    if (!result.wrote_candidate) return true;
    if (result.candidate_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[result.candidate_actor].team != side) return false;
    *actor_out = result.candidate_actor;
    *score_out = result.candidate_score;
    return true;
}

bool scene_update_selection_candidates(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT])
{
    TecmoGameplayLiveFoundation candidate;
    uint8_t offense;
    uint8_t defense;
    uint8_t actor;
    uint16_t score;
    uint8_t sector;
    size_t index;
    if (scene == NULL || controls == NULL || scene->legacy_direct_launch ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        return scene != NULL && scene->legacy_direct_launch;
    }
    candidate = scene->live_foundation;
    offense = candidate.offense_side;
    defense = candidate.defense_side;

    /* Fixed loop $F05E: Bank06 $B139 offense first. */
    actor = candidate.candidate_actor_by_side[offense];
    score = candidate.candidate_score_by_side[offense];
    if (candidate.control_mode[offense] != 0U) {
        uint8_t direction = scene->actors[candidate.primary_actor]
            .movement_direction;
        if (direction >= 8U) return false;
        sector = tecmo_gameplay_candidate_cpu_direction_sector[direction];
    } else {
        sector = scene_candidate_direction_nibble(
            scene_candidate_controls_for_side(scene, controls, offense));
    }
    if (!scene_directional_candidate(
            scene, offense, candidate.primary_actor,
            candidate.selected_actor_by_side[offense], sector, 0U,
            &actor, &score)) return false;
    if (sector != 0U) {
        candidate.candidate_actor_by_side[offense] = actor;
        candidate.candidate_score_by_side[offense] = score;
    }
    candidate.candidate_sector_by_side[offense] = sector;

    /* Fixed loop $F061: Bank06 $B104 defense second. */
    actor = candidate.candidate_actor_by_side[defense];
    score = candidate.candidate_score_by_side[defense];
    if (candidate.control_mode[defense] != 0U) {
        TecmoGameplayDefenseContactB06ScanInput input;
        TecmoGameplayDefenseContactB06ScanResult result;
        uint8_t x_low[10], x_high[10], depth[10], flags[10];
        memset(&input, 0, sizeof(input));
        for (index = 0U; index < 10U; ++index) {
            uint16_t x = (uint16_t)scene->actors[index].position.x;
            x_low[index] = (uint8_t)x;
            x_high[index] = (uint8_t)(x >> 8U);
            depth[index] = (uint8_t)scene->actors[index].position.y;
            flags[index] = candidate.actor_selector_flags[index];
        }
        input.contract_tag =
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_INPUT_TAG;
        input.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
        input.raw_0309 = candidate.defender_actor;
        input.raw_030b = defense;
        input.raw_007d = (uint8_t)((uint32_t)scene->ball_position.x_q8 >> 8U);
        input.raw_00f2 = (uint8_t)((uint32_t)scene->ball_position.x_q8 >> 16U);
        input.raw_00fd = (uint8_t)((uint32_t)scene->ball_position.y_q8 >> 8U);
        input.raw_06d5 = actor;
        input.raw_06d7 = 0U;
        input.raw_037f_at_030b = actor;
        input.raw_0073_low = x_low; input.raw_0073_low_count = 10U;
        input.raw_00e8_high = x_high; input.raw_00e8_high_count = 10U;
        input.raw_00f3_depth = depth; input.raw_00f3_depth_count = 10U;
        input.raw_04b0_by_slot = flags; input.raw_04b0_by_slot_count = 10U;
        if (!tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
                &input, &result)) return false;
        actor = result.raw_037f_at_030b;
        score = (uint16_t)result.raw_06d7 |
            (uint16_t)((uint16_t)result.raw_06d8 << 8U);
        candidate.candidate_sector_by_side[defense] = 0U;
    } else {
        sector = scene_candidate_direction_nibble(
            scene_candidate_controls_for_side(scene, controls, defense));
        if (!scene_directional_candidate(
                scene, defense, candidate.defender_actor,
                candidate.selected_actor_by_side[defense], sector, 0x10U,
                &actor, &score)) return false;
        candidate.candidate_sector_by_side[defense] = sector;
        if (sector == 0U) {
            if (!tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &candidate)) return false;
            scene->live_foundation = candidate;
            return true;
        }
    }
    if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[actor].team != defense) return false;
    candidate.candidate_actor_by_side[defense] = actor;
    candidate.candidate_score_by_side[defense] = score;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate)) return false;
    scene->live_foundation = candidate;
    return true;
}

bool scene_pass_or_switch(TecmoGameplayScene *scene,
                                  size_t controller)
{
    TecmoGameplayTeam team;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplayLiveFoundation candidate_foundation;
    bool facing_right;
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM) {
        return true;
    }
    team = (TecmoGameplayTeam)scene->launch.controller_team[controller];
    if (team == scene->state.possession &&
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        uint8_t next = scene->legacy_direct_launch
            ? scene_next_teammate(scene, scene->ball_holder)
            : scene->live_foundation.candidate_actor_by_side[
                scene->live_foundation.offense_side];
        if (next < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            if (!scene->legacy_direct_launch) {
                return scene_begin_pass(scene, controller, next);
            }
            /* A pass is an action boundary: the new holder starts from the
               validated attack-facing baseline, then movement/shot actions
               may deliberately override it. Stage both facing and ball
               attachment before committing the holder switch. */
            if (!scene_goal_facing_right_for_team(
                    scene, team, &facing_right)) {
                return false;
            }
            memcpy(candidate_actors, scene->actors,
                   sizeof(candidate_actors));
            candidate_actors[next].facing_right = facing_right;
            if (!scene_ball_position_for_actors(
                    scene, candidate_actors, next, &candidate_ball)) {
                return false;
            }
            candidate_foundation = scene->live_foundation;
            if (!scene->legacy_direct_launch &&
                !tecmo_gameplay_live_foundation_pass_handoff(
                    &scene->cpu_steering_assets, next,
                    &candidate_foundation)) {
                return false;
            }
            memcpy(scene->actors, candidate_actors,
                   sizeof(candidate_actors));
            scene->ball_holder = next;
            scene->controlled_actor[controller] = next;
            scene->ball_position = candidate_ball;
            if (!scene->legacy_direct_launch) {
                scene->live_foundation = candidate_foundation;
            }
            return true;
        }
    } else {
        uint8_t next = scene->legacy_direct_launch
            ? scene_nearest_actor_for_team(scene, team, scene->ball_holder)
            : scene->live_foundation.candidate_actor_by_side[
                scene->live_foundation.defense_side];
        if (next >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[next].team != team) return false;
        scene->controlled_actor[controller] = next;
        if (!scene->legacy_direct_launch) {
            uint8_t old = scene->live_foundation.defender_actor;
            scene->live_foundation.defender_actor = next;
            scene->live_foundation.play_state.defender_actor = next;
            scene->live_foundation.selected_actor_by_side[
                scene->live_foundation.defense_side] = next;
            scene->live_foundation.candidate_actor_by_side[
                scene->live_foundation.defense_side] = old;
            scene->live_foundation.selected_defender_handoff_active = false;
        }
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

static bool scene_cpu_route_target_metadata(
    const TecmoGameplayCpuSteeringPlayState *play_state,
    size_t actor,
    TecmoGameplayCourtCoordinate *target_out,
    TecmoGameplayCpuSteeringHarnessTargetKind *target_kind_out)
{
    uint8_t target_object;
    TecmoGameplayCourtCoordinate target;
    if (play_state == NULL || target_out == NULL || target_kind_out == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    target_object = play_state->target_object[actor];
    target.x = play_state->target_x[actor];
    target.y = play_state->target_depth[actor];
    if (!tecmo_gameplay_court_coordinate_valid(&target)) return false;
    if (target_object < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        *target_kind_out =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR;
    } else if (target_object ==
                   TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT) {
        *target_kind_out =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET;
    } else {
        return false;
    }
    *target_out = target;
    return true;
}

static bool scene_cpu_route_publish_metadata(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveFoundation *foundation,
    size_t actor,
    uint8_t direction,
    bool writes_direction,
    bool advance_decision_serial,
    TecmoGameplaySceneCpuActor *cpu)
{
    TecmoGameplayCourtCoordinate target;
    TecmoGameplayCpuSteeringHarnessTargetKind target_kind;
    uint8_t direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    if (scene == NULL || foundation == NULL || cpu == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        (advance_decision_serial && cpu->decision_serial == UINT32_MAX) ||
        !scene_cpu_route_target_metadata(
            &foundation->play_state, actor, &target, &target_kind) ||
        (writes_direction && !scene_cpu_route_direction_bits(
            scene, direction, &direction_bits))) {
        return false;
    }
    /* decision_serial counts source-command decisions at the C integration
       seam. Bank06 state-5 ticks continue the frozen opcode-4 decision and
       therefore must not look like new TGMO steering decisions. */
    if (advance_decision_serial) ++cpu->decision_serial;
    cpu->snapshot_fingerprint = 0U;
    cpu->target_position = target;
    cpu->target_kind = (uint8_t)target_kind;
    cpu->direction = writes_direction
        ? direction : TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    cpu->held_direction_bits = direction_bits;
    cpu->target_valid = true;
    cpu->writes_direction = writes_direction;
    cpu->command_offset = TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
    cpu->linked_actor = foundation->play_state.fixed_link[actor];
    return scene_cpu_actor_state_valid(scene, actor, cpu);
}

static bool scene_cpu_route_launch(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCpuSteeringPlayResult *play_result,
    size_t actor,
    const TecmoTeamDataPlayer *player,
    TecmoGameplayLiveFoundation *foundation,
    TecmoGameplaySceneCpuActor *cpu,
    bool *route_owned_out)
{
    TecmoGameplayCpuRouteProfileInput profile_input;
    TecmoGameplayCpuRouteProfileResult profile_result;
    TecmoGameplayCpuSteeringRouteLaunchInput input;
    TecmoGameplayCpuSteeringRouteLaunchResult result;
    bool extra_adjust_admitted;
    if (scene == NULL || play_result == NULL || player == NULL ||
        foundation == NULL || cpu == NULL || route_owned_out == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        play_result->actor != actor || !play_result->fetched ||
        play_result->command.opcode != 4U ||
        play_result->effect !=
            TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_TARGET ||
        scene->launch.speed_value >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT ||
        scene->launch.difficulty >= 3U ||
        scene->actors[actor].condition > 100U) {
        return false;
    }
    *route_owned_out = true;
    if (play_result->target_vector_zero) {
        foundation->play_state.route_motion[actor].active = false;
        foundation->play_state.actor_state[actor] = 0x04U;
        return scene_cpu_route_publish_metadata(
            scene, foundation, actor,
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION, false, true, cpu);
    }

    /* Bank02 $A89E-$A90D owns the raw $06E7 profile byte. The speed-mode
       adjustment is exact. Until LIVE owns raw $030C/$030D parity, the
       explicitly typed native approximation admits $A908 only when this
       actor's side has no assigned controller; it must not be described as
       an exact mirror of either raw side-control byte. */
    extra_adjust_admitted = !scene_team_has_controller(
        scene, scene->actors[actor].team);
    memset(&profile_input, 0, sizeof(profile_input));
    profile_input.contract_tag =
        TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_INPUT_TAG;
    profile_input.actor = (uint8_t)actor;
    profile_input.team = (uint8_t)scene->actors[actor].team;
    profile_input.roster_index = scene->actors[actor].roster_index;
    profile_input.profile_movement_byte = player->profile[0];
    profile_input.speed_value = scene->launch.speed_value;
    profile_input.difficulty = scene->launch.difficulty;
    profile_input.condition_7c48 = scene->actors[actor].condition;
    profile_input.extra_adjust_admission_available = true;
    profile_input.extra_adjust_admitted = extra_adjust_admitted;
    if (!tecmo_gameplay_cpu_route_profile_project(
            &scene->movement_assets, &scene->rebound_audit,
            &profile_input, &profile_result)) {
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_INPUT_TAG;
    input.actor_position = scene->actors[actor].position;
    input.horizontal_delta = play_result->target_horizontal_delta;
    input.depth_delta = play_result->target_depth_delta;
    input.condition_7c48 = profile_result.condition_7c48;
    input.movement_value_06e7 = profile_result.movement_value_06e7;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &scene->cpu_steering_assets, &input, &result) ||
        !result.launched || !result.motion.active ||
        result.direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
        return false;
    }
    foundation->play_state.route_motion[actor] = result.motion;
    foundation->play_state.actor_state[actor] = 0x05U;
    if (!scene_cpu_route_publish_metadata(
            scene, foundation, actor, result.direction, true, true, cpu)) {
        return false;
    }
    return true;
}

static bool scene_cpu_route_step(
    const TecmoGameplayScene *scene,
    size_t actor,
    uint8_t completion_side_bit_0359,
    TecmoGameplayLiveFoundation *foundation,
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    TecmoGameplaySceneCpuActor *cpu)
{
    TecmoGameplayCpuSteeringRouteMotionState next;
    TecmoGameplayCpuSteeringRouteStepResult result;
    TecmoGameplayCourtCoordinate position;
    if (scene == NULL || foundation == NULL || candidate_actors == NULL ||
        cpu == NULL || actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        foundation->play_state.actor_state[actor] != 0x05U ||
        !foundation->play_state.route_motion[actor].active ||
        !tecmo_gameplay_cpu_steering_route_step(
            (uint8_t)actor, completion_side_bit_0359,
            &foundation->play_state.route_motion[actor], &next, &result)) {
        return false;
    }
    position.x = (int16_t)result.horizontal_position;
    position.y = (int16_t)result.depth_position;
    candidate_actors[actor].position = position;
    /* TGAI-3 is deliberately not passed through TGMO on an active route.
       Bank06 $8999-$89D3 pose/action writes remain unowned presentation
       effects, so the actor's TGMO locomotion fields are preserved. Raw
       Q6 wrap is never clamped; a projection outside the scene court rejects
       this complete candidate transaction. */
    if (!scene_actor_world_position_valid(&candidate_actors[actor])) {
        return false;
    }
    foundation->play_state.route_motion[actor] = next;
    if (result.finished) {
        foundation->play_state.actor_state[actor] = 0x04U;
    }
    return scene_cpu_route_publish_metadata(
        scene, foundation, actor, cpu->direction,
        cpu->writes_direction, false, cpu);
}

/* Bank05's state-$17 claim changes possession, not the center objects. Until
   the special pre-tip recovery has naturally landed the active jumper, both
   center slots remain anchored by scene_pretip_apply_jump_frame. Letting
   TGMO/CPU steering process either slot in that LIVE update moved it away
   from its anchor and made the court renderer reject the projection contract. */
static bool scene_actor_in_pretip_recovery(const TecmoGameplayScene *scene,
                                           size_t actor)
{
    size_t jumper;
    if (scene == NULL || actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->pretip_jump_active || !scene->pretip_state.live_handoff ||
        !scene->pretip_state.simulation_active) {
        return false;
    }
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        uint8_t jumper_actor = scene->pretip_jumper_actor[jumper];
        /* scene_pretip_apply_jump_frame anchors both center objects while
           either tip jump is active.  The non-winning jumper is still part
           of that renderer contract, even when it never committed a jump. */
        if (jumper_actor == actor)
            return true;
    }
    return false;
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

static bool scene_cpu_source_target(
    const TecmoGameplayCpuSteeringPlayState *play_state,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    const TecmoGameplayCourtCoordinate *ball_position,
    size_t actor,
    TecmoGameplayCourtCoordinate *target_out,
    TecmoGameplayCpuSteeringHarnessTargetKind *target_kind_out)
{
    uint8_t target_object;
    TecmoGameplayCourtCoordinate target;
    if (play_state == NULL || actor_position == NULL || ball_position == NULL ||
        target_out == NULL ||
        target_kind_out == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    target_object = play_state->target_object[actor];
    if (target_object < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        /* Native-faithful adapter policy: a player-object target follows the
           current referenced player coordinate on every immutable post-human
           snapshot/tick. The stored source coordinate remains evidence;
           original Bank05 dynamic retarget/matchup semantics remain
           incomplete/unproven. */
        target = actor_position[target_object];
        *target_kind_out =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR;
    } else if (target_object ==
               TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT) {
        /* Bank06 opcode 4 loads C8 as an object index. The strict corpus's
           C8=$0A record resolves to the separately owned production ball
           coordinate, never an eleventh actor stream. */
        target = *ball_position;
        *target_kind_out =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET;
    } else {
        if (target_object != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            return false;
        }
        target.x = play_state->target_x[actor];
        target.y = play_state->target_depth[actor];
        *target_kind_out =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
    }
    if (!tecmo_gameplay_court_coordinate_valid(&target)) return false;
    *target_out = target;
    return true;
}

bool scene_cpu_target_for_source_direction(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate *actor_position,
    uint8_t source_direction,
    TecmoGameplayCourtCoordinate *target_out)
{
    bool found = false;
    int64_t best_distance = INT64_MAX;
    int32_t best_abs_sum = INT32_MAX;
    int32_t best_horizontal = INT32_MAX;
    int32_t best_depth = INT32_MAX;
    TecmoGameplayCourtCoordinate best_target = {0};
    if (assets == NULL || actor_position == NULL || target_out == NULL ||
        source_direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT ||
        !tecmo_gameplay_court_coordinate_valid(actor_position)) {
        return false;
    }
    /* TGMO consumes held bits and derives its direction from a target. When
       the accepted play state supplies a direction without a target write,
       choose the nearest deterministic in-court target whose exact TGAI
       octant equals that direction. This is an owned TGAI->TGMO composition
       and an explicit native adapter policy; it does not invent a ROM
       command argument. */
    for (int32_t horizontal = -64; horizontal <= 64; horizontal += 8) {
        for (int32_t depth = -64; depth <= 64; depth += 8) {
            TecmoGameplayCourtCoordinate candidate;
            uint8_t direction;
            int64_t distance;
            int32_t abs_sum;
            if (horizontal == 0 && depth == 0) continue;
            if ((int32_t)actor_position->x + horizontal <
                    TECMO_GAMEPLAY_COURT_WORLD_MIN_X ||
                (int32_t)actor_position->x + horizontal >
                    TECMO_GAMEPLAY_COURT_WORLD_MAX_X ||
                (int32_t)actor_position->y + depth <
                    TECMO_GAMEPLAY_COURT_WORLD_MIN_Y ||
                (int32_t)actor_position->y + depth >
                    TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
                continue;
            }
            candidate.x = (int16_t)((int32_t)actor_position->x + horizontal);
            candidate.y = (int16_t)((int32_t)actor_position->y + depth);
            /* Scene movement uses the playable court polygon, which is
               narrower than the decoded full TGCT world at its baseline and
               sidelines. Never synthesize a TGMO target in that outside
               staging area merely because it is inside the raw world box. */
            if (!scene_actor_coordinate_valid(&candidate)) continue;
            if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                    assets, (int16_t)horizontal, (int16_t)depth,
                    &direction) || direction != source_direction) {
                continue;
            }
            distance = (int64_t)horizontal * (int64_t)horizontal +
                       (int64_t)depth * (int64_t)depth;
            abs_sum = (horizontal < 0 ? -horizontal : horizontal) +
                      (depth < 0 ? -depth : depth);
            if (!found || distance < best_distance ||
                (distance == best_distance &&
                 (abs_sum < best_abs_sum ||
                  (abs_sum == best_abs_sum &&
                   (horizontal < best_horizontal ||
                    (horizontal == best_horizontal &&
                     depth < best_depth)))))) {
                found = true;
                best_distance = distance;
                best_abs_sum = abs_sum;
                best_horizontal = horizontal;
                best_depth = depth;
                best_target = candidate;
            }
        }
    }
    if (!found) return false;
    *target_out = best_target;
    return true;
}

bool scene_cpu_common_tail_has_ordinary_live_zero(
    const TecmoGameplayScene *scene)
{
    /* Bank05 $86DD-$8798 owns nonzero $BA low bits while shot/recovery
       lifecycles are active, and Bank05 $8FAD permits the ordinary possession
       handoff only when ($BA & 3) is zero.  This is a narrow projection for
       Bank06 $92CA's same low-bit gate, not a raw-RAM mirror or timer value. */
    return scene != NULL && scene->active && !scene->result_ready &&
           !scene->pretip_abort_pending &&
           scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
           scene->state.violation == TECMO_GAMEPLAY_VIOLATION_NONE &&
           scene->state.free_throws.attempts_remaining == 0U &&
           scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           !scene_pass_active(scene) && !scene->free_throw_lineup_active &&
           !tecmo_gameplay_scene_in_dunk_presentation(scene);
}

bool scene_cpu_opcode21_flags_007e(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    const TecmoGameplayLiveFoundation *foundation,
    uint8_t *flags_007e_out)
{
    uint8_t primary;
    uint8_t orientation;
    uint16_t x;
    uint8_t depth;
    if (flags_007e_out == NULL) return false;
    *flags_007e_out = 0U;
    if (scene == NULL || actor_position == NULL || foundation == NULL ||
        !scene_cpu_common_tail_has_ordinary_live_zero(scene) ||
        foundation->primary_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        foundation->primary_actor != foundation->play_state.primary_actor ||
        scene->orientation_state.attack_direction > 1U) {
        return false;
    }
    /* Fixed `$F07E-$F0B9` clears bit 1 every loop. Its `$0478==0` gate is
       exactly the already-owned ordinary-LIVE seam above. It then requires
       primary depth `$7B..$AE` and sets bit 1 beyond the attacking baseline:
       raw X below `$00F8` for orientation 0, or at/above `$0208` for 1. */
    primary = foundation->primary_actor;
    orientation = scene->orientation_state.attack_direction;
    if (!tecmo_gameplay_court_coordinate_valid(&actor_position[primary])) {
        return false;
    }
    x = (uint16_t)actor_position[primary].x;
    depth = (uint8_t)actor_position[primary].y;
    if (depth >= 0x7BU && depth < 0xAFU &&
        ((orientation == 0U && x < 0x00F8U) ||
         (orientation == 1U && x >= 0x0208U))) {
        *flags_007e_out = 0x02U;
    }
    return true;
}

static bool scene_cpu_current_ball_snapshot(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    TecmoGameplayCourtCoordinate *ball_out)
{
    TecmoGameplaySceneActor actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayBallDribbleFrame frame;
    size_t actor;
    if (scene == NULL || actor_position == NULL || ball_out == NULL) {
        return false;
    }
    /* In ordinary held-ball LIVE, scene_update_ai receives actor positions
       after human movement but scene->ball_position is not reattached until
       later. Recreate the exact current holder/dribble projection over that
       immutable snapshot. Pass/shot/non-LIVE transports keep their own ball
       owner and therefore retain the stored Q8 projection. */
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene_pass_active(scene) &&
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        memcpy(actors, scene->actors, sizeof(actors));
        for (actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            actors[actor].position = actor_position[actor];
        }
        if (!scene_live_ball_frame_for_actors(
                scene, actors, scene->ball_holder, &frame)) {
            return false;
        }
        *ball_out = frame.visible_position;
        return true;
    }
    return tecmo_gameplay_court_coordinate_q8_floor(
        &scene->ball_position, ball_out);
}

bool scene_cpu_opcode10_selector_project(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    const TecmoGameplayLiveFoundation *foundation,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    TecmoGameplayBackcourtState backcourt;
    TecmoGameplayBackcourtStepInput backcourt_input;
    TecmoGameplayCpuOpcode10SelectorInput selector_input;
    TecmoGameplayCpuOpcode10SelectorResult selector_result;
    bool violation = false;
    size_t actor;
    if (scene == NULL || actor_position == NULL || foundation == NULL ||
        input == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG ||
        foundation->primary_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        foundation->defender_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->orientation_state.attack_direction > 1U ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state)) {
        return false;
    }
    input->special_actor_07df_available = false;
    input->special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_actor_branch_context_available = false;
    input->linked_actor_resolved_valid = false;
    input->linked_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_relative_valid = false;
    input->linked_relative_x = 0;
    input->linked_relative_depth = 0;

    /* scene_update_ai precedes authoritative backcourt settlement. Preview
       the exact TGBC step against this immutable ball snapshot so Bank05
       $9737-$973C's bit-$10 projection has current-tick boundary cadence;
       neither state nor a possible violation is committed here. */
    memset(&backcourt_input, 0, sizeof(backcourt_input));
    if (!scene_cpu_current_ball_snapshot(
            scene, actor_position, &backcourt_input.ball_position)) {
        return false;
    }
    /* Opcode-4 ball targeting and the selector gate consume the same
       immutable post-human held-ball snapshot. */
    input->ball_position = backcourt_input.ball_position;
    backcourt_input.orientation =
        scene->orientation_state.attack_direction;
    backcourt_input.global_object_state = 0U;
    backcourt = scene->backcourt_state;
    if (!tecmo_gameplay_backcourt_step(
            &scene->backcourt_assets, &backcourt, &backcourt_input,
            &violation)) {
        return false;
    }

    memset(&selector_input, 0, sizeof(selector_input));
    selector_input.contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE10_SELECTOR_INPUT_TAG;
    selector_input.flags_0588 = backcourt.frontcourt_established != 0U
        ? 0x10U : 0U;
    /* This seam is only Bank02's ordinary $0478==0 path. BA bit $40 is not
       read on that branch and therefore is not manufactured here. */
    selector_input.context_0478 = 0U;
    selector_input.flags_ba = 0U;
    selector_input.primary_actor_0308 = foundation->primary_actor;
    selector_input.defender_actor_0309 = foundation->defender_actor;
    selector_input.orientation_035a =
        scene->orientation_state.attack_direction;
    /* The prior byte is deliberately unavailable. It cannot influence either
       kind of actual source store accepted below; a no-store result is never
       projected into LIVE. */
    selector_input.prior_special_actor_07df =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        selector_input.actor_selector_04b0[actor] =
            foundation->actor_selector_flags[actor];
        selector_input.dynamic_link_06cb[actor] =
            foundation->dynamic_link[actor];
        selector_input.actor_position[actor] = actor_position[actor];
    }
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(
            &selector_input, &selector_result)) {
        /* Typed world depth can exceed the selector's source byte. That is an
           unavailable projection, not a reason to invent truncation or fail
           the complete scene transaction. */
        return true;
    }
    if (!selector_result.candidate_stored &&
        !selector_result.explicit_ff_stored) {
        return true;
    }
    input->special_actor_07df_available = true;
    input->special_actor_07df = selector_result.special_actor_07df_after;
    input->linked_actor_branch_context_available = true;
    return true;
}

bool scene_cpu_opcode10_workspace_project(
    uint8_t actor,
    const TecmoGameplayLiveFoundation *foundation,
    const TecmoGameplaySceneOpcode10FrameContext *context,
    TecmoGameplayCpuSteeringPlayInput *input,
    TecmoGameplaySceneOpcode10Projection *projection_out)
{
    TecmoGameplayCpuOpcode10WorkspaceInput workspace_input;
    TecmoGameplayCpuOpcode10WorkspaceResult workspace_result;
    TecmoGameplaySceneOpcode10Projection projection;
    uint8_t linked;
    if (foundation == NULL || context == NULL || input == NULL ||
        projection_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        foundation->primary_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    memset(&projection, 0, sizeof(projection));
    projection.contract_tag = TECMO_GAMEPLAY_SCENE_OPCODE10_PROJECTION_TAG;
    input->linked_actor_resolved_valid = false;
    input->linked_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_relative_valid = false;
    input->linked_relative_x = 0;
    input->linked_relative_depth = 0;
    if (!input->special_actor_07df_available ||
        !input->linked_actor_branch_context_available) {
        *projection_out = projection;
        return true;
    }
    linked = actor == input->special_actor_07df
        ? foundation->primary_actor : foundation->dynamic_link[actor];
    if (linked >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    input->linked_actor_resolved_valid = true;
    input->linked_actor = linked;
    if (linked == foundation->primary_actor) {
        if (context->contract_tag !=
                TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG ||
            !context->available || context->sample_6a == 0U ||
            context->rate_index_075f >= 3U) {
            *projection_out = projection;
            return true;
        }
    }
    if (input->actor_position[linked].y < 0 ||
        input->actor_position[linked].y > UINT8_MAX) {
        *projection_out = projection;
        return true;
    }
    memset(&workspace_input, 0, sizeof(workspace_input));
    workspace_input.contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_INPUT_TAG;
    workspace_input.actor_index = actor;
    workspace_input.special_actor_07df = input->special_actor_07df;
    workspace_input.primary_actor_0308 = foundation->primary_actor;
    workspace_input.dynamic_link_06cb = foundation->dynamic_link[actor];
    workspace_input.orientation_035a = input->orientation_035a;
    workspace_input.linked_target_x =
        (uint16_t)input->actor_position[linked].x;
    workspace_input.linked_target_depth =
        (uint8_t)input->actor_position[linked].y;
    if (linked == foundation->primary_actor) {
        workspace_input.timer_0798 = context->timer_0798;
        workspace_input.rate_index_075f = context->rate_index_075f;
        workspace_input.sample_006a = context->sample_6a;
        workspace_input.timer_0760 = context->timer_bias_0760;
    }
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(
            &workspace_input, &workspace_result) ||
        workspace_result.linked_actor != linked ||
        (linked != foundation->primary_actor &&
         (workspace_result.timer_reloaded ||
          workspace_result.timer_decremented))) {
        return false;
    }
    input->linked_relative_valid = true;
    input->linked_relative_x = workspace_result.linked_relative_x;
    input->linked_relative_depth = workspace_result.linked_relative_depth;
    if (linked == foundation->primary_actor) {
        projection.primary_timer_pending = true;
        projection.timer_before = context->timer_0798;
        projection.timer_after = workspace_result.timer_0798_after;
    }
    *projection_out = projection;
    return true;
}

bool scene_cpu_opcode10_projection_commit(
    const TecmoGameplaySceneOpcode10Projection *projection,
    const TecmoGameplayCpuSteeringPlayResult *play_result,
    uint8_t *candidate_timer_io)
{
    if (projection == NULL || play_result == NULL ||
        candidate_timer_io == NULL ||
        projection->contract_tag !=
            TECMO_GAMEPLAY_SCENE_OPCODE10_PROJECTION_TAG) {
        return false;
    }
    if (!projection->primary_timer_pending || !play_result->fetched ||
        (play_result->command.opcode != 10U &&
         play_result->command.opcode != 12U) || play_result->deferred) {
        return true;
    }
    if (*candidate_timer_io != projection->timer_before) return false;
    *candidate_timer_io = projection->timer_after;
    return true;
}

static void scene_cpu_opcode12_context_begin(
    const TecmoGameplayLiveFoundation *foundation, uint8_t actor,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (input == NULL) return;
    input->opcode12_context_available = false;
    input->opcode12_automatic_offense = false;
    input->opcode12_actor_eligible = false;
    input->opcode12_linked_actor_06cb =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    if (foundation == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        foundation->offense_side >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        foundation->dynamic_link[actor] >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return;
    }
    input->opcode12_context_available = true;
    input->opcode12_automatic_offense =
        foundation->actor_team[actor] == foundation->offense_side &&
        foundation->control_mode[foundation->offense_side] != 0U;
    input->opcode12_actor_eligible =
        foundation->play_state.actor_state[actor] == 0x04U &&
        actor != foundation->defender_actor;
    input->opcode12_linked_actor_06cb = foundation->dynamic_link[actor];
}

static void scene_cpu_opcode12_context_end(
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (input == NULL) return;
    input->opcode12_context_available = false;
    input->opcode12_automatic_offense = false;
    input->opcode12_actor_eligible = false;
    input->opcode12_linked_actor_06cb =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
}

static bool scene_cpu_opcode16_context_result(
    const TecmoGameplaySceneOpcode16FrameContext *context,
    TecmoGameplayCpuOpcode16WorkspaceResult *result_out)
{
    TecmoGameplayCpuOpcode16WorkspaceInput input;
    TecmoGameplayCpuOpcode16WorkspaceResult result;
    if (context == NULL || result_out == NULL ||
        context->contract_tag !=
            TECMO_GAMEPLAY_SCENE_OPCODE16_FRAME_CONTEXT_TAG ||
        !context->available ||
        context->primary_actor_0308 >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        context->orientation_035a > 1U) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_INPUT_TAG;
    input.orientation_035a = context->orientation_035a;
    input.actor_position = context->primary_position;
    if (!tecmo_gameplay_cpu_opcode16_workspace_harness(&input, &result) ||
        result.workspace_036e != context->workspace_036e ||
        result.workspace_0370 != context->workspace_0370) {
        return false;
    }
    *result_out = result;
    return true;
}

bool scene_cpu_opcode16_workspace_capture(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneOpcode16FrameContext *context_out)
{
    TecmoGameplaySceneOpcode16FrameContext context;
    TecmoGameplayCpuOpcode16WorkspaceInput input;
    TecmoGameplayCpuOpcode16WorkspaceResult result;
    uint8_t primary;
    if (scene == NULL || context_out == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        scene->orientation_state.attack_direction > 1U) {
        return false;
    }
    primary = scene->live_foundation.primary_actor;
    if (primary >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[primary].active) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_INPUT_TAG;
    input.orientation_035a = scene->orientation_state.attack_direction;
    input.actor_position = scene->actors[primary].position;
    if (!tecmo_gameplay_cpu_opcode16_workspace_harness(&input, &result)) {
        return false;
    }
    memset(&context, 0, sizeof(context));
    context.contract_tag = TECMO_GAMEPLAY_SCENE_OPCODE16_FRAME_CONTEXT_TAG;
    context.available = true;
    context.primary_actor_0308 = primary;
    context.orientation_035a = input.orientation_035a;
    context.primary_position = input.actor_position;
    context.workspace_036e = result.workspace_036e;
    context.workspace_0370 = result.workspace_0370;
    *context_out = context;
    return true;
}

bool scene_cpu_opcode16_workspace_project(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveFoundation *foundation,
    const TecmoGameplaySceneOpcode16FrameContext *context,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    TecmoGameplayCpuOpcode16WorkspaceResult result;
    if (scene == NULL || foundation == NULL || context == NULL ||
        input == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG) {
        return false;
    }
    input->pointer_workspace_valid = false;
    input->workspace_036e = 0U;
    input->workspace_0370 = 0U;
    if (!context->available) return true;
    if (!scene_cpu_opcode16_context_result(context, &result) ||
        context->primary_actor_0308 != foundation->primary_actor ||
        context->orientation_035a !=
            scene->orientation_state.attack_direction) {
        return false;
    }
    input->pointer_workspace_valid = true;
    input->workspace_036e = result.workspace_036e;
    input->workspace_0370 = result.workspace_0370;
    return true;
}

static bool scene_cpu_build_play_input(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    const TecmoGameplayLiveFoundation *foundation,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (scene == NULL || actor_position == NULL || foundation == NULL ||
        input == NULL) {
        return false;
    }
    memset(input, 0, sizeof(*input));
    input->contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    /* Bank06 advances one bounded source record for the selected actor per
       live tick. The accepted executor's maximum budget is not a per-actor
       scene tick budget. */
    input->step_budget = 1U;
    input->orientation_035a = scene->orientation_state.attack_direction;
    /* Bank06 $9146 reads only $04B0 bit-$10. LIVE owns that selector flag
       through formation synchronization, so zero is a valid unselected
       value here rather than a substitute for missing RAM. */
    memcpy(input->actor_04b0, foundation->actor_selector_flags,
           sizeof(input->actor_04b0));
    /* Only the Bank06 $92CA low-two-bit gate has a typed ordinary-LIVE owner:
       every modeled transient lifecycle above is absent, so its exact zero
       branch may advance the five-byte stream. This does not represent all of
       $BA or derive a value from a zeroed struct, shot clock, or frame count. */
    input->common_tail_ba_available =
        scene_cpu_common_tail_has_ordinary_live_zero(scene);
    input->flags_ba = 0U;
    /* Fixed `$F07E-$F0B9` authors `$007E` bit 1 before Bank06 dispatch from
       the owned ordinary slot-10 state, orientation, and primary coordinate. */
    input->opcode21_gate_inputs_available = scene_cpu_opcode21_flags_007e(
        scene, actor_position, foundation, &input->flags_007e);
    input->state_058a = scene->state.shot_clock;
    input->state_0357 = scene->state.clock_minutes;
    input->state_0358 = scene->state.clock_seconds;
    /* Opcode 7 still has no faithful typed LIVE owner. */
    input->actor_046e_probe_available = false;
    memcpy(input->actor_position, actor_position,
           sizeof(input->actor_position));
    if (!scene_cpu_current_ball_snapshot(
            scene, actor_position, &input->ball_position)) {
        return false;
    }
    return scene_cpu_opcode10_selector_project(
        scene, actor_position, foundation, input);
}

static bool scene_cpu_a023_latch_context_valid(
    const TecmoGameplaySceneA023LatchFrameContext *context)
{
    const TecmoGameplayActorCommandAssignmentSameFrameLatch *latch;
    if (context == NULL ||
        context->contract_tag !=
            TECMO_GAMEPLAY_SCENE_A023_LATCH_FRAME_CONTEXT_TAG ||
        !context->available) {
        return false;
    }
    latch = &context->latch;
    return latch->contract_tag ==
               TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_TAG &&
        latch->valid && latch->target.depth <= UINT8_MAX &&
        latch->immediate_opcode20_actor_mask != 0U &&
        (latch->immediate_opcode20_actor_mask & ~0x03FFU) == 0U &&
        ((latch->producer_kind ==
              TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721 &&
          !latch->b783_bit20_clear_follows_assignment) ||
         (latch->producer_kind ==
              TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B783 &&
          latch->b783_bit20_clear_follows_assignment));
}

static void scene_cpu_selected_primary_opcode7_probe_begin(
    const TecmoGameplayScene *scene,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (input == NULL) return;
    input->actor_046e_probe_available = false;
    input->opcode6_context_available = false;
    input->opcode6_automatic = false;
    input->opcode23_context_available = false;
    input->opcode23_uncontrolled = false;
    memset(input->actor_046e_probe, 0, sizeof(input->actor_046e_probe));
    if (!scene_cpu_common_tail_has_ordinary_live_zero(scene)) return;
    /* Bank06's selected-primary prepass is still in the proven ordinary
       `$0478==0` seam. Canonical opcode-7 records index only slot 10, so this
       scoped capture represents exactly `$046E[$0A] == $0478 == 0`. */
    input->actor_046e_probe[0x0AU] = 0U;
    input->actor_046e_probe_available = true;
    input->opcode6_context_available = true;
    input->opcode6_automatic = true;
    input->opcode23_context_available = true;
    input->opcode23_uncontrolled = true;
}

static void scene_cpu_selected_primary_opcode7_probe_end(
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (input == NULL) return;
    input->actor_046e_probe_available = false;
    input->opcode6_context_available = false;
    input->opcode6_automatic = false;
    input->opcode23_context_available = false;
    input->opcode23_uncontrolled = false;
    memset(input->actor_046e_probe, 0, sizeof(input->actor_046e_probe));
}

static bool scene_cpu_shot_input(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *holder,
    const TecmoTeamDataPlayer *player,
    TecmoGameplayCpuSteeringShotInput *input)
{
    int32_t delta;
    if (scene == NULL || holder == NULL || player == NULL || input == NULL ||
        scene->orientation_state.offensive_hoop.x <
            TECMO_GAMEPLAY_COURT_WORLD_MIN_X ||
        scene->orientation_state.offensive_hoop.x >
            TECMO_GAMEPLAY_COURT_WORLD_MAX_X) {
        return false;
    }
    memset(input, 0, sizeof(*input));
    input->contract_tag = TECMO_GAMEPLAY_CPU_STEERING_SHOT_INPUT_TAG;
    /* State/gate zero is the supported neutral live-action mapping. The
       target delta is the exact scene holder-to-TGOR offensive hoop delta;
       timer_0798=1, timer_0760=shot clock, and random_byte=0 are deterministic
       native approximations because their original caller workspace is not
       proven. Bank02 $A8CC-$A8D0 proves that TTDT profile[4] supplies $0533. */
    input->state_0588 = 0U;
    input->flags_ba = 0U;
    delta = (int32_t)scene->orientation_state.offensive_hoop.x -
            holder->position.x;
    if (delta < 0) delta = -delta;
    if (delta > 0xFFFF) delta = 0xFFFF;
    input->target_delta_low = (uint8_t)delta;
    input->target_delta_high = (uint8_t)((uint32_t)delta >> 8U);
    input->gate_0478 = 0U;
    input->timer_0798 = 1U;
    input->difficulty = scene->launch.difficulty;
    input->timer_0760 = scene->state.shot_clock;
    input->rating_0533 = player->profile[4];
    input->random_byte = 0U;
    return true;
}

/* Direct callers that use the source/default-initializer unbound launch retain
   pre-R1 native approximation. This path is deliberately isolated behind the
   stored legacy bit; production launchers are canonical bound LIVE launches
   and never consume these formation targets or cadence. */
static bool scene_cpu_legacy_policy_target(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        snapshot[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
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
        !scene->legacy_direct_launch ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->state.possession > TECMO_GAMEPLAY_TEAM_HOME ||
        scene->orientation_state.attack_direction >=
            TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT) {
        return false;
    }
    item = &scene->actors[actor];
    linked_actor = scene->cpu_actors[actor].linked_actor;
    if (item->team > TECMO_GAMEPLAY_TEAM_HOME ||
        item->roster_index >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        linked_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        linked_actor == actor ||
        !tecmo_gameplay_court_coordinate_valid(&snapshot[linked_actor])) {
        return false;
    }
    if (item->team == (uint8_t)scene->state.possession) {
        const TecmoGameplayCourtCoordinate *formation =
            &formation_targets[item->roster_index];
        target_x = scene->orientation_state.attack_direction == 0U
            ? formation->x
            : TECMO_GAMEPLAY_COURT_WORLD_MAX_X - formation->x;
        target_y = formation->y;
    } else {
        const TecmoGameplayCourtCoordinate *linked = &snapshot[linked_actor];
        goal_side = scene->orientation_state.attack_direction == 0U
            ? -1 : 1;
        target_x = (int32_t)linked->x + goal_side * 32;
        target_y = (int32_t)linked->y +
            defender_depth_split[item->roster_index];
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

static bool scene_update_ai_legacy(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneCpuShotRequest *shot_request_out)
{
    TecmoGameplaySceneActor candidate_actors[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor candidate_cpu[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinate snapshot[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplayBallDribbleFrame candidate_dribble;
    TecmoGameplayScene candidate_scene;
    size_t actor;
    if (scene == NULL || shot_request_out == NULL ||
        !scene->legacy_direct_launch) {
        return false;
    }
    shot_request_out->requested = false;
    shot_request_out->actor_index = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    shot_request_out->playback_supported = false;
    shot_request_out->deferred = false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene->actors[actor].active ||
            !scene_actor_position_valid_for_scene(scene, actor) ||
            !scene_cpu_actor_state_valid(
                scene, actor, &scene->cpu_actors[actor])) {
            return false;
        }
        snapshot[actor] = scene->actors[actor].position;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    memcpy(candidate_cpu, scene->cpu_actors, sizeof(candidate_cpu));
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplayCpuSteeringMovementInput input;
        TecmoGameplayCpuSteeringMovementResult result;
        TecmoGameplaySceneCpuActor *cpu;
        const TecmoTeamDataPlayer *player;
        if (scene_actor_is_controlled(scene, actor) ||
            scene_actor_in_pretip_recovery(scene, actor) ||
            actor == scene->shot_actor ||
            (scene->live_foundation
                 .regulation_entry_clamp_exemption_active &&
             actor == scene->live_foundation
                 .regulation_entry_clamp_exempt_actor)) {
            /* The compatibility AI adapter must honor the same exact
               `$0588&$08` staging lifecycle as the source-stream path.
               Natural automatic opcode-5/pass staging leaves this holder
               stationary until catch; a legacy hoop target must not invent
               locomotion and immediately latch an out-of-bounds call. */
            continue;
        }
        player = scene_actor_player(scene, &scene->actors[actor]);
        cpu = &candidate_cpu[actor];
        if (player == NULL || cpu->decision_serial == UINT32_MAX) return false;
        memset(&input, 0, sizeof(input));
        input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
        input.steering.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
        memcpy(input.steering.actor_position, snapshot,
               sizeof(input.steering.actor_position));
        input.steering.actor = (uint8_t)actor;
        input.steering.possession = (uint8_t)scene->state.possession;
        input.steering.orientation =
            scene->orientation_state.attack_direction;
        input.steering.ball_holder = scene->ball_holder;
        input.steering.matchup_actor = cpu->linked_actor;
        input.steering.difficulty = scene->launch.difficulty;
        if (actor != scene->ball_holder &&
            !scene_cpu_legacy_policy_target(scene, snapshot, actor,
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
        input.primary_selected_actor = actor == scene->ball_holder;
        if (!tecmo_gameplay_cpu_steering_movement_step(
                &scene->cpu_steering_assets, &scene->movement_assets,
                &input, &result) ||
            !scene_actor_apply_movement(
                scene, candidate_actors, actor, &result.movement,
                result.held_direction_bits)) {
            return false;
        }
        if (!scene_actor_world_position_valid(&candidate_actors[actor])) {
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
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene_team_has_controller(scene, scene->state.possession) &&
        !candidate_actors[scene->ball_holder].movement_boundary_latched) {
        const TecmoGameplaySceneActor *holder =
            &candidate_actors[scene->ball_holder];
        const TecmoGameplaySceneCpuActor *cpu =
            &candidate_cpu[scene->ball_holder];
        int32_t target_dx = (int32_t)holder->position.x -
            cpu->target_position.x;
        int32_t target_dy = (int32_t)holder->position.y -
            cpu->target_position.y;
        uint32_t shot_cadence = 60U -
            (uint32_t)scene->launch.difficulty * 15U;
        if (target_dx < 0) target_dx = -target_dx;
        if (target_dy < 0) target_dy = -target_dy;
        if (!holder->movement_boundary_latched && cpu->target_valid &&
            cpu->target_kind ==
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
            target_dx <= 2 && target_dy <= 2 &&
            scene->frame % shot_cadence == 0U) {
            shot_request_out->requested = true;
            shot_request_out->actor_index = scene->ball_holder;
            shot_request_out->deferred = false;
        }
    }
    if (shot_request_out->requested) {
        candidate_scene = *scene;
        memcpy(candidate_scene.actors, candidate_actors,
               sizeof(candidate_actors));
        memcpy(candidate_scene.cpu_actors, candidate_cpu,
               sizeof(candidate_cpu));
        candidate_scene.ball_position = candidate_ball;
        if (scene_start_shot_actor(
                &candidate_scene, 0U, shot_request_out->actor_index)) {
            shot_request_out->playback_supported = true;
            shot_request_out->deferred = false;
            *scene = candidate_scene;
            return true;
        }
        shot_request_out->requested = false;
        shot_request_out->playback_supported = false;
        shot_request_out->deferred = true;
    }
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    memcpy(scene->cpu_actors, candidate_cpu, sizeof(candidate_cpu));
    scene->ball_position = candidate_ball;
    return true;
}

uint8_t scene_bank06_ordinary_actor_at(size_t source_index)
{
    /* Canonical Bank06 $8284 LDX #$09 / $82A4 DEX visits 9..0. Keep this
       small seam directly testable because opcode handlers share play state
       and therefore make actor traversal order behaviorally significant. */
    return source_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
        ? (uint8_t)(TECMO_GAMEPLAY_SCENE_ACTOR_COUNT - 1U - source_index)
        : TECMO_GAMEPLAY_SCENE_NO_ACTOR;
}

bool scene_update_ai(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneCpuShotRequest *shot_request_out)
{
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCourtCoordinate
        steering_snapshot[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation candidate_foundation;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        candidate_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplayBallDribbleFrame candidate_dribble;
    TecmoGameplayScene candidate_scene;
    TecmoGameplaySceneOpcode10FrameContext candidate_opcode10_context;
    TecmoGameplaySceneA023LatchFrameContext candidate_a023_context;
    TecmoGameplaySceneOpcode10Projection primary_opcode10_projection;
    uint8_t actor_team[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    bool selected_primary_stepped = false;
    bool selected_primary_route_owned = false;
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
    shot_request_out->playback_supported = false;
    shot_request_out->deferred = false;
    if (scene->legacy_direct_launch) {
        return scene_update_ai_legacy(scene, shot_request_out);
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene->actors[actor].active ||
            !scene_actor_position_valid_for_scene(scene, actor) ||
            !scene_cpu_actor_state_valid(
                scene, actor, &scene->cpu_actors[actor])) {
            return false;
        }
        steering_snapshot[actor] =
            scene->actors[actor].position;
        actor_team[actor] = scene->actors[actor].team;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    memcpy(candidate_cpu, scene->cpu_actors, sizeof(candidate_cpu));
    candidate_ball = scene->ball_position;
    candidate_foundation = scene->live_foundation;
    candidate_opcode10_context = scene->opcode10_frame_context;
    candidate_a023_context = scene->a023_latch_frame_context;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, steering_snapshot,
            scene->orientation_state.attack_direction,
            (uint8_t)scene->state.possession, scene->ball_holder,
            actor_team, scene->launch.controller_team,
            scene->controlled_actor, &candidate_foundation)) return false;
    if (!scene_cpu_build_play_input(
            scene, steering_snapshot, &candidate_foundation, &play_input))
        return false;
    /* Opcode 6 returns with selected action `$10` and its cursor retained.
       Fixed `$C711` consumes that marker on the following update before
    another Bank06 record can be fetched, preserving the source delay. */
    actor = candidate_foundation.primary_actor;
    if (actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene_selected_primary_automatic_dispatch_owned(
            scene, &candidate_foundation, (uint8_t)actor) &&
        candidate_foundation.play_state.actor_state[actor] == 0x04U &&
        candidate_foundation.play_state.action_state_046e[actor] == 0x10U) {
        candidate_scene = *scene;
        candidate_scene.live_foundation = candidate_foundation;
        candidate_scene.opcode10_frame_context = candidate_opcode10_context;
        if (!scene_begin_cpu_pass_from_action10(
                &candidate_scene, (uint8_t)actor) ||
            !scene_ownership_valid(&candidate_scene)) {
            return false;
        }
        *scene = candidate_scene;
        return true;
    }
    /* Bank05 produced this workspace once before any source movement. Bind it
       once to the immutable play input; selected-primary and ordinary 9..0
       dispatch must not recompute it from post-move coordinates. */
    if (!scene_cpu_opcode16_workspace_project(
            scene, &candidate_foundation, &scene->opcode16_frame_context,
            &play_input)) return false;

    /* Canonical selected-primary order precedes $8284's descending ordinary
       actor loop: $827E JSR $935D, $8281 JSR $8374, then automatic offense in
       the supported ordinary $05A1=0 context reaches $83F3 JSR $8491. State
       4 dispatches through $8B90 exactly once. Typed controller ownership is
       the native admission boundary; raw $030C is not mirrored. State 6 is
       the exact independent wait/countdown lifecycle (including the
       alternate catch stream $007D); $9053-$905D performs a wrapping byte
       decrement once per update, returns to state 4 only when that decrement
       produces zero, and never fetches another record on a state-6 tick. Other
       selected-primary states/gates remain inert/fail-closed. */
    actor = candidate_foundation.primary_actor;
    if (!scene_cpu_opcode10_workspace_project(
            (uint8_t)actor, &candidate_foundation,
            &candidate_opcode10_context, &play_input,
            &primary_opcode10_projection)) return false;
    if (scene_selected_primary_automatic_dispatch_owned(
            scene, &candidate_foundation, (uint8_t)actor) &&
        candidate_foundation.play_state.actor_state[actor] == 0x05U) {
        if (!scene_cpu_route_step(
                scene, actor, (uint8_t)(scene->state.clock_divider & 1U),
                &candidate_foundation, candidate_actors,
                &candidate_cpu[actor])) {
            return false;
        }
        selected_primary_route_owned = true;
    } else if (scene_selected_primary_automatic_dispatch_owned(
            scene, &candidate_foundation, (uint8_t)actor) &&
        candidate_foundation.play_state.actor_state[actor] == 0x06U) {
        TecmoGameplayCpuSteeringPlayResult primary_result;
        bool primary_step_ok;
        memset(&primary_result, 0, sizeof(primary_result));
        play_input.actor = (uint8_t)actor;
        scene_cpu_opcode12_context_begin(
            &candidate_foundation, (uint8_t)actor, &play_input);
        scene_cpu_selected_primary_opcode7_probe_begin(scene, &play_input);
        primary_step_ok = tecmo_gameplay_live_foundation_play_step(
            &scene->cpu_steering_assets, &play_input,
            &candidate_foundation, &primary_result);
        scene_cpu_selected_primary_opcode7_probe_end(&play_input);
        scene_cpu_opcode12_context_end(&play_input);
        if (!primary_step_ok ||
            !scene_cpu_opcode10_projection_commit(
                &primary_opcode10_projection, &primary_result,
                &candidate_opcode10_context.timer_0798) ||
            primary_result.fetched || primary_result.advanced ||
            primary_result.next_offset !=
                candidate_foundation.play_state.stream_offset[actor]) {
            return false;
        }
        selected_primary_stepped = true;
    } else if (scene_selected_primary_automatic_dispatch_owned(
            scene, &candidate_foundation, (uint8_t)actor) &&
        candidate_foundation.play_state.actor_state[actor] == 0x04U) {
        TecmoGameplayCpuSteeringPlayResult primary_result;
        const TecmoTeamDataPlayer *primary_player =
            scene_actor_player(scene, &scene->actors[actor]);
        bool route_owned = false;
        bool primary_step_ok;
        memset(&primary_result, 0, sizeof(primary_result));
        play_input.actor = (uint8_t)actor;
        scene_cpu_opcode12_context_begin(
            &candidate_foundation, (uint8_t)actor, &play_input);
        scene_cpu_selected_primary_opcode7_probe_begin(scene, &play_input);
        primary_step_ok = primary_player != NULL &&
            tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &primary_result);
        scene_cpu_selected_primary_opcode7_probe_end(&play_input);
        scene_cpu_opcode12_context_end(&play_input);
        if (!primary_step_ok ||
            !scene_cpu_opcode10_projection_commit(
                &primary_opcode10_projection, &primary_result,
                &candidate_opcode10_context.timer_0798)) return false;
        selected_primary_stepped = true;
        if (candidate_foundation.play_state.action_state_046e[actor] ==
                0x21U) {
            candidate_scene = *scene;
            candidate_scene.live_foundation = candidate_foundation;
            candidate_scene.opcode10_frame_context =
                candidate_opcode10_context;
            if (!scene_begin_cpu_pass_from_action21(
                    &candidate_scene, (uint8_t)actor) ||
                !scene_ownership_valid(&candidate_scene)) {
                return false;
            }
            *scene = candidate_scene;
            return true;
        }
        /* Bank06 opcode 9 may leave the selected holder in actor state 0
           with action $17. That pair is not an idle/reset endpoint: Bank05
           $81F2-$822F dispatches action index $17 through $8351/$8378 to
           $8A6D, which restores the selected registers and jumps to the
           shot initializer at $8ACE. The pointer dispatch is exact; launch
           admission remains a bounded native adapter because $8ACE consumes
           unowned $0478/$0499/$007E. Stage the typed automatic-shot seam on
           the complete candidate so either close or jump playback commits
           atomically without manufacturing a human controller. */
        if (candidate_foundation.play_state.actor_state[actor] == 0U &&
            candidate_foundation.play_state.action_state_046e[actor] ==
                0x17U) {
            bool shot_started;
            candidate_foundation.last_shot_request = true;
            candidate_foundation.last_shot_actor = (uint8_t)actor;
            candidate_foundation.last_shot_deferred = false;
            candidate_foundation.last_shot_playback_supported = false;
            candidate_scene = *scene;
            candidate_scene.live_foundation = candidate_foundation;
            candidate_scene.opcode10_frame_context =
                candidate_opcode10_context;
            shot_started = scene_start_automatic_cpu_shot_actor(
                &candidate_scene, (uint8_t)actor);
            if (shot_started && candidate_scene.shot_actor == actor) {
                candidate_scene.live_foundation.last_shot_request = true;
                candidate_scene.live_foundation.last_shot_actor =
                    (uint8_t)actor;
                candidate_scene.live_foundation.last_shot_deferred = false;
                candidate_scene.live_foundation
                    .last_shot_playback_supported = true;
                if (!scene_ownership_valid(&candidate_scene)) return false;
                *scene = candidate_scene;
                shot_request_out->requested = true;
                shot_request_out->actor_index = (uint8_t)actor;
                shot_request_out->playback_supported = true;
                shot_request_out->deferred = false;
                return true;
            }
            /* A malformed/unavailable shot dependency still fails closed.
               Recover state 4 only after the complete automatic candidate is
               rejected; this remains a justified native adapter rather than
               a claim about the unowned $8ACE admission gates. */
            candidate_foundation.play_state.actor_state[actor] = 0x04U;
            candidate_foundation.play_state.action_state_046e[actor] = 0U;
            candidate_foundation.last_shot_deferred = true;
            candidate_scene = *scene;
            candidate_scene.live_foundation = candidate_foundation;
            candidate_scene.opcode10_frame_context =
                candidate_opcode10_context;
            if (!scene_ownership_valid(&candidate_scene)) return false;
            *scene = candidate_scene;
            shot_request_out->requested = false;
            shot_request_out->actor_index = (uint8_t)actor;
            shot_request_out->playback_supported = false;
            shot_request_out->deferred = true;
            return true;
        }
        if (primary_result.fetched && primary_result.command.opcode == 4U) {
            if (!scene_cpu_route_launch(
                    scene, &primary_result, actor, primary_player,
                    &candidate_foundation, &candidate_cpu[actor],
                    &route_owned)) {
                return false;
            }
            selected_primary_route_owned = route_owned;
        }
    }

    /* Opcode 6 may change `$0478` during the descending traversal. The
       selected-primary zero capture must never escape into ordinary actors. */
    scene_cpu_selected_primary_opcode7_probe_end(&play_input);

    /* Bank06 $8284 loads X=$09 and $82A4 decrements through actor 0. Ordinary
       non-selected decisions consume one immutable post-human-input court
       snapshot in that exact 9..0 order. Each accepted play step is staged in
       candidate_foundation; no actor, CPU metadata, or ball state reaches the
       scene until every requested validation succeeds. */
    for (size_t source_index = 0U;
         source_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++source_index) {
        TecmoGameplayCpuSteeringPlayResult play_result;
        TecmoGameplaySceneOpcode10Projection opcode10_projection;
        TecmoGameplayCpuSteeringMovementInput input;
        TecmoGameplayCpuSteeringMovementResult result;
        TecmoGameplaySceneCpuActor *cpu;
        const TecmoTeamDataPlayer *player;
        TecmoGameplayCourtCoordinate target = {0, 0};
        TecmoGameplayCpuSteeringHarnessTargetKind target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
        bool source_target;
        bool source_direction;
        bool source_direction_target = false;
        bool movement_target = false;
        bool selected_defender;
        actor = scene_bank06_ordinary_actor_at(source_index);
        if (scene_actor_is_controlled(scene, actor) ||
            scene_actor_in_pretip_recovery(scene, actor) ||
            actor == scene->shot_actor) {
            continue;
        }
        player = scene_actor_player(scene, &scene->actors[actor]);
        cpu = &candidate_cpu[actor];
        if (player == NULL || cpu->decision_serial == UINT32_MAX) {
            return false;
        }
        memset(&input, 0, sizeof(input));
        play_input.actor = (uint8_t)actor;
        if (!scene_cpu_opcode10_workspace_project(
                (uint8_t)actor, &candidate_foundation,
                &candidate_opcode10_context, &play_input,
                &opcode10_projection)) return false;
        memset(&play_result, 0, sizeof(play_result));
        selected_defender =
            candidate_foundation.selected_defender_handoff_active &&
            actor == candidate_foundation.defender_actor;
        /* After selected-primary dispatch above, Bank06 $8286/$8289 skips
           $0308 and $828B/$828E skips $0309 in the ordinary loop. A non-route
           primary effect may still compose through TGMO below. Opcode 4/state
           5 is already owned above and must neither fetch twice nor double
           move through TGMO. */
        if (actor == candidate_foundation.primary_actor) {
            if (selected_primary_route_owned) {
                if (!scene_cpu_actor_state_valid(scene, actor, cpu)) {
                    return false;
                }
                continue;
            } else if (selected_primary_stepped) {
                /* Continue with source metadata and movement composition. */
            } else {
                cpu->decision_serial = 0U;
                cpu->snapshot_fingerprint = 0U;
                cpu->target_position.x = 0;
                cpu->target_position.y = 0;
                cpu->target_kind =
                    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT;
                cpu->direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
                cpu->held_direction_bits =
                    TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
                cpu->target_valid = false;
                cpu->writes_direction = false;
                cpu->command_offset =
                    TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
                cpu->linked_actor = candidate_foundation.play_state
                    .fixed_link[actor];
                if (!scene_cpu_actor_state_valid(scene, actor, cpu)) {
                    return false;
                }
                continue;
            }
        }
        /* Bank06 $828B/$828E excludes $0309 from ordinary dispatch. If an
           actor becomes the selected defender while carrying an older
           offense route, the bounded selected-defender adapter takes
           exclusive ownership and discards that stale state-5 continuation.
           This reset is a native safety policy until Bank05's complete
           selected-defender state transition is converted. */
        if (selected_defender &&
            (candidate_foundation.play_state.actor_state[actor] == 0x05U ||
             candidate_foundation.play_state.route_motion[actor].active)) {
            memset(&candidate_foundation.play_state.route_motion[actor], 0,
                   sizeof(candidate_foundation.play_state
                              .route_motion[actor]));
            candidate_foundation.play_state.route_motion[actor].contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
            candidate_foundation.play_state.actor_state[actor] = 0x04U;
            candidate_foundation.play_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            candidate_foundation.play_state.target_x[actor] = 0;
            candidate_foundation.play_state.target_depth[actor] = 0;
            candidate_foundation.play_state.direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            candidate_foundation.source_target_valid[actor] = false;
            candidate_foundation.source_raw_target_valid[actor] = false;
            candidate_foundation.source_inactive_target_storage[actor] =
                false;
            candidate_foundation.source_direction_valid[actor] = false;
            candidate_foundation.source_direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            candidate_foundation.deferred[actor] = false;
            candidate_foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        if (!selected_defender &&
            candidate_foundation.play_state.actor_state[actor] == 0x05U) {
            if (!scene_cpu_route_step(
                    scene, actor,
                    (uint8_t)(scene->state.clock_divider & 1U),
                    &candidate_foundation, candidate_actors, cpu)) return false;
            continue;
        }
        /* The selected-defender setup is also outside ordinary dispatch while
           Bank05 $9B27 owns its on-ball responsibility. */
        scene_cpu_opcode12_context_begin(
            &candidate_foundation, (uint8_t)actor, &play_input);
        if (actor != candidate_foundation.primary_actor &&
            !selected_defender) {
            bool play_step_ok;
            /* `$B721`/`$B783` stores precede the same loop's Bank06
               traversal. Only TGCA's immediate selector-$10 assignment at
               `$0019` can consume this ephemeral latch. The selected-primary
               `$000A` wait route and every opcode-13 record remain unavailable. */
            play_input.global_target_available =
                candidate_foundation.play_state.stream_offset[actor] ==
                    0x0019U &&
                (candidate_a023_context.latch
                     .immediate_opcode20_actor_mask &
                 (uint16_t)(1U << actor)) != 0U &&
                scene_cpu_a023_latch_context_valid(&candidate_a023_context);
            if (play_input.global_target_available) {
                play_input.global_target = candidate_a023_context.latch.target;
            }
            play_step_ok = tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &play_result);
            play_input.global_target_available = false;
            memset(&play_input.global_target, 0,
                   sizeof(play_input.global_target));
            if (!play_step_ok) return false;
        }
        scene_cpu_opcode12_context_end(&play_input);
        if (!scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &play_result,
                &candidate_opcode10_context.timer_0798)) return false;
        if (play_result.fetched && play_result.command.opcode == 4U) {
            bool route_owned = false;
            if (!scene_cpu_route_launch(
                    scene, &play_result, actor, player,
                    &candidate_foundation, cpu, &route_owned) ||
                !route_owned) return false;
            continue;
        }
        source_target = candidate_foundation.source_target_valid[actor];
        source_direction = candidate_foundation.source_direction_valid[actor];
        input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
        input.steering.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
        memcpy(input.steering.actor_position, steering_snapshot,
               sizeof(input.steering.actor_position));
        input.steering.actor = (uint8_t)actor;
        input.steering.possession = (uint8_t)scene->state.possession;
        input.steering.orientation =
            scene->orientation_state.attack_direction;
        input.steering.ball_holder = scene->ball_holder;
        input.steering.difficulty = scene->launch.difficulty;
        /* Native facing adapter; `$06CB` itself is the fixed pairing in
           foundation->dynamic_link, while `$037F/$07DF` stays separate. */
        input.steering.matchup_actor = candidate_foundation.play_state
            .fixed_link_target[actor];
        if (selected_defender) {
            /* Bank06 omits $0309 from ordinary command dispatch; the
               selected-defender setup at Bank05 $9B27 owns its on-ball
               responsibility until released by the next handoff. */
            target = steering_snapshot[scene->ball_holder];
            /* $90DC/$90DE is the exact orientation-selected signed
               horizontal adjustment (+16/-16). It supplies separation and
               prevents the old exact-coordinate overlap policy. */
            target.x = (int16_t)(target.x +
                (scene->orientation_state.attack_direction == 0U
                    ? 16 : -16));
            if (!tecmo_gameplay_court_coordinate_valid(&target)) {
                return false;
            }
            target_kind =
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR;
            input.steering.matchup_actor = scene->ball_holder;
            source_target = false;
            source_direction = false;
            movement_target = true;
        } else if (source_target) {
            if (!scene_cpu_source_target(
                    &candidate_foundation.play_state, steering_snapshot,
                    &play_input.ball_position,
                    actor, &target, &target_kind)) return false;
            if (source_direction) {
                uint8_t direction;
                if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                        &scene->cpu_steering_assets,
                        (int16_t)(target.x - steering_snapshot[actor].x),
                        (int16_t)(target.y - steering_snapshot[actor].y),
                        &direction) ||
                    direction != candidate_foundation.source_direction[actor])
                    return false;
            }
        } else if (source_direction) {
            if (scene_cpu_target_for_source_direction(
                    &scene->cpu_steering_assets, &steering_snapshot[actor],
                    candidate_foundation.source_direction[actor], &target)) {
                source_direction_target = true;
            } else {
                /* An outward source direction at an edge/corner has no
                   legal in-court TGMO target. Preserve the accepted source
                   direction and classify only its owned TGMO composition as
                   inert/deferred; the LIVE scene transaction remains valid. */
                candidate_foundation.deferred[actor] = true;
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NATIVE_TARGET_OUTSIDE_COURT;
            }
        }
        /* A deferred source effect preserves its last validated target. If
           none exists, LIVE deliberately performs no movement this tick;
           there is no fallback to the retired approximate formation policy. */
        if (!candidate_foundation.selected_defender_handoff_active ||
            actor != candidate_foundation.defender_actor) {
            movement_target = source_target || source_direction_target;
        }
        /* `$85EA`'s selected-primary staging flag survives the automatic
           opcode-5/wait/opcode-23/opcode-6 chain; natural source observation
           retains the exact table coordinate through action `$10/$13`.
           Do not compose the opcode-5 facing direction into adapter TGMO
           locomotion while that exact clamp-exemption lifecycle is active. */
        if (candidate_foundation.regulation_entry_clamp_exemption_active &&
            actor == candidate_foundation
                .regulation_entry_clamp_exempt_actor) {
            movement_target = false;
        }
        input.steering.has_explicit_target = movement_target;
        if (movement_target) input.steering.explicit_target = target;
        if (!scene_actor_movement_state(
            scene, &scene->actors[actor], &input.movement)) return false;
        input.player_movement_rating = player->profile[0];
        input.condition = scene->actors[actor].condition;
        input.speed_value = scene->launch.speed_value;
        input.global_object_state = 0U;
        input.movement_flags =
            candidate_foundation.regulation_entry_clamp_exemption_active &&
            actor == candidate_foundation
                .regulation_entry_clamp_exempt_actor ? 0x08U : 0U;
        input.primary_selected_actor = actor == scene->ball_holder;
        if (!movement_target) {
            /* A deferred/no-target source step is intentionally inert. Clear
               all prior CPU target metadata so a target computed under the
               old role/orientation cannot remain scene-visible; fixed links
               and the validated foundation stream still advance separately. */
            cpu->decision_serial = 0U;
            cpu->snapshot_fingerprint = 0U;
            cpu->target_position.x = 0;
            cpu->target_position.y = 0;
            cpu->target_kind =
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT;
            cpu->direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            cpu->held_direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            cpu->target_valid = false;
            cpu->writes_direction = false;
            cpu->command_offset = TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
            cpu->linked_actor = candidate_foundation.play_state
                .fixed_link[actor];
            if (!scene_cpu_actor_state_valid(scene, actor, cpu)) {
                return false;
            }
            continue;
        }
        if (!tecmo_gameplay_cpu_steering_movement_step(
            &scene->cpu_steering_assets, &scene->movement_assets,
            &input, &result) ||
            result.steering.target_position.x != target.x ||
            result.steering.target_position.y != target.y ||
            result.steering.matchup_actor != input.steering.matchup_actor ||
            !scene_actor_apply_movement(
                scene, candidate_actors, actor, &result.movement,
                result.held_direction_bits)) return false;
        if (!scene_actor_world_position_valid(&candidate_actors[actor])) {
            TecmoGameplayScene position_candidate = *scene;
            position_candidate.actors[actor] = candidate_actors[actor];
            position_candidate.live_foundation = candidate_foundation;
            if (!scene_actor_position_valid_for_scene(
                    &position_candidate, actor)) {
                return false;
            }
        } else if (
            candidate_foundation.regulation_entry_clamp_exemption_active &&
            actor == candidate_foundation
                .regulation_entry_clamp_exempt_actor) {
            candidate_actors[actor].anchor = candidate_actors[actor].position;
            candidate_foundation.regulation_entry_clamp_exemption_active =
                false;
            candidate_foundation.regulation_entry_clamp_exempt_actor =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        }
        if (result.steering.writes_direction) {
            uint8_t expected_direction;
            int16_t horizontal = (int16_t)(
                target.x - steering_snapshot[actor].x);
            int16_t depth = (int16_t)(
                target.y - steering_snapshot[actor].y);
            if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                    &scene->cpu_steering_assets, horizontal, depth,
                    &expected_direction) ||
                result.steering.direction != expected_direction ||
                result.held_direction_bits >=
                    sizeof(scene->movement_assets.direction_map) ||
                scene->movement_assets.direction_map[
                    result.held_direction_bits] != expected_direction) {
                return false;
            }
            /* For the current target-write family this equality proves the
               accepted source target and TGMO octant remain identical. A
               source direction write is checked against the actual held-bit
               map below and may also supply a deterministic adapter target. */
        }
        if (source_direction &&
            (result.steering.direction !=
                 candidate_foundation.source_direction[actor] ||
             result.held_direction_bits >=
                 sizeof(scene->movement_assets.direction_map) ||
             scene->movement_assets.direction_map[
                 result.held_direction_bits] !=
                 candidate_foundation.source_direction[actor])) return false;
        if (cpu->decision_serial == UINT32_MAX) return false;
        ++cpu->decision_serial;
        cpu->snapshot_fingerprint = result.steering.input_fingerprint;
        cpu->target_position = result.steering.target_position;
        cpu->target_kind = (uint8_t)target_kind;
        cpu->direction = result.steering.direction;
        cpu->held_direction_bits = result.held_direction_bits;
        cpu->target_valid = true;
        cpu->writes_direction = result.steering.writes_direction;
        cpu->command_offset = TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
        cpu->linked_actor = candidate_foundation.play_state.fixed_link[actor];
        if (!scene_cpu_actor_state_valid(scene, actor, cpu)) return false;
    }

    if (!scene_live_ball_frame_for_actors(
            scene, candidate_actors, scene->ball_holder,
            &candidate_dribble) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &candidate_dribble.visible_position, &candidate_ball)) return false;
    candidate_foundation.last_shot_request = false;
    candidate_foundation.last_shot_actor =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.last_shot_deferred = false;
    candidate_foundation.last_shot_playback_supported = false;

    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene_team_has_controller(scene, scene->state.possession) &&
        !candidate_foundation.score_restart_selection_active &&
        !candidate_actors[scene->ball_holder].movement_boundary_latched) {
        TecmoGameplaySceneActor *holder =
            &candidate_actors[scene->ball_holder];
        const TecmoTeamDataPlayer *player = scene_actor_player(
            scene, holder);
        TecmoGameplayCpuSteeringShotInput shot_input;
        TecmoGameplayCpuSteeringShotResult shot_result;
        if (player == NULL ||
            !scene_cpu_shot_input(scene, holder, player, &shot_input) ||
            !tecmo_gameplay_live_foundation_shot_request(
                &scene->cpu_steering_assets, &shot_input, scene->ball_holder,
                &candidate_foundation, &shot_result)) {
            return false;
        }
        if (shot_result.request) {
            candidate_foundation.last_shot_request = true;
            candidate_foundation.last_shot_actor = scene->ball_holder;
            candidate_scene = *scene;
            memcpy(candidate_scene.actors, candidate_actors,
                   sizeof(candidate_actors));
            memcpy(candidate_scene.cpu_actors, candidate_cpu,
                   sizeof(candidate_cpu));
            candidate_scene.ball_position = candidate_ball;
            candidate_scene.live_foundation = candidate_foundation;
            candidate_scene.opcode10_frame_context =
                candidate_opcode10_context;
            /* shots.c remains the sole playback owner. Probe it once on the
               complete candidate; an unsupported request is recorded as a
               nonfatal deferred/non-launch classification. */
             if (scene_start_shot_actor(
                     &candidate_scene, 0U, scene->ball_holder) &&
                 scene_shot_is_close(candidate_scene.shot_kind) &&
                 candidate_scene.shot_actor == scene->ball_holder) {
                candidate_scene.live_foundation.last_shot_deferred = false;
                candidate_scene.live_foundation.last_shot_playback_supported =
                    true;
                if (!scene_ownership_valid(&candidate_scene)) return false;
                *scene = candidate_scene;
                shot_request_out->requested = true;
                shot_request_out->actor_index = scene->shot_actor;
                shot_request_out->playback_supported = true;
                shot_request_out->deferred = false;
                return true;
            }
            /* A source predicate may describe a jump/far request that the
               native shots.c entry can superficially start, but the later
               playback path requires controller-team state that autonomous
               CPU sides do not own. Only the bounded close-shot profile is
               therefore exposed as supported; discard every other shallow
               candidate and retain an explicit non-launch classification. */
            candidate_foundation.last_shot_request = true;
            candidate_foundation.last_shot_actor = scene->ball_holder;
            candidate_foundation.last_shot_deferred = true;
            candidate_foundation.last_shot_playback_supported = false;
            shot_request_out->deferred = true;
            shot_request_out->actor_index = scene->ball_holder;
        }
    }
    /* All scene-visible mutations, including shot-decision metadata, commit
       only after the complete candidate transaction succeeds. */
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    memcpy(scene->cpu_actors, candidate_cpu, sizeof(candidate_cpu));
    scene->ball_position = candidate_ball;
    scene->live_foundation = candidate_foundation;
    scene->opcode10_frame_context = candidate_opcode10_context;
    candidate_a023_context.available = false;
    scene->a023_latch_frame_context = candidate_a023_context;
    return true;
}

static bool scene_cpu_build_shot_play_input(
    const TecmoGameplayScene *scene,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    const TecmoGameplayLiveFoundation *foundation,
    TecmoGameplayCpuSteeringPlayInput *input)
{
    if (scene == NULL || actor_position == NULL || foundation == NULL ||
        input == NULL || scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    memset(input, 0, sizeof(*input));
    input->contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    input->step_budget = 1U;
    input->orientation_035a = scene->orientation_state.attack_direction;
    memcpy(input->actor_04b0, foundation->actor_selector_flags,
           sizeof(input->actor_04b0));
    /* Bank05 owns nonzero BA low bits during shot/recovery. Commands that
       require the ordinary clear branch must therefore defer; this is not a
       fabricated zero-value possession context. */
    input->common_tail_ba_available = false;
    input->flags_ba = 0U;
    input->opcode21_gate_inputs_available = false;
    input->actor_046e_probe_available = false;
    memcpy(input->actor_position, actor_position,
           sizeof(input->actor_position));
    if (!tecmo_gameplay_court_coordinate_q8_floor(
            &scene->ball_position, &input->ball_position)) {
        return false;
    }
    input->special_actor_07df_available = false;
    input->special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_actor_branch_context_available = false;
    input->linked_actor_resolved_valid = false;
    input->linked_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input->linked_relative_valid = false;
    return true;
}

bool scene_apply_a9da_landing_assignment(TecmoGameplayScene *scene)
{
    TecmoGameplayCpuGlobalLatch latch;
    TecmoGameplayCpuA9daInput input;
    TecmoGameplayCpuA9daInput output;
    TecmoGameplayCpuA9daResult result;
    TecmoGameplayLiveFoundation foundation;
    size_t actor;
    if (scene == NULL || scene->legacy_direct_launch ||
        scene->jump_rim_rattle_debug ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->predicted_make_route || !scene->shot_rim_rattle_selected ||
        !scene->shot_a0f3_raw_position_valid ||
        !scene->shot_a8e9_normalized_valid ||
        scene->shot_a8e9_normalized.contract_tag !=
            TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_RESULT_TAG ||
        scene->live_foundation.primary_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.defender_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.primary_actor ==
            scene->live_foundation.defender_actor ||
        scene->shot_a9da_assignment_valid ||
        scene->shot_a9da_opcode13_pending ||
        !scene->shot_global_latch_initialized ||
        !scene->shot_global_ba_low2_known_zero ||
        !scene->shot_global_latch.valid ||
        scene->shot_global_latch.producer !=
            TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A790) {
        return false;
    }
    memset(&latch, 0, sizeof(latch));
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    memset(&result, 0, sizeof(result));
    latch = scene->shot_global_latch;
    input.contract_tag = TECMO_GAMEPLAY_CPU_A9DA_INPUT_TAG;
    input.expected_latch_serial = latch.write_serial;
    input.ball_x = scene->shot_a0f3_raw_x;
    input.ball_raw_depth_8 = scene->shot_a0f3_raw_depth;
    input.normalized_object10_vx_a9da =
        (int16_t)scene->shot_a8e9_normalized.raw_vx_04f1_04fc;
    input.normalized_object10_vz_a9da =
        (int16_t)scene->shot_a8e9_normalized.raw_vz_0507_0512;
    input.multiplier_002c = 0x002CU;
    input.orientation_035a = scene->jump_rim_rattle.orientation;
    input.primary_0308 = scene->live_foundation.primary_actor;
    input.defender_0309 = scene->live_foundation.defender_actor;
    /* This exact frame-89 branch follows the rattle terminal transition that
       admits `$A9DA->$AAB8`; its successful source trace owns BA-low2 clear
       and `$05A1==0` only for this transaction. */
    input.ba = 0U;
    input.value_05a1 = 0U;
    input.global_0587 = 0U;
    input.global_0588 = 0U;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene_actor_position_valid_for_scene(scene, actor) ||
            scene->actors[actor].position.y < 0 ||
            scene->actors[actor].position.y > UINT8_MAX) {
            return false;
        }
        input.actor_x[actor] =
            (uint16_t)scene->actors[actor].position.x;
        input.actor_raw_depth_8[actor] =
            (uint8_t)scene->actors[actor].position.y;
        input.fixed_link_06cb[actor] =
            scene->live_foundation.play_state.fixed_link[actor];
        input.stream_offset[actor] =
            scene->live_foundation.play_state.stream_offset[actor];
        input.last_step_offset[actor] =
            scene->live_foundation.last_step_offset[actor];
        input.state[actor] =
            scene->live_foundation.play_state.actor_state[actor];
        input.action_state_046e[actor] =
            scene->live_foundation.play_state.action_state_046e[actor];
        input.action_0458[actor] =
            scene->live_foundation.play_state.action[actor];
    }
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.outcome != TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ASSIGNED ||
        !result.latch_overwritten || !result.same_loop_first_002d ||
        result.latch_serial != input.expected_latch_serial + 1U ||
        result.chosen_actor_002d >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        result.chosen_actor_002d == input.primary_0308 ||
        result.chosen_actor_002d == input.defender_0309 ||
        latch.producer != TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA ||
        !latch.valid || latch.write_serial != result.latch_serial) {
        return false;
    }
    foundation = scene->live_foundation;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        foundation.play_state.stream_offset[actor] =
            output.stream_offset[actor];
        foundation.last_step_offset[actor] = output.last_step_offset[actor];
        foundation.play_state.actor_state[actor] = output.state[actor];
        foundation.play_state.action_state_046e[actor] =
            output.action_state_046e[actor];
        foundation.play_state.action[actor] = output.action_0458[actor];
        if (input.state[actor] == 0x05U && output.state[actor] == 0x04U) {
            /* `$A993` replaces the source actor state directly. Q6 route
               motion and typed target provenance are C-only expansions of
               state 5, so they must retire in the same transaction; leaving
               them active would manufacture a state pair the NES cannot
               represent. */
            memset(&foundation.play_state.route_motion[actor], 0,
                   sizeof(foundation.play_state.route_motion[actor]));
            foundation.play_state.route_motion[actor].contract_tag =
                TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
            foundation.play_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            foundation.play_state.target_x[actor] = 0;
            foundation.play_state.target_depth[actor] = 0;
            foundation.play_state.direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            foundation.source_target_valid[actor] = false;
            foundation.source_raw_target_valid[actor] = false;
            foundation.source_inactive_target_storage[actor] = false;
            foundation.source_direction_valid[actor] = false;
            foundation.source_direction[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            foundation.deferred[actor] = false;
            foundation.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
    }
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &foundation)) {
        return false;
    }
    scene->live_foundation = foundation;
    scene->shot_global_latch = latch;
    scene->shot_a9da_latch = latch;
    scene->shot_a9da_input = input;
    scene->shot_a9da_output = output;
    scene->shot_a9da_result = result;
    scene->shot_a9da_assignment_valid = true;
    scene->shot_a9da_opcode13_pending = true;
    return true;
}

bool scene_update_shot_cpu_offball(TecmoGameplayScene *scene)
{
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCourtCoordinate
        steering_snapshot[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation candidate_foundation;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        candidate_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneOpcode10FrameContext candidate_opcode10_context;
    TecmoGameplaySceneA023LatchFrameContext candidate_a023_context;
    uint8_t actor_direction_0463[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    bool a9da_consumed = false;
    size_t source_index;
    if (scene == NULL || scene->legacy_direct_launch ||
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->cpu_steering_assets.available ||
        !scene->movement_assets.available ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation) ||
        scene->live_foundation.primary_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.defender_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->launch.difficulty >=
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT) {
        return false;
    }
    for (source_index = 0U;
         source_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++source_index) {
        if (!scene->actors[source_index].active ||
            !scene_actor_position_valid_for_scene(scene, source_index) ||
            !scene_cpu_actor_state_valid(
                scene, source_index, &scene->cpu_actors[source_index])) {
            return false;
        }
        steering_snapshot[source_index] =
            scene->actors[source_index].position;
        actor_direction_0463[source_index] =
            scene->actors[source_index].movement_direction;
        if (actor_direction_0463[source_index] >=
                TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
            return false;
        }
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    memcpy(candidate_cpu, scene->cpu_actors, sizeof(candidate_cpu));
    candidate_foundation = scene->live_foundation;
    candidate_opcode10_context = scene->opcode10_frame_context;
    candidate_a023_context = scene->a023_latch_frame_context;
    if (!scene_cpu_build_shot_play_input(
            scene, steering_snapshot, &candidate_foundation, &play_input) ||
        !scene_cpu_opcode16_workspace_project(
            scene, &candidate_foundation, &scene->opcode16_frame_context,
            &play_input)) {
        return false;
    }

    /* Fixed Bank06 order remains 9..0. Bank05 separately owns the selected
       primary and defender during a shot, so this phase runs only the
       ordinary actors and never executes pass/shot/switch/defense policy. */
    for (source_index = 0U;
         source_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++source_index) {
        TecmoGameplayCpuSteeringPlayResult play_result;
        TecmoGameplaySceneOpcode10Projection opcode10_projection;
        TecmoGameplayCpuSteeringMovementInput input;
        TecmoGameplayCpuSteeringMovementResult result;
        TecmoGameplaySceneCpuActor *cpu;
        const TecmoTeamDataPlayer *player;
        TecmoGameplayCourtCoordinate target = {0, 0};
        TecmoGameplayCpuSteeringHarnessTargetKind target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
        uint8_t actor = scene_bank06_ordinary_actor_at(source_index);
        bool source_target;
        bool source_direction;
        bool source_direction_target = false;
        bool movement_target;
        bool global_authorized;
        bool a9da_authorized;
        bool a023_authorized;
        if (candidate_foundation.play_state.actor_state[actor] == 0x07U) {
            TecmoGameplayCpuOpcode15State7Result state7_result;
            memset(&state7_result, 0, sizeof(state7_result));
            if (!tecmo_gameplay_live_foundation_opcode15_state7_step(
                    actor, &candidate_foundation, &state7_result) ||
                state7_result.contract_tag !=
                    TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_RESULT_TAG ||
                !tecmo_gameplay_live_foundation_valid(
                    &scene->cpu_steering_assets, &candidate_foundation)) {
                return false;
            }
            continue;
        }
        if (scene_actor_is_controlled(scene, actor) ||
            scene_actor_in_pretip_recovery(scene, actor) ||
            actor == scene->shot_actor ||
            actor == candidate_foundation.primary_actor ||
            actor == candidate_foundation.defender_actor) {
            continue;
        }
        player = scene_actor_player(scene, &scene->actors[actor]);
        cpu = &candidate_cpu[actor];
        if (player == NULL || cpu->decision_serial == UINT32_MAX) {
            return false;
        }
        memset(&input, 0, sizeof(input));
        memset(&play_result, 0, sizeof(play_result));
        play_input.actor = actor;
        if (!scene_cpu_opcode10_workspace_project(
                actor, &candidate_foundation, &candidate_opcode10_context,
                &play_input, &opcode10_projection)) {
            return false;
        }
        if (candidate_foundation.play_state.actor_state[actor] == 0x05U) {
            if (!scene_cpu_route_step(
                    scene, actor,
                    (uint8_t)(scene->state.clock_divider & 1U),
                    &candidate_foundation, candidate_actors, cpu)) {
                return false;
            }
            continue;
        }
        {
            TecmoGameplayCpuSteeringCommand command;
            uint16_t stream =
                candidate_foundation.play_state.stream_offset[actor];
            if (!tecmo_gameplay_cpu_steering_decode_command(
                    &scene->cpu_steering_assets, stream, &command)) {
                return false;
            }
            if (command.opcode == 15U &&
                candidate_foundation.control_mode[
                    candidate_foundation.actor_team[actor]] != 0U) {
                TecmoGameplayCpuSteeringOpcode15RawResult opcode15_result;
                uint8_t raw_0499 =
                    (uint8_t)(scene->jump_ball_altitude_q8 >> 8U);
                memset(&opcode15_result, 0, sizeof(opcode15_result));
                if (!tecmo_gameplay_live_foundation_opcode15_step_automatic(
                        &scene->cpu_steering_assets, actor, raw_0499,
                        actor_direction_0463, &candidate_foundation,
                        &opcode15_result) ||
                    opcode15_result.contract_tag !=
                        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_RESULT_TAG) {
                    return false;
                }
                /* Every source branch returns without advancing this record.
                   A committed role swap changes lifecycle state; a gate/noop
                   simply retries on the next eligible Bank06 traversal. */
                continue;
            }
        }
        scene_cpu_opcode12_context_begin(
            &candidate_foundation, actor, &play_input);
        global_authorized = scene->shot_global_latch_initialized &&
            scene->shot_global_ba_low2_known_zero &&
            scene->shot_global_latch.valid &&
            (scene->shot_global_latch.producer ==
                 TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A790 ||
             scene->shot_global_latch.producer ==
                 TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA);
        a9da_authorized = scene->shot_a9da_assignment_valid &&
            scene->shot_a9da_opcode13_pending &&
            actor == scene->shot_a9da_result.chosen_actor_002d &&
            candidate_foundation.play_state.stream_offset[actor] == 0x002DU &&
            scene->shot_a9da_result.latch_serial ==
                scene->shot_a9da_latch.write_serial &&
            scene->shot_a9da_latch.valid &&
            scene->shot_a9da_latch.producer ==
                TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA;
        a023_authorized =
            candidate_foundation.play_state.stream_offset[actor] == 0x0019U &&
            (candidate_a023_context.latch.immediate_opcode20_actor_mask &
             (uint16_t)(1U << actor)) != 0U &&
            scene_cpu_a023_latch_context_valid(&candidate_a023_context);
        play_input.global_target_available =
            global_authorized || a023_authorized;
        if (a023_authorized) {
            play_input.global_target = candidate_a023_context.latch.target;
        } else if (global_authorized) {
            play_input.global_target.x =
                scene->shot_global_latch.raw_x_038d_038e;
            play_input.global_target.depth =
                scene->shot_global_latch.raw_depth_038f_0390;
            play_input.common_tail_ba_available = true;
            play_input.flags_ba = 0U;
        }
        if (!tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &play_result)) {
            scene_cpu_opcode12_context_end(&play_input);
            return false;
        }
        play_input.global_target_available = false;
        memset(&play_input.global_target, 0,
               sizeof(play_input.global_target));
        play_input.common_tail_ba_available = false;
        play_input.flags_ba = 0U;
        scene_cpu_opcode12_context_end(&play_input);
        if (global_authorized && play_result.fetched &&
            play_result.command.opcode == 13U) {
            if (play_result.deferred || !play_result.advanced ||
                !play_result.raw_target_valid ||
                play_result.raw_target_x !=
                    scene->shot_global_latch.raw_x_038d_038e ||
                play_result.raw_target_depth !=
                    scene->shot_global_latch.raw_depth_038f_0390) {
                return false;
            }
        }
        if (a023_authorized &&
            (!play_result.fetched || play_result.command.opcode != 20U ||
             play_result.deferred || !play_result.advanced ||
             !play_result.raw_target_valid ||
             play_result.raw_target_x !=
                 candidate_a023_context.latch.target.x ||
             play_result.raw_target_depth !=
                 candidate_a023_context.latch.target.depth)) {
            return false;
        }
        if (a9da_authorized) {
            if (!play_result.fetched || play_result.command.opcode != 13U ||
                play_result.deferred || !play_result.advanced ||
                play_result.next_offset != 0x0032U ||
                !play_result.raw_target_valid ||
                play_result.raw_target_x !=
                    scene->shot_global_latch.raw_x_038d_038e ||
                play_result.raw_target_depth !=
                    scene->shot_global_latch.raw_depth_038f_0390) {
                return false;
            }
            a9da_consumed = true;
        }
        if (!scene_cpu_opcode10_projection_commit(
                &opcode10_projection, &play_result,
                &candidate_opcode10_context.timer_0798)) {
            return false;
        }
        if (play_result.fetched && play_result.command.opcode == 4U) {
            bool route_owned = false;
            if (!scene_cpu_route_launch(
                    scene, &play_result, actor, player,
                    &candidate_foundation, cpu, &route_owned) ||
                !route_owned) {
                return false;
            }
            continue;
        }
        source_target = candidate_foundation.source_target_valid[actor];
        source_direction =
            candidate_foundation.source_direction_valid[actor];
        if (source_target) {
            if (!scene_cpu_source_target(
                    &candidate_foundation.play_state, steering_snapshot,
                    &play_input.ball_position, actor, &target,
                    &target_kind)) {
                return false;
            }
            if (source_direction) {
                uint8_t direction;
                if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                        &scene->cpu_steering_assets,
                        (int16_t)(target.x - steering_snapshot[actor].x),
                        (int16_t)(target.y - steering_snapshot[actor].y),
                        &direction) ||
                    direction !=
                        candidate_foundation.source_direction[actor]) {
                    return false;
                }
            }
        } else if (source_direction) {
            if (scene_cpu_target_for_source_direction(
                    &scene->cpu_steering_assets,
                    &steering_snapshot[actor],
                    candidate_foundation.source_direction[actor], &target)) {
                source_direction_target = true;
            } else {
                candidate_foundation.deferred[actor] = true;
                candidate_foundation.deferred_reason[actor] =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NATIVE_TARGET_OUTSIDE_COURT;
            }
        }
        movement_target = source_target || source_direction_target;
        if (candidate_foundation.regulation_entry_clamp_exemption_active &&
            actor == candidate_foundation
                .regulation_entry_clamp_exempt_actor) {
            movement_target = false;
        }
        if (!movement_target) {
            cpu->decision_serial = 0U;
            cpu->snapshot_fingerprint = 0U;
            memset(&cpu->target_position, 0, sizeof(cpu->target_position));
            cpu->target_kind =
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT;
            cpu->direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
            cpu->held_direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
            cpu->target_valid = false;
            cpu->writes_direction = false;
            cpu->command_offset = TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
            cpu->linked_actor =
                candidate_foundation.play_state.fixed_link[actor];
            if (!scene_cpu_actor_state_valid(scene, actor, cpu)) return false;
            continue;
        }
        input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
        input.steering.contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
        memcpy(input.steering.actor_position, steering_snapshot,
               sizeof(input.steering.actor_position));
        input.steering.actor = actor;
        input.steering.possession = (uint8_t)scene->state.possession;
        input.steering.orientation =
            scene->orientation_state.attack_direction;
        input.steering.ball_holder = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        input.steering.matchup_actor =
            candidate_foundation.play_state.fixed_link_target[actor];
        input.steering.difficulty = scene->launch.difficulty;
        input.steering.has_explicit_target = true;
        input.steering.explicit_target = target;
        if (!scene_actor_movement_state(
                scene, &scene->actors[actor], &input.movement)) {
            return false;
        }
        input.player_movement_rating = player->profile[0];
        input.condition = scene->actors[actor].condition;
        input.speed_value = scene->launch.speed_value;
        input.global_object_state = 0U;
        input.movement_flags = 0U;
        input.primary_selected_actor = false;
        if (!tecmo_gameplay_cpu_steering_movement_step(
                &scene->cpu_steering_assets, &scene->movement_assets,
                &input, &result) ||
            result.steering.ball_holder !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
            result.steering.target_position.x != target.x ||
            result.steering.target_position.y != target.y ||
            !scene_actor_apply_movement(
                scene, candidate_actors, actor, &result.movement,
                result.held_direction_bits) ||
            !scene_actor_world_position_valid(&candidate_actors[actor])) {
            return false;
        }
        if (source_direction &&
            (result.steering.direction !=
                 candidate_foundation.source_direction[actor] ||
             result.held_direction_bits >=
                 sizeof(scene->movement_assets.direction_map) ||
             scene->movement_assets.direction_map[
                 result.held_direction_bits] !=
                 candidate_foundation.source_direction[actor])) {
            return false;
        }
        if (cpu->decision_serial == UINT32_MAX) return false;
        ++cpu->decision_serial;
        cpu->snapshot_fingerprint = result.steering.input_fingerprint;
        cpu->target_position = result.steering.target_position;
        cpu->target_kind = (uint8_t)target_kind;
        cpu->direction = result.steering.direction;
        cpu->held_direction_bits = result.held_direction_bits;
        cpu->target_valid = true;
        cpu->writes_direction = result.steering.writes_direction;
        cpu->command_offset = TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
        cpu->linked_actor = candidate_foundation.play_state.fixed_link[actor];
        if (!scene_cpu_actor_state_valid(scene, actor, cpu)) return false;
    }
    if (scene->shot_a9da_opcode13_pending) {
        uint8_t chosen = scene->shot_a9da_result.chosen_actor_002d;
        if (!a9da_consumed && chosen < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
            !scene_actor_is_controlled(scene, chosen) &&
            chosen != scene->shot_actor &&
            chosen != scene->live_foundation.primary_actor &&
            chosen != scene->live_foundation.defender_actor) {
            return false;
        }
    }
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    memcpy(scene->cpu_actors, candidate_cpu, sizeof(candidate_cpu));
    scene->live_foundation = candidate_foundation;
    scene->opcode10_frame_context = candidate_opcode10_context;
    scene->shot_a9da_opcode13_pending = false;
    candidate_a023_context.available = false;
    scene->a023_latch_frame_context = candidate_a023_context;
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
