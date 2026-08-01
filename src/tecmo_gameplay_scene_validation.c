#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"

/* Fail-closed cross-module scene invariants.  This lower layer is shared by
   orchestration, court snapshots, and production rendering. */

bool scene_ownership_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplayMovementState movement;
    size_t actor;
    size_t controller;
    if (scene == NULL ||
        !scene->camera_assets.available ||
        !scene->movement_assets.available ||
        !scene->ball_dribble_assets.available ||
        !scene->cpu_steering_assets.available ||
        !scene->penalty_assets.available ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state) ||
        !tecmo_gameplay_fatigue_state_valid(
            &scene->fatigue_assets, &scene->fatigue_state) ||
        scene->court_world.contract_tag !=
            TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG ||
        scene->court_world.tiles_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
        scene->court_world.palette_indices_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32 ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state) ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.possession ||
        (scene->state.phase ==
                 TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE
             ? !scene_court_free_throw_lineup_matches(scene)
             : scene->free_throw_lineup_active) ||
        !tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!scene_cpu_actor_state_valid(
                scene, actor, &scene->cpu_actors[actor]) ||
            (scene->actors[actor].active &&
             (!scene_actor_world_position_valid(&scene->actors[actor]) ||
              !scene_actor_coordinate_valid(
                  &scene->actors[actor].anchor) ||
              !scene_actor_movement_state(
                  scene, &scene->actors[actor], &movement) ||
              scene->actors[actor].team >=
                   TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT ||
              scene->actors[actor].roster_index >=
                   TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT ||
              scene->actors[actor].condition !=
                   scene->fatigue_state.condition
                       [scene->actors[actor].team]
                       [scene->actors[actor].roster_index]))) {
            return false;
        }
    }
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        uint8_t team = scene->launch.controller_team[controller];
        uint8_t controlled = scene->controlled_actor[controller];
        if (team == TECMO_GAMEPLAY_SCENE_NO_TEAM) {
            if (controlled != TECMO_GAMEPLAY_SCENE_NO_ACTOR) return false;
        } else if (controlled >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                   scene->actors[controlled].team != team) {
            return false;
        }
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        (scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
         scene->actors[scene->ball_holder].team != scene->state.possession)) {
        return false;
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (scene->launch.controller_team[controller] ==
                    scene->state.possession &&
                scene->controlled_actor[controller] != scene->ball_holder) {
                return false;
            }
        }
    }
    return true;
}
