#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"

#include <string.h>

/* Camera-coherent court snapshots and the invariants that bind a free-throw
   lineup to that same court projection.  This is a lower layer shared by the
   scene orchestrator and renderer; it never calls back into either one. */

static bool scene_camera_states_equal(
    const TecmoGameplayCameraState *left,
    const TecmoGameplayCameraState *right)
{
    return left != NULL && right != NULL &&
           left->camera_x == right->camera_x &&
           left->scroll_x == right->scroll_x &&
           left->scroll_aux == right->scroll_aux &&
           left->nametable_page == right->nametable_page &&
           left->aux == right->aux &&
           left->stream_direction == right->stream_direction &&
           left->layout_cursor == right->layout_cursor &&
           left->left_threshold == right->left_threshold &&
           left->right_threshold == right->right_threshold &&
           left->thresholds_valid == right->thresholds_valid &&
           left->endpoint_latched == right->endpoint_latched;
}

static bool scene_pretip_projection_altitude(
    const TecmoGameplayScene *scene,
    size_t actor_index,
    uint8_t *altitude_out)
{
    size_t jumper;
    if (scene == NULL || altitude_out == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    *altitude_out = 0U;
    if (!scene->pretip_jump_active) return true;
    if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.phase_frame >=
            TECMO_GAMEPLAY_PRETIP_JUMP_DURATION) {
        return false;
    }
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        uint8_t jumper_actor = scene->pretip_jumper_actor[jumper];
        const TecmoGameplaySceneActor *actor;
        if (jumper_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            (jumper == 0U && jumper_actor >=
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
            (jumper == 1U && jumper_actor <
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
            (jumper > 0U && jumper_actor ==
                scene->pretip_jumper_actor[0U]) ||
            scene->pretip_jumper_altitude_q8[jumper] >
                TECMO_GAMEPLAY_PRETIP_JUMP_MAX_ALTITUDE_Q8) {
            return false;
        }
        actor = &scene->actors[jumper_actor];
        if (actor->position.x != actor->anchor.x ||
            actor->position.y != actor->anchor.y) {
            return false;
        }
        if (jumper_actor == actor_index) {
            *altitude_out = (uint8_t)(
                scene->pretip_jumper_altitude_q8[jumper] /
                TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE);
        }
    }
    return true;
}

bool scene_court_controller_team_valid(uint8_t team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY ||
           team == TECMO_GAMEPLAY_TEAM_HOME ||
           team == TECMO_GAMEPLAY_SCENE_NO_TEAM;
}

bool scene_court_free_throw_lineup_matches(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayFreeThrowLineup lineup;
    TecmoGameplayCourtCoordinateQ8 expected_ball;
    TecmoGameplayCourtCoordinateQ8 focus;
    TecmoGameplayCameraState settled;
    size_t controller;
    size_t actor;
    uint16_t expected_camera_x;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene->active ||
        !scene->free_throw_lineup_active ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        (scene->state.free_throws.scoring_team !=
             TECMO_GAMEPLAY_TEAM_AWAY &&
         scene->state.free_throws.scoring_team !=
             TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene->free_throw_lineup_assets.available ||
        scene->free_throw_lineup_orientation >=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT ||
        scene->free_throw_lineup_orientation !=
            scene->orientation_state.current_direction ||
        scene->free_throw_lineup_transition_serial !=
            scene->orientation_state.transition_serial ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.free_throws.scoring_team ||
        scene->state.possession != scene->state.free_throws.scoring_team ||
        scene->free_throw_shooter >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->free_throw_secondary >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->free_throw_shooter == scene->free_throw_secondary ||
        scene->actors[scene->free_throw_shooter].team !=
            (uint8_t)scene->state.free_throws.scoring_team ||
        scene->actors[scene->free_throw_secondary].team !=
            (uint8_t)scene_other_team(
                scene->state.free_throws.scoring_team) ||
        scene->ball_holder != scene->free_throw_shooter ||
        scene->actors[scene->free_throw_shooter].facing_right !=
            (scene->free_throw_lineup_orientation != 0U) ||
        !tecmo_gameplay_free_throw_lineup_derive(
            &scene->free_throw_lineup_assets,
            scene->free_throw_lineup_orientation,
            scene->free_throw_shooter,
            scene->free_throw_secondary, &lineup)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        const TecmoGameplayFreeThrowLineupActor *source =
            &lineup.actors[actor];
        if (!source->position_defined ||
            !scene->actors[actor].active ||
            scene->actors[actor].team !=
                (uint8_t)(actor <
                                  TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                              ? TECMO_GAMEPLAY_TEAM_AWAY
                              : TECMO_GAMEPLAY_TEAM_HOME) ||
            source->raw_world_x >
                (uint16_t)TECMO_GAMEPLAY_COURT_WORLD_MAX_X ||
            source->raw_world_y >
                (uint8_t)TECMO_GAMEPLAY_COURT_WORLD_MAX_Y ||
            scene->actors[actor].position.x !=
                (int16_t)source->raw_world_x ||
            scene->actors[actor].position.y !=
                (int16_t)source->raw_world_y ||
            scene->actors[actor].anchor.x !=
                scene->actors[actor].position.x ||
            scene->actors[actor].anchor.y !=
                scene->actors[actor].position.y ||
            !scene_actor_world_position_valid(&scene->actors[actor])) {
            return false;
        }
    }
    if (!scene_attached_ball_position(
            &scene->actors[scene->free_throw_shooter],
            &expected_ball) ||
        scene->ball_position.x_q8 != expected_ball.x_q8 ||
        scene->ball_position.y_q8 != expected_ball.y_q8 ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &scene->actors[scene->free_throw_shooter].position,
            &focus)) {
        return false;
    }
    expected_camera_x =
        scene->free_throw_lineup_orientation == 0U
            ? TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X
            : TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X;
    settled = scene->camera_state;
    if (scene->camera_state.camera_x != expected_camera_x ||
        !tecmo_gameplay_camera_settle_court(
            &scene->camera_assets, &settled, &focus,
            scene->free_throw_lineup_orientation, false) ||
        !scene_camera_states_equal(&settled, &scene->camera_state)) {
        return false;
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        uint8_t team = scene->launch.controller_team[controller];
        if (!scene_court_controller_team_valid(team)) {
            return false;
        }
        if (team == (uint8_t)scene->state.free_throws.scoring_team &&
            scene->controlled_actor[controller] !=
                scene->free_throw_shooter) {
            return false;
        }
        if (team != TECMO_GAMEPLAY_SCENE_NO_TEAM &&
            team != (uint8_t)scene->state.free_throws.scoring_team &&
            scene->controlled_actor[controller] !=
                scene->free_throw_secondary) {
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_scene_court_coordinates(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtCoordinates *coordinates_out)
{
    TecmoGameplaySceneCourtCoordinates coordinates;
    size_t actor;
    uint8_t direction;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene->active ||
        coordinates_out == NULL ||
        !tecmo_gameplay_court_coordinate_q8_valid(
            &scene->ball_position) ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        (scene->state.phase ==
                 TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE
             ? !scene_court_free_throw_lineup_matches(scene)
             : scene->free_throw_lineup_active) ||
        (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
         (!tecmo_gameplay_court_coordinate_q8_valid(
              &scene->shot_start_position) ||
          !tecmo_gameplay_court_coordinate_q8_valid(
              &scene->shot_end_position)))) {
        return false;
    }
    memset(&coordinates, 0, sizeof(coordinates));
    coordinates.contract_tag =
        TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        if (!tecmo_gameplay_court_coordinate_valid(
                &scene->actors[actor].position) ||
            !tecmo_gameplay_court_coordinate_valid(
                &scene->actors[actor].anchor)) {
            return false;
        }
        coordinates.players[actor] = scene->actors[actor].position;
    }
    coordinates.ball = scene->ball_position;
    for (direction = 0U;
         direction < TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT;
         ++direction) {
        if (!tecmo_gameplay_court_orientation_hoop(
                &scene->court_orientation, direction,
                &coordinates.hoops[direction])) {
            return false;
        }
    }
    *coordinates_out = coordinates;
    return true;
}

bool tecmo_gameplay_scene_court_projection(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtProjection *projection_out)
{
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtProjection projection;
    size_t actor;
    if (projection_out == NULL ||
        !tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state)) {
        return false;
    }
    memset(&projection, 0, sizeof(projection));
    projection.contract_tag =
        TECMO_GAMEPLAY_SCENE_COURT_PROJECTION_TAG;
    projection.camera_x = scene->camera_state.camera_x;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        uint8_t altitude = 0U;
        if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
            scene->shot_actor == actor) {
            altitude =
                (uint8_t)(scene->jump_actor_altitude_q8 /
                          TECMO_GAMEPLAY_COURT_COORDINATE_Q8_SCALE);
        } else if (!scene_pretip_projection_altitude(
                       scene, actor, &altitude)) {
            return false;
        }
        if (!tecmo_gameplay_camera_project_court(
                &scene->camera_assets, &scene->camera_state,
                &coordinates.players[actor], altitude,
                &projection.players[actor])) {
            return false;
        }
    }
    if (!tecmo_gameplay_camera_project_court_q8(
            &scene->camera_assets, &scene->camera_state,
            &coordinates.ball, 0U, &projection.ball)) {
        return false;
    }
    *projection_out = projection;
    return true;
}

