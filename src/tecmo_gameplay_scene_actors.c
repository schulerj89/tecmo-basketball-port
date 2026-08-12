#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_gameplay_candidate_selection.h"
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
    TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t actor_team[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation candidate;
    size_t actor;
    if (scene == NULL || scene->state.possession > TECMO_GAMEPLAY_TEAM_HOME) {
        return false;
    }
    /* shots.c deliberately clears the slot holder during playback. Preserve
       the last validated LIVE binding until its existing slot-based handoff
       restores a holder; no dynamic matchup is fabricated mid-shot. */
    if (scene->ball_holder == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
        return scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene->actors[actor].active ||
            !scene_actor_world_position_valid(&scene->actors[actor])) {
            return false;
        }
        positions[actor] = scene->actors[actor].position;
        actor_team[actor] = scene->actors[actor].team;
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

#define TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES 8U
#define TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES 24U

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
               !pass->receiver_locked &&
               pass->reserved[0U] == 0U && pass->reserved[1U] == 0U &&
               pass->flight_frame == 0U && pass->flight_duration == 0U &&
               pass->start_position.x_q8 == 0 &&
               pass->start_position.y_q8 == 0 &&
               pass->target_position.x_q8 == 0 &&
               pass->target_position.y_q8 == 0;
    }
    if (pass->phase >= TECMO_GAMEPLAY_SCENE_PASS_PHASE_COUNT ||
        pass->passer >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        pass->receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        pass->passer == pass->receiver ||
        (pass->controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT &&
         pass->controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR) ||
        scene->actors[pass->passer].team != scene->state.possession ||
        scene->actors[pass->receiver].team != scene->state.possession ||
        scene->ball_holder != pass->passer ||
        pass->flight_duration < TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES ||
        pass->flight_duration > TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES ||
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
    } else if (scene->controlled_actor[pass->controller] != pass->passer ||
               scene->launch.controller_team[pass->controller] !=
                   scene->state.possession) {
        return false;
    }
    if (pass->phase == TECMO_GAMEPLAY_SCENE_PASS_GATHER) {
        return !pass->receiver_locked &&
               (pass->packed_animation_state == 0x32U ||
                pass->packed_animation_state == 0x22U ||
                pass->packed_animation_state == 0x12U ||
                pass->packed_animation_state == 0x02U ||
                pass->packed_animation_state == 0x03U);
    }
    if (pass->phase != TECMO_GAMEPLAY_SCENE_PASS_FLIGHT ||
        pass->packed_animation_state != 0x04U || !pass->receiver_locked) {
        return false;
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
    int32_t dx;
    int32_t dy;
    uint32_t distance;
    uint16_t duration;
    if (scene == NULL || scene_pass_active(scene) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        passer != scene->ball_holder ||
        receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        receiver == passer ||
        scene->actors[receiver].team != scene->state.possession ||
        !scene_attached_ball_position(&scene->actors[passer], &start) ||
        !scene_attached_ball_position(&scene->actors[receiver], &target)) {
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
    dx = target.x_q8 - start.x_q8;
    dy = target.y_q8 - start.y_q8;
    distance = (uint32_t)((dx < 0 ? -dx : dx) +
                          (dy < 0 ? -dy : dy)) / 256U;
    /* The source duration comes from $B42F's $BB9F/$BBA0 lookup. That table
       is not yet a strict pass asset, so only this duration mapping is a
       native adapter; release/flight/catch ownership follows the ROM order. */
    duration = (uint16_t)(TECMO_GAMEPLAY_PASS_MIN_FLIGHT_UPDATES +
                          distance / 12U);
    if (duration > TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES)
        duration = TECMO_GAMEPLAY_PASS_MAX_FLIGHT_UPDATES;
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
    pass.flight_duration = duration;
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
    /* Canonical Rev1 Bank06 $8FC5-$8FE7 copies C9 into $046E[X] at
       $8FCA/$8FCC and returns at $8FE7; rewind begins at $8FE8. Capture proves
       C9=$21 was written to actor 9, not that actor 9 was current $0308.
       Bank06 $8284-$82A5 excludes selected primary from ordinary dispatch,
       so this bounded downstream consumer admits only explicit already-
       retained $21 state. No live producer/retention/adoption lifecycle is
       claimed, no Bank04 cursor is forced, and CPU intent never uses NES A. */
    return scene_begin_actor_pass(
        scene, passer, receiver, TECMO_GAMEPLAY_SCENE_NO_ACTOR);
}

bool scene_update_pass(TecmoGameplayScene *scene)
{
    TecmoGameplayScene candidate;
    TecmoGameplayScenePassState *pass;
    bool receiver_facing_right;
    uint32_t step;
    if (scene == NULL || !scene_pass_active(scene) ||
        !scene_pass_state_valid(scene)) return false;
    candidate = *scene;
    pass = &candidate.pass_state;
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
            pass->receiver_locked = true;
            pass->phase = TECMO_GAMEPLAY_SCENE_PASS_FLIGHT;
            pass->flight_frame = 0U;
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
    if (pass->flight_frame < pass->flight_duration)
        ++pass->flight_frame;
    step = pass->flight_frame;
    /* Bank05 $B1E7 schedules five $B500->$BD6E fixed-point substeps. Its
       $B42F/$BB9F/$BBA0 trajectory asset is not imported yet, so this shared
       human/CPU interpolation remains a deliberately local native adapter;
       gather, launch lock, holder lifetime, and catch ordering are source-
       proven and do not depend on pretending these coordinates are exact. */
    candidate.ball_position.x_q8 = pass->start_position.x_q8 + (int32_t)(
        ((int64_t)(pass->target_position.x_q8 - pass->start_position.x_q8) *
         step) / pass->flight_duration);
    candidate.ball_position.y_q8 = pass->start_position.y_q8 + (int32_t)(
        ((int64_t)(pass->target_position.y_q8 - pass->start_position.y_q8) *
         step) / pass->flight_duration);
    if (pass->flight_frame < pass->flight_duration) {
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
    if (!scene_handoff_possession(scene, restart_team, passer) ||
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
        scene->live_foundation.primary_actor != passer ||
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

bool scene_update_inbound(TecmoGameplayScene *scene)
{
    TecmoGameplaySceneInboundState inbound;
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
    /* Reuse only the typed Bank05 $B24F catch transaction after the locked
       inbound flight reaches its `$037F[$030A]`-shaped candidate. */
    if (!scene->legacy_direct_launch &&
        !tecmo_gameplay_live_foundation_pass_handoff(
            &scene->cpu_steering_assets, inbound.receiver,
            &scene->live_foundation)) {
        return false;
    }
    if (!scene_goal_facing_right_for_team(
            scene, (TecmoGameplayTeam)inbound.restart_team,
            &receiver_facing_right)) {
        return false;
    }
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

static bool scene_cpu_common_tail_has_ordinary_live_zero(
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
    /* The remaining Bank06 inputs have no faithful typed LIVE owner. Keep
       their availability false rather than inventing $046E, $07DF, or the
       opcode-21 gate plane. */
    input->actor_046e_probe_available = false;
    input->opcode21_gate_inputs_available = false;
    input->special_actor_07df_available = false;
    input->special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    memcpy(input->actor_position, actor_position,
           sizeof(input->actor_position));
    if (!tecmo_gameplay_court_coordinate_q8_floor(
            &scene->ball_position, &input->ball_position)) {
        return false;
    }
    return true;
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
       proven. TTDT profile[0] supplies the source-backed rating byte. */
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
    input->rating_0533 = player->profile[0];
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
            !scene_actor_world_position_valid(&scene->actors[actor]) ||
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
            actor == scene->shot_actor) {
            continue;
        }
        player = scene_actor_player(scene, &scene->actors[actor]);
        cpu = &candidate_cpu[actor];
        if (player == NULL || cpu->decision_serial == UINT32_MAX) {
            return false;
        }
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
    uint8_t actor_team[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    static const uint8_t source_actor_order[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
        0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U
    };
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
            !scene_actor_world_position_valid(&scene->actors[actor]) ||
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
    if (!tecmo_gameplay_live_foundation_synchronize(
            &scene->cpu_steering_assets, steering_snapshot,
            scene->orientation_state.attack_direction,
            (uint8_t)scene->state.possession, scene->ball_holder,
            actor_team, scene->launch.controller_team,
            scene->controlled_actor, &candidate_foundation)) {
        return false;
    }
    /* Bank06 $8284-$82A5 excludes selected primary ($0308) before ordinary
       $057C->$8B90 dispatch. The native producer/retention/adoption lifecycle
       is unowned; this fail-closed seam can consume only explicit already-
       retained $21 state and must never manufacture it via a primary record. */
    if (candidate_foundation.primary_actor == scene->ball_holder &&
        candidate_foundation.play_state.action_state_046e[
            candidate_foundation.primary_actor] == 0x21U &&
        !scene_team_has_controller(scene, scene->state.possession)) {
        candidate_scene = *scene;
        candidate_scene.live_foundation = candidate_foundation;
        if (!scene_begin_cpu_pass_from_action21(
                &candidate_scene, candidate_foundation.primary_actor) ||
            !scene_ownership_valid(&candidate_scene)) {
            return false;
        }
        *scene = candidate_scene;
        return true;
    }
    if (!scene_cpu_build_play_input(
            scene, steering_snapshot, &candidate_foundation, &play_input)) {
        return false;
    }

    /* Ordinary non-selected decisions consume one immutable post-human-input
       court snapshot. Each accepted play step is staged in
       candidate_foundation; no actor, CPU metadata, or ball state reaches the
       scene until every requested validation succeeds. */
    for (size_t source_index = 0U;
         source_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++source_index) {
        TecmoGameplayCpuSteeringPlayResult play_result;
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
        actor = source_actor_order[source_index];
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
        memset(&play_result, 0, sizeof(play_result));
        /* Bank06 $8286/$8289 skips $0308 and $828B/$828E skips $0309 before
           the ordinary $057C indirect dispatch. Keep the selected primary
           inert here whether or not it carries retained action $21; its
           Bank05 selected-primary dispatcher is a separate owner. */
        if (actor == candidate_foundation.primary_actor) {
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
            if (!scene_cpu_actor_state_valid(scene, actor, cpu)) return false;
            continue;
        }
        /* The selected-defender setup is also outside ordinary dispatch while
           Bank05 $9B27 owns its on-ball responsibility. */
        if (!(candidate_foundation.selected_defender_handoff_active &&
              actor == candidate_foundation.defender_actor) &&
            !tecmo_gameplay_live_foundation_play_step(
                &scene->cpu_steering_assets, &play_input,
                &candidate_foundation, &play_result)) {
            return false;
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
        /* Native fixed projection; never expose it as live $037F/$06CB. */
        input.steering.matchup_actor = candidate_foundation.play_state
            .fixed_link_target[actor];
        if (candidate_foundation.selected_defender_handoff_active &&
            actor == candidate_foundation.defender_actor) {
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
                    actor, &target, &target_kind)) {
                return false;
            }
            if (source_direction) {
                uint8_t direction;
                if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                        &scene->cpu_steering_assets,
                        (int16_t)(target.x - steering_snapshot[actor].x),
                        (int16_t)(target.y - steering_snapshot[actor].y),
                        &direction) ||
                    direction != candidate_foundation.source_direction[actor]) {
                    return false;
                }
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
        input.steering.has_explicit_target = movement_target;
        if (movement_target) input.steering.explicit_target = target;
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
                result.held_direction_bits) ||
            !scene_actor_world_position_valid(&candidate_actors[actor])) {
            return false;
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

    if (!scene_live_ball_frame_for_actors(
            scene, candidate_actors, scene->ball_holder,
            &candidate_dribble) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &candidate_dribble.visible_position, &candidate_ball)) {
        return false;
    }
    candidate_foundation.last_shot_request = false;
    candidate_foundation.last_shot_actor =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate_foundation.last_shot_deferred = false;
    candidate_foundation.last_shot_playback_supported = false;

    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene_team_has_controller(scene, scene->state.possession) &&
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