bool tecmo_gameplay_scene_court_slice(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtSlice *slice_out)
{
    TecmoGameplaySceneCourtSlice slice;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || slice_out == NULL ||
        scene->state.possession >= TECMO_GAMEPLAY_TEAM_COUNT ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state) ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.possession) {
        return false;
    }
    memset(&slice, 0, sizeof(slice));
    slice.contract_tag = TECMO_GAMEPLAY_SCENE_COURT_SLICE_TAG;
    slice.transition_serial = scene->orientation_state.transition_serial;
    slice.possession = (uint8_t)scene->state.possession;
    slice.direction = scene->orientation_state.current_direction;
    if (!tecmo_gameplay_court_slice_viewport(
            &scene->court_world, scene->camera_state.camera_x,
            &slice.viewport) ||
        slice.viewport.camera_x != scene->camera_state.camera_x) {
        return false;
    }
    *slice_out = slice;
    return true;
}

bool tecmo_gameplay_scene_court_frame(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtFrame *frame_out)
{
    TecmoGameplaySceneCourtFrame frame;
    if (frame_out == NULL) return false;
    memset(&frame, 0, sizeof(frame));
    if (!tecmo_gameplay_scene_court_slice(scene, &frame.slice) ||
        !tecmo_gameplay_scene_court_projection(
            scene, &frame.projection) ||
        frame.slice.viewport.camera_x != frame.projection.camera_x) {
        return false;
    }
    frame.contract_tag = TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG;
    frame.scene_frame = scene->frame;
    frame.camera_follow_count = scene->camera_follow_count;
    *frame_out = frame;
    return true;
}
