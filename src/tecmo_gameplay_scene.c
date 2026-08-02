#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_scene_internal.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool scene_self_test_skip_pretip;

static void scene_set_status(TecmoGameplayScene *scene, const char *status)
{
    if (scene == NULL) return;
    (void)snprintf(scene->status, sizeof(scene->status), "%s",
                   status != NULL ? status : "");
}

static uint32_t scene_pixels_fnv1a32(const uint32_t *pixels,
                                     size_t pixel_count)
{
    uint32_t hash = 2166136261U;
    size_t pixel;
    if (pixels == NULL) return 0U;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        unsigned shift;
        for (shift = 0U; shift < 32U; shift += 8U) {
            hash ^= (pixels[pixel] >> shift) & 0xFFU;
            hash *= 16777619U;
        }
    }
    return hash;
}

static bool scene_file_exists(const char *path)
{
    FILE *file;
    if (path == NULL || path[0] == '\0') return false;
    file = fopen(path, "rb");
    if (file == NULL) return false;
    fclose(file);
    return true;
}

static bool scene_copy_path(char *destination, size_t destination_size,
                            const char *path)
{
    int written;
    if (destination == NULL || destination_size == 0U || path == NULL ||
        path[0] == '\0') {
        return false;
    }
    written = snprintf(destination, destination_size, "%s", path);
    return written >= 0 && (size_t)written < destination_size;
}

static bool scene_join_path(char *destination, size_t destination_size,
                            const char *root, const char *suffix)
{
    size_t length;
    int written;
    if (destination == NULL || destination_size == 0U || root == NULL ||
        root[0] == '\0' || suffix == NULL) {
        return false;
    }
    length = strlen(root);
    written = snprintf(destination, destination_size, "%s%s%s", root,
                       root[length - 1U] == '\\' || root[length - 1U] == '/'
                           ? ""
                           : "\\",
                       suffix);
    return written >= 0 && (size_t)written < destination_size;
}

static bool scene_select_asset_pack(char *destination,
                                    size_t destination_size,
                                    const char *project_root,
                                    const char *explicit_path)
{
    const char *environment_path;
    char root_build[1024];
    char root_pack[1024];

    if (explicit_path != NULL) {
        return scene_copy_path(destination, destination_size, explicit_path);
    }
    environment_path = getenv("TECMO_ASSETPACK");
    if (environment_path != NULL && environment_path[0] != '\0') {
        return scene_copy_path(destination, destination_size,
                               environment_path);
    }
    if (scene_join_path(root_build, sizeof(root_build), project_root,
                        "build\\tecmo.assetpack") &&
        scene_file_exists(root_build)) {
        return scene_copy_path(destination, destination_size, root_build);
    }
    if (scene_join_path(root_pack, sizeof(root_pack), project_root,
                        "tecmo.assetpack") && scene_file_exists(root_pack)) {
        return scene_copy_path(destination, destination_size, root_pack);
    }
    if (scene_file_exists("build\\tecmo.assetpack")) {
        return scene_copy_path(destination, destination_size,
                               "build\\tecmo.assetpack");
    }
    if (scene_file_exists("tecmo.assetpack")) {
        return scene_copy_path(destination, destination_size,
                               "tecmo.assetpack");
    }
    return false;
}

static void scene_release_owned(TecmoGameplayScene *scene)
{
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return;
    }
    if (scene->audio_player.asset != NULL) {
        tecmo_gameplay_audio_stop_all(&scene->audio_player);
    }
    tecmo_gameplay_audio_asset_shutdown(&scene->audio_asset);
    tecmo_gameplay_dunk_cutaway_destroy(&scene->dunk_cutaway);
    tecmo_gameplay_jump_shots_destroy(&scene->jump_shots);
    tecmo_gameplay_shot_resolution_destroy(&scene->shot_resolution);
    tecmo_gameplay_pretip_destroy(&scene->pretip_assets);
    free(scene->pretip_closeup);
    free(scene->pretip_team_data);
    tecmo_gameplay_close_shots_destroy(&scene->close_shots);
    tecmo_gameplay_free_throw_lineup_destroy(
        &scene->free_throw_lineup_assets);
    tecmo_gameplay_hud_assets_destroy(&scene->hud_assets);
    tecmo_gameplay_court_orientation_destroy(&scene->court_orientation);
    tecmo_gameplay_cpu_steering_assets_destroy(
        &scene->cpu_steering_assets);
    tecmo_gameplay_ball_dribble_assets_destroy(
        &scene->ball_dribble_assets);
    tecmo_gameplay_penalties_destroy(&scene->penalty_assets);
    tecmo_gameplay_violation_referee_destroy(
        &scene->violation_referee_assets);
    tecmo_gameplay_backcourt_assets_destroy(&scene->backcourt_assets);
    tecmo_gameplay_fatigue_assets_destroy(&scene->fatigue_assets);
    tecmo_gameplay_movement_assets_destroy(&scene->movement_assets);
    tecmo_gameplay_camera_assets_destroy(&scene->camera_assets);
    tecmo_gameplay_court_destroy(&scene->court);
    tecmo_gameplay_assets_destroy(&scene->assets);
    memset(scene, 0, sizeof(*scene));
    scene->lifecycle_tag = TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG;
    tecmo_gameplay_assets_init(&scene->assets);
    tecmo_gameplay_court_init(&scene->court);
    tecmo_gameplay_camera_assets_init(&scene->camera_assets);
    tecmo_gameplay_movement_assets_init(&scene->movement_assets);
    tecmo_gameplay_ball_dribble_assets_init(
        &scene->ball_dribble_assets);
    tecmo_gameplay_cpu_steering_assets_init(
        &scene->cpu_steering_assets);
    tecmo_gameplay_penalties_init(&scene->penalty_assets);
    tecmo_gameplay_violation_referee_init(
        &scene->violation_referee_assets);
    tecmo_gameplay_backcourt_assets_init(&scene->backcourt_assets);
    tecmo_gameplay_fatigue_assets_init(&scene->fatigue_assets);
    tecmo_gameplay_court_orientation_init(&scene->court_orientation);
    tecmo_gameplay_free_throw_lineup_init(
        &scene->free_throw_lineup_assets);
    tecmo_gameplay_hud_assets_init(&scene->hud_assets);
    tecmo_gameplay_close_shots_init(&scene->close_shots);
    tecmo_gameplay_dunk_cutaway_init(&scene->dunk_cutaway);
    tecmo_gameplay_jump_shots_init(&scene->jump_shots);
    tecmo_gameplay_shot_resolution_init(&scene->shot_resolution);
    tecmo_gameplay_pretip_init(&scene->pretip_assets);
    scene_set_status(scene, "gameplay scene initialized; assets not loaded");
}

void tecmo_gameplay_scene_init(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    memset(scene, 0, sizeof(*scene));
    scene->lifecycle_tag = TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG;
    tecmo_gameplay_assets_init(&scene->assets);
    tecmo_gameplay_court_init(&scene->court);
    tecmo_gameplay_camera_assets_init(&scene->camera_assets);
    tecmo_gameplay_movement_assets_init(&scene->movement_assets);
    tecmo_gameplay_ball_dribble_assets_init(
        &scene->ball_dribble_assets);
    tecmo_gameplay_cpu_steering_assets_init(
        &scene->cpu_steering_assets);
    tecmo_gameplay_penalties_init(&scene->penalty_assets);
    tecmo_gameplay_violation_referee_init(
        &scene->violation_referee_assets);
    tecmo_gameplay_backcourt_assets_init(&scene->backcourt_assets);
    tecmo_gameplay_fatigue_assets_init(&scene->fatigue_assets);
    tecmo_gameplay_court_orientation_init(&scene->court_orientation);
    tecmo_gameplay_free_throw_lineup_init(
        &scene->free_throw_lineup_assets);
    tecmo_gameplay_hud_assets_init(&scene->hud_assets);
    tecmo_gameplay_close_shots_init(&scene->close_shots);
    tecmo_gameplay_dunk_cutaway_init(&scene->dunk_cutaway);
    tecmo_gameplay_jump_shots_init(&scene->jump_shots);
    tecmo_gameplay_shot_resolution_init(&scene->shot_resolution);
    tecmo_gameplay_pretip_init(&scene->pretip_assets);
    scene_set_status(scene, "gameplay scene initialized; assets not loaded");
}

bool tecmo_gameplay_scene_load(TecmoGameplayScene *scene,
                               const char *project_root,
                               const char *asset_pack_path,
                               TecmoMusicPlayer *music_player)
{
    char selected_candidate[1024];
    char selected[1024];
    char failure[192];

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return false;
    }
    scene_release_owned(scene);
    if (!scene_select_asset_pack(
            selected_candidate, sizeof(selected_candidate), project_root,
            asset_pack_path) ||
        tecmo_asset_pack_canonicalize_path(
            selected_candidate, selected, sizeof(selected)) != 0) {
        scene_set_status(scene, "gameplay asset pack unavailable");
        return false;
    }
    /* TGSR performs its own exact same-pack TGPL dependency read. Load it
       first so scene failures identify the shot-resolution boundary directly. */
    if (!tecmo_gameplay_shot_resolution_load(&scene->shot_resolution,
                                              selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->shot_resolution.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_assets_load(&scene->assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s", scene->assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_pretip_load(&scene->pretip_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->pretip_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    scene->pretip_team_data =
        (TecmoTeamDataAsset *)malloc(sizeof(*scene->pretip_team_data));
    scene->pretip_closeup =
        (TecmoIntroWarriorsAsset *)malloc(sizeof(*scene->pretip_closeup));
    if (scene->pretip_team_data == NULL || scene->pretip_closeup == NULL) {
        (void)snprintf(failure, sizeof(failure),
                       "pre-tip dependency allocation failed");
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_team_data_asset_load_from_pack(
            scene->pretip_team_data, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->pretip_team_data->status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_intro_warriors_asset_load_from_pack(
            scene->pretip_closeup, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->pretip_closeup->status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_court_load(&scene->court, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s", scene->court.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_camera_assets_load(&scene->camera_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->camera_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_movement_assets_load(
            &scene->movement_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->movement_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_ball_dribble_assets_load(
            &scene->ball_dribble_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->ball_dribble_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_assets_load(
            &scene->cpu_steering_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->cpu_steering_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_penalties_load(&scene->penalty_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->penalty_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_violation_referee_load(
            &scene->violation_referee_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->violation_referee_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_fatigue_assets_load(
            &scene->fatigue_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->fatigue_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_court_decode_world(&scene->court,
                                           &scene->court_world)) {
        scene_release_owned(scene);
        scene_set_status(scene, "TGCT-1 full court world decode rejected");
        return false;
    }
    if (!tecmo_gameplay_court_orientation_load(
            &scene->court_orientation, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->court_orientation.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_backcourt_assets_load(
            &scene->backcourt_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->backcourt_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_free_throw_lineup_load(
            &scene->free_throw_lineup_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->free_throw_lineup_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_hud_assets_load(&scene->hud_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->hud_assets.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_close_shots_load(&scene->close_shots, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->close_shots.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_dunk_cutaway_load(&scene->dunk_cutaway, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->dunk_cutaway.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (!tecmo_gameplay_jump_shots_load(&scene->jump_shots, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->jump_shots.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    if (music_player == NULL || music_player->asset == NULL ||
        !music_player->asset->available ||
        music_player->asset->payload_fingerprint !=
            TECMO_MUSIC_PAYLOAD_FNV1A32 ||
        strcmp(music_player->asset->asset_pack_path, selected) != 0 ||
        !tecmo_gameplay_audio_asset_load_from_pack(&scene->audio_asset,
                                                   selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       music_player == NULL || music_player->asset == NULL ||
                               !music_player->asset->available ||
                               music_player->asset->payload_fingerprint !=
                                   TECMO_MUSIC_PAYLOAD_FNV1A32 ||
                               strcmp(music_player->asset->asset_pack_path,
                                      selected) != 0
                           ? "TMUS-1 shared music player unavailable"
                           : scene->audio_asset.status);
        scene_release_owned(scene);
        scene_set_status(scene, failure);
        return false;
    }
    tecmo_gameplay_audio_player_init(&scene->audio_player,
                                     &scene->audio_asset, music_player);
    if (!scene_copy_path(scene->asset_pack_path,
                         sizeof(scene->asset_pack_path), selected)) {
        scene_release_owned(scene);
        scene_set_status(scene, "gameplay asset pack path too long");
        return false;
    }
    scene->available = true;
    scene_set_status(scene,
                     "native gameplay ready: TPTI-1/TGPL-1/TTDT-1/TWAR-1/TMUS-1/TGCT-1/TGCP-2/TGMO-1/TGBD-1/TGAI-1/TGFT-1/TPNL-1/TGVR-1/TGOR-1/TGFL-1/THUD-1/TGCS-1/TGDK-1/TGJS-2/TGSR-3/TSFX-1/TDMC-1");
    return true;
}

void tecmo_gameplay_scene_destroy(TecmoGameplayScene *scene)
{
    scene_release_owned(scene);
}

static bool scene_source_valid(TecmoGameplaySceneSource source)
{
    return source >= TECMO_GAMEPLAY_SCENE_PRESEASON &&
           source < TECMO_GAMEPLAY_SCENE_SOURCE_COUNT;
}

static void scene_clear_free_throw_lineup_binding(
    TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    scene->free_throw_lineup_transition_serial = 0U;
    scene->free_throw_lineup_orientation =
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX;
    scene->free_throw_shooter = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->free_throw_secondary = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->free_throw_lineup_active = false;
}

static bool scene_launch_valid(const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayConfig config;
    if (launch == NULL || !scene_source_valid(launch->source) ||
        launch->away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->home_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->away_team == launch->home_team || launch->difficulty > 2U ||
        launch->control_mode > 6U || launch->speed_value > 2U ||
        !scene_court_controller_team_valid(launch->controller_team[0]) ||
        !scene_court_controller_team_valid(launch->controller_team[1]) ||
        (launch->controller_team[0] != TECMO_GAMEPLAY_SCENE_NO_TEAM &&
         launch->controller_team[0] == launch->controller_team[1])) {
        return false;
    }
    if (!tecmo_gameplay_config_init(&config,
                                    launch->regulation_minutes)) {
        return false;
    }
    return true;
}

static bool scene_initialize_fatigue(TecmoGameplayScene *scene)
{
    TecmoGameplayFatigueRosterSeed
        seeds[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
             [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
    uint8_t team_id[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT];
    if (scene == NULL || !scene->fatigue_assets.available ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        return false;
    }
    team_id[TECMO_GAMEPLAY_TEAM_AWAY] = scene->launch.away_team;
    team_id[TECMO_GAMEPLAY_TEAM_HOME] = scene->launch.home_team;
    if (team_id[0U] >= TECMO_TEAM_DATA_TEAM_COUNT ||
        team_id[1U] >= TECMO_TEAM_DATA_TEAM_COUNT) {
        return false;
    }
    memset(seeds, 0, sizeof(seeds));
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        for (size_t roster = 0U;
             roster < TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT; ++roster) {
            const TecmoTeamDataPlayer *player =
                &scene->pretip_team_data->players[team_id[team]][roster];
            seeds[team][roster].condition = player->condition_seed;
            seeds[team][roster].maximum_capacity =
                player->profile[scene->fatigue_assets.capacity_profile_index];
        }
    }
    return tecmo_gameplay_fatigue_state_initialize(
        &scene->fatigue_assets, &scene->fatigue_state, seeds);
}

static bool scene_initialize_actors(TecmoGameplayScene *scene)
{
    /* Native approximate starting layout expressed once in canonical
       full-court coordinates. These are not claimed as ROM lineup seeds. */
    static const TecmoGameplayCourtCoordinate
        initial_positions[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
            {0x0160, 198}, {0x017C, 167}, {0x0197, 207},
            {0x01B2, 151}, {0x01CF, 183}, {0x016F, 214},
            {0x018B, 190}, {0x01A6, 169}, {0x01C2, 205},
            {0x01DE, 145}
    };
    TecmoGameplaySceneActor
        initialized[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        initialized_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t controlled[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayCourtCoordinateQ8 ball_position;
    TecmoGameplayBallDribbleFrame ball_frame;
    size_t actor;
    if (scene == NULL || !scene->movement_assets.available ||
        !scene->cpu_steering_assets.available ||
        !tecmo_gameplay_fatigue_state_valid(
            &scene->fatigue_assets, &scene->fatigue_state) ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        scene->launch.speed_value >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT) {
        return false;
    }
    memset(initialized, 0, sizeof(initialized));
    memset(initialized_cpu, 0, sizeof(initialized_cpu));
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplaySceneActor *item = &initialized[actor];
        TecmoGameplaySceneCpuActor *cpu = &initialized_cpu[actor];
        const TecmoTeamDataPlayer *player;
        TecmoGameplayMovementState movement;
        uint16_t pose_index;
        uint8_t linked_actor;
        uint8_t team_id;
        int adjusted_rating;
        item->position = initial_positions[actor];
        item->anchor = initial_positions[actor];
        item->team = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                         ? TECMO_GAMEPLAY_TEAM_AWAY
                         : TECMO_GAMEPLAY_TEAM_HOME;
        item->roster_index = (uint8_t)(actor %
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
        item->sprite_slot_base = 0x41U;
        /* Base actor facing is owned by TGOR's team-to-goal mapping. Fresh
           TGOR starts Away on the left goal, so Away actors face left. */
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)item->team,
                &item->facing_right)) {
            return false;
        }
        item->pose_orientation_encoded = false;
        item->active = true;
        linked_actor = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
            ? (uint8_t)(actor + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT)
            : (uint8_t)(actor - TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
        team_id = item->team == TECMO_GAMEPLAY_TEAM_AWAY
                      ? scene->launch.away_team : scene->launch.home_team;
        if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT ||
            item->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
            return false;
        }
        player = &scene->pretip_team_data->players[team_id]
                                                   [item->roster_index];
        adjusted_rating = (int)player->profile[0] +
            scene->movement_assets.speed_adjustment[
                scene->launch.speed_value];
        if (player->condition_seed > 0x64U ||
            adjusted_rating <
                (int)scene->movement_assets.minimum_movement_amount ||
            adjusted_rating > 0xFF ||
            !tecmo_gameplay_movement_state_initialize(
                &scene->movement_assets, &movement, &item->position,
                item->facing_right ? 0U : 1U) ||
            !scene_movement_pose_index(
                scene, &movement, &initial_positions[linked_actor],
                &pose_index)) {
            return false;
        }
        item->pose_index = pose_index;
        item->movement_action_state = movement.action_state;
        item->movement_direction = movement.direction;
        item->movement_fractional_accumulator =
            movement.fractional_accumulator;
        item->movement_animation_phase = movement.animation_phase;
        item->condition = scene->fatigue_state.condition
            [item->team][item->roster_index];
        item->movement_boundary_latched =
            movement.boundary_violation_latched;
        cpu->contract_tag = TECMO_GAMEPLAY_SCENE_CPU_ACTOR_TAG;
        cpu->command_offset =
            TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
        cpu->linked_actor = linked_actor;
        cpu->target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT;
        cpu->direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        cpu->held_direction_bits =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++actor) {
        if (scene->launch.controller_team[actor] == TECMO_GAMEPLAY_TEAM_AWAY) {
            controlled[actor] = 0U;
        } else if (scene->launch.controller_team[actor] ==
                   TECMO_GAMEPLAY_TEAM_HOME) {
            controlled[actor] =
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        } else {
            controlled[actor] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        }
    }
    memcpy(scene->actors, initialized, sizeof(initialized));
    memcpy(scene->cpu_actors, initialized_cpu, sizeof(initialized_cpu));
    memcpy(scene->controlled_actor, controlled, sizeof(controlled));
    scene->ball_holder = 0U;
    if (!scene_live_ball_frame_for_actors(
            scene, scene->actors, scene->ball_holder, &ball_frame) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &ball_frame.visible_position, &ball_position)) {
        return false;
    }
    scene->ball_position = ball_position;
    return true;
}

static bool scene_initialize_tip_actors(TecmoGameplayScene *scene)
{
    TecmoGameplayPreTipLineup lineup;
    size_t actor;
    size_t jumper;
    if (scene == NULL ||
        !tecmo_gameplay_pretip_tip_lineup(
            &scene->pretip_assets, &lineup) ||
        lineup.ball_sprite_slot_base != 0xC1U ||
        lineup.ball_pose_index != TECMO_GAMEPLAY_BALL_POSE ||
        scene->pretip_assets.tip_actor_indices[0U] >=
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->pretip_assets.tip_actor_indices[1U] <
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene->pretip_assets.tip_actor_indices[1U] >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->pretip_assets.tip_actor_indices[0U] ==
            scene->pretip_assets.tip_actor_indices[1U]) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        scene->actors[actor].position = lineup.players[actor];
        scene->actors[actor].anchor = lineup.players[actor];
        scene->actors[actor].pose_index = lineup.player_pose_indices[actor];
        scene->actors[actor].sprite_slot_base =
            lineup.player_sprite_slot_bases[actor];
        /* Bank04 already selects an orientation-specific pointer through
           $ADC4/$ADCD, so the scene must not mirror that resolved pose again. */
        scene->actors[actor].pose_orientation_encoded = true;
    }
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        uint8_t actor_index = scene->pretip_assets.tip_actor_indices[jumper];
        scene->pretip_jumper_actor[jumper] = actor_index;
        scene->pretip_jumper_standing_pose[jumper] =
            lineup.player_pose_indices[actor_index];
        scene->pretip_jumper_altitude_q8[jumper] = 0U;
    }
    scene->pretip_jump_active = false;
    scene->ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->ball_position.x_q8 = (int32_t)lineup.ball.x * 256;
    scene->ball_position.y_q8 = (int32_t)lineup.ball.y * 256;
    return true;
}

static int32_t scene_pretip_descent_ball_y_q8(uint16_t phase_frame)
{
    uint16_t clamped_frame =
        phase_frame < TECMO_GAMEPLAY_PRETIP_DESCENT_MOVE_FRAMES
            ? phase_frame
            : TECMO_GAMEPLAY_PRETIP_DESCENT_MOVE_FRAMES;
    uint32_t distance =
        (uint32_t)(TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y -
                   TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y);
    uint32_t y = TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y +
                 (distance * clamped_frame) /
                     TECMO_GAMEPLAY_PRETIP_DESCENT_MOVE_FRAMES;
    return (int32_t)y * 256;
}

static bool scene_pretip_jumper_mapping_valid(
    const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->pretip_jumper_actor[0U] <
               TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT &&
           scene->pretip_jumper_actor[1U] >=
               TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT &&
           scene->pretip_jumper_actor[1U] <
               TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           scene->pretip_jumper_actor[0U] !=
               scene->pretip_jumper_actor[1U];
}

static bool scene_pretip_jumper_order(
    const TecmoGameplayScene *scene,
    size_t *left_jumper_out,
    size_t *right_jumper_out)
{
    const TecmoGameplaySceneActor *first;
    const TecmoGameplaySceneActor *second;
    int16_t first_x;
    int16_t second_x;
    if (scene == NULL || left_jumper_out == NULL ||
        right_jumper_out == NULL || !scene_pretip_jumper_mapping_valid(scene)) {
        return false;
    }
    first = &scene->actors[scene->pretip_jumper_actor[0U]];
    second = &scene->actors[scene->pretip_jumper_actor[1U]];
    if (!first->active || !second->active ||
        !scene_actor_coordinate_valid(&first->anchor) ||
        !scene_actor_coordinate_valid(&second->anchor)) {
        return false;
    }
    first_x = first->anchor.x;
    second_x = second->anchor.x;
    if (first_x == second_x) return false;
    if (first_x < second_x) {
        *left_jumper_out = 0U;
        *right_jumper_out = 1U;
    } else {
        *left_jumper_out = 1U;
        *right_jumper_out = 0U;
    }
    return true;
}

static bool scene_pretip_jumper_inward_facing(
    const TecmoGameplayScene *scene,
    bool facing_right_out[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT])
{
    size_t left_jumper;
    size_t right_jumper;
    if (facing_right_out == NULL ||
        !scene_pretip_jumper_order(
            scene, &left_jumper, &right_jumper)) {
        return false;
    }
    facing_right_out[left_jumper] = true;
    facing_right_out[right_jumper] = false;
    return true;
}

static uint16_t scene_pretip_jump_altitude_q8(uint16_t phase_frame)
{
    const uint32_t maximum = TECMO_GAMEPLAY_PRETIP_JUMP_MAX_ALTITUDE_Q8;
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_CROUCH_LAST_FRAME) {
        return 0U;
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_RISE_LAST_FRAME) {
        uint32_t steps = (uint32_t)phase_frame -
                         TECMO_GAMEPLAY_PRETIP_JUMP_CROUCH_LAST_FRAME;
        uint32_t total = TECMO_GAMEPLAY_PRETIP_JUMP_RISE_LAST_FRAME -
                         TECMO_GAMEPLAY_PRETIP_JUMP_CROUCH_LAST_FRAME;
        return (uint16_t)((maximum * steps) / total);
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_APEX_LAST_FRAME) {
        return (uint16_t)maximum;
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_FALL_LAST_FRAME) {
        uint32_t elapsed = (uint32_t)phase_frame -
                           TECMO_GAMEPLAY_PRETIP_JUMP_APEX_LAST_FRAME;
        uint32_t total = TECMO_GAMEPLAY_PRETIP_JUMP_FALL_LAST_FRAME -
                         TECMO_GAMEPLAY_PRETIP_JUMP_APEX_LAST_FRAME;
        return (uint16_t)(maximum - (maximum * elapsed) / total);
    }
    return 0U;
}

static uint16_t scene_pretip_jump_pose(uint16_t phase_frame)
{
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_CROUCH_LAST_FRAME) {
        return TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE;
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_TAKEOFF_LAST_FRAME) {
        return TECMO_GAMEPLAY_JUMP_TURN_POSE;
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_RISE_LAST_FRAME) {
        return TECMO_GAMEPLAY_JUMP_RELEASE_POSE;
    }
    if (phase_frame <= TECMO_GAMEPLAY_PRETIP_JUMP_FALL_LAST_FRAME) {
        return TECMO_GAMEPLAY_JUMP_FLIGHT_POSE;
    }
    return TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
}

static bool scene_pretip_apply_jump_frame(
    TecmoGameplayScene *scene,
    uint16_t phase_frame)
{
    bool inward_facing[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint16_t pose_index;
    size_t jumper;
    if (scene == NULL ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        phase_frame >= TECMO_GAMEPLAY_PRETIP_JUMP_DURATION ||
        !scene_pretip_jumper_inward_facing(scene, inward_facing)) {
        return false;
    }
    pose_index = scene_pretip_jump_pose(phase_frame);
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        TecmoGameplaySceneActor *actor = &scene->actors[
            scene->pretip_jumper_actor[jumper]];
        actor->position = actor->anchor;
        actor->pose_index = pose_index;
        /* Generic TGJS-derived action pointers are not orientation encoded.
           Resolve the two actual jumper anchors so their action silhouettes
           face inward, independent of team/array order. */
        actor->facing_right = inward_facing[jumper];
        actor->pose_orientation_encoded = false;
        scene->pretip_jumper_altitude_q8[jumper] =
            scene_pretip_jump_altitude_q8(phase_frame);
    }
    scene->pretip_jump_active = true;
    return true;
}

static bool scene_pretip_land_jump(TecmoGameplayScene *scene)
{
    bool goal_facing[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    size_t left_jumper;
    size_t right_jumper;
    size_t jumper;
    if (!scene_pretip_jumper_order(
            scene, &left_jumper, &right_jumper)) {
        return false;
    }
    (void)left_jumper;
    (void)right_jumper;
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        TecmoGameplaySceneActor *actor = &scene->actors[
            scene->pretip_jumper_actor[jumper]];
        if (!scene_goal_facing_right_for_team(
                scene, (TecmoGameplayTeam)actor->team,
                &goal_facing[jumper])) {
            return false;
        }
    }
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        TecmoGameplaySceneActor *actor = &scene->actors[
            scene->pretip_jumper_actor[jumper]];
        actor->position = actor->anchor;
        actor->pose_index = scene->pretip_jumper_standing_pose[jumper];
        actor->facing_right = goal_facing[jumper];
        actor->pose_orientation_encoded = true;
        scene->pretip_jumper_altitude_q8[jumper] = 0U;
    }
    scene->pretip_jump_active = false;
    return true;
}

bool tecmo_gameplay_scene_launch(TecmoGameplayScene *scene,
                                 const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayConfig config;
    TecmoGameplayState initial_state;
    TecmoGameplayCourtOrientationState initial_orientation;
    TecmoGameplayCameraState initial_camera;
    TecmoGameplayBackcourtState initial_backcourt;
    TecmoGameplaySceneCourtCoordinates initial_coordinates;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene_launch_valid(launch) ||
        !tecmo_gameplay_config_init(&config, launch->regulation_minutes)) {
        return false;
    }
    scene->launch = *launch;
    memset(&scene->result, 0, sizeof(scene->result));
    tecmo_gameplay_events_clear(&scene->events);
    if (!tecmo_gameplay_state_init(&initial_state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_court_orientation_state_initialize(
            &scene->court_orientation, &initial_orientation) ||
        !tecmo_gameplay_camera_state_initialize(
            &scene->camera_assets, &initial_camera) ||
        !tecmo_gameplay_camera_state_prime_live(
            &scene->camera_assets, &initial_camera) ||
        !tecmo_gameplay_backcourt_state_initialize(
            &scene->backcourt_assets, &initial_backcourt)) {
        scene_set_status(scene, "gameplay state initialization rejected");
        return false;
    }
    scene->state = initial_state;
    scene->orientation_state = initial_orientation;
    scene->camera_state = initial_camera;
    scene->backcourt_state = initial_backcourt;
    if (!scene_initialize_fatigue(scene) ||
        !scene_initialize_actors(scene)) {
        scene_set_status(scene, "gameplay actor movement initialization rejected");
        return false;
    }
    if (!tecmo_gameplay_camera_settle_court(
            &scene->camera_assets, &scene->camera_state,
            &scene->ball_position,
            scene->orientation_state.current_direction, false) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state)) {
        scene_set_status(scene, "gameplay live camera initialization rejected");
        return false;
    }
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->close_shot_profile = TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0;
    scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    scene_shot_clear_jump_playback(scene);
    scene->frame = 0U;
    scene->action_serial = 0U;
    scene->camera_follow_count = 0U;
    scene->free_throw_frame = 0U;
    scene_clear_free_throw_lineup_binding(scene);
    scene->previous_phase = scene->state.phase;
    scene->pretip_abort_pending = false;
    scene->result_ready = false;
    scene->active = true;
    if (!tecmo_gameplay_pretip_state_initialize(
            &scene->pretip_assets, &scene->pretip_state,
            launch->source == TECMO_GAMEPLAY_SCENE_SEASON)) {
        scene_set_status(scene, "pre-tip state initialization rejected");
        scene->active = false;
        return false;
    }
    if (!scene_initialize_tip_actors(scene)) {
        scene_set_status(scene, "ROM tip-off lineup initialization rejected");
        scene->active = false;
        return false;
    }
    if (!scene_self_test_skip_pretip) {
        /* TPTI's fixed center-court lineup spans both sides of the 0x0100
           TGCP launch viewport. Do not let the live ball/goal attachment
           pre-settle move that presentation camera before the tip. The live
           handoff below settles again around the awarded possession. */
        scene->camera_state = initial_camera;
        if (!tecmo_gameplay_camera_state_live_valid(
                &scene->camera_assets, &scene->camera_state)) {
            scene_set_status(scene, "pre-tip center camera initialization rejected");
            scene->active = false;
            return false;
        }
    }
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    tecmo_gameplay_audio_set_game_music_enabled(
        &scene->audio_player, launch->game_music_enabled);
    if (!tecmo_gameplay_audio_queue_pregame_matchup_stinger(
            &scene->audio_player)) {
        scene_set_status(scene, "pre-tip track 8 queue rejected");
        scene->active = false;
        return false;
    }
    if (scene_self_test_skip_pretip) {
        size_t phase;
        scene->pretip_state.total_frame = 0U;
        for (phase = 0U; phase < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++phase) {
            scene->pretip_state.total_frame +=
                phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST
                    ? TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES
                    : scene->pretip_assets.phase_frames[phase];
        }
        scene->pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
        scene->pretip_state.phase_frame = 0U;
        scene->pretip_state.contest_frame = scene->pretip_assets.phase_frames[
            TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST];
        scene->pretip_state.live_handoff = true;
        if (!scene_initialize_actors(scene)) {
            scene_set_status(scene,
                             "self-test actor movement initialization rejected");
            scene->active = false;
            return false;
        }
        if (launch->game_music_enabled &&
            !tecmo_gameplay_audio_queue_game_music(&scene->audio_player)) {
            scene_set_status(scene, "self-test live music handoff rejected");
            scene->active = false;
            return false;
        }
    }
    if (!tecmo_gameplay_scene_court_coordinates(
            scene, &initial_coordinates)) {
        scene_set_status(scene, "gameplay initial coordinates rejected");
        scene->active = false;
        return false;
    }
    scene_set_status(scene, "native pre-tip active");
    return true;
}

static void scene_pad_from_controls(TecmoGameplayPadInput *pad,
                                    const TecmoControlFrame *controls)
{
    memset(pad, 0, sizeof(*pad));
    if (controls == NULL) return;
    pad->held.dpad_up = controls->held.up;
    pad->held.dpad_down = controls->held.down;
    pad->held.dpad_left = controls->held.left;
    pad->held.dpad_right = controls->held.right;
    pad->held.nes_a_pass_switch = controls->held.shoot;
    pad->held.nes_b_jump_steal_shot = controls->held.cancel;
    pad->held.nes_select = controls->held.tab;
    pad->held.nes_start = controls->held.confirm;
    pad->released.dpad_up = controls->released.up;
    pad->released.dpad_down = controls->released.down;
    pad->released.dpad_left = controls->released.left;
    pad->released.dpad_right = controls->released.right;
    pad->released.nes_a_pass_switch = controls->released.shoot;
    pad->released.nes_b_jump_steal_shot = controls->released.cancel;
    pad->released.nes_select = controls->released.tab;
    pad->released.nes_start = controls->released.confirm;
}

static bool scene_controls_pressed_a(const TecmoControlFrame *controls)
{
    return controls != NULL && controls->pressed.shoot;
}

static bool scene_controls_pressed_b(const TecmoControlFrame *controls)
{
    return controls != NULL && controls->pressed.cancel;
}

static bool scene_controls_held_b(const TecmoControlFrame *controls)
{
    return controls != NULL && controls->held.cancel;
}


static bool scene_free_throw_lineup_policy_slots(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam scoring_team,
    uint8_t *shooter_out,
    uint8_t *secondary_out)
{
    TecmoGameplayTeam other_team;
    size_t other_controller;
    uint8_t shooter;
    uint8_t secondary;
    if (scene == NULL || shooter_out == NULL || secondary_out == NULL ||
        (scoring_team != TECMO_GAMEPLAY_TEAM_AWAY &&
         scoring_team != TECMO_GAMEPLAY_TEAM_HOME)) {
        return false;
    }
    shooter = scene->ball_holder;
    if (shooter >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[shooter].team != (uint8_t)scoring_team) {
        shooter = scene_first_actor_for_team(scoring_team);
    }
    other_team = scene_other_team(scoring_team);
    other_controller = scene_controller_for_team(scene, other_team);
    secondary =
        other_controller < TECMO_GAMEPLAY_CONTROLLER_COUNT
            ? scene->controlled_actor[other_controller]
            : TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (secondary >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[secondary].team != (uint8_t)other_team) {
        secondary = scene_first_actor_for_team(other_team);
    }
    if (shooter >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        secondary >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        shooter == secondary) {
        return false;
    }
    *shooter_out = shooter;
    *secondary_out = secondary;
    return true;
}

static bool scene_apply_free_throw_lineup(
    TecmoGameplayScene *scene,
    uint8_t shooter,
    uint8_t secondary)
{
    TecmoGameplayFreeThrowLineup lineup;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneActor
        previous_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCameraState candidate_camera;
    TecmoGameplayCameraState previous_camera;
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplayCourtCoordinateQ8 previous_ball;
    TecmoGameplayCourtCoordinateQ8 focus;
    uint8_t candidate_controlled[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t previous_controlled[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t previous_holder;
    uint32_t previous_transition_serial;
    uint8_t previous_orientation;
    uint8_t previous_shooter;
    uint8_t previous_secondary;
    bool previous_active;
    size_t controller;
    size_t actor;
    if (scene == NULL ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene->orientation_state.current_direction >=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.free_throws.scoring_team ||
        shooter >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        secondary >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        shooter == secondary ||
        scene->actors[shooter].team !=
            (uint8_t)scene->state.free_throws.scoring_team ||
        scene->actors[secondary].team ==
            (uint8_t)scene->state.free_throws.scoring_team ||
        !tecmo_gameplay_free_throw_lineup_derive(
            &scene->free_throw_lineup_assets,
            scene->orientation_state.current_direction,
            shooter, secondary, &lineup)) {
        return false;
    }
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++actor) {
        const TecmoGameplayFreeThrowLineupActor *source =
            &lineup.actors[actor];
        TecmoGameplayCourtCoordinate coordinate;
        if (!source->position_defined ||
            source->raw_world_x >
                (uint16_t)TECMO_GAMEPLAY_COURT_WORLD_MAX_X ||
            source->raw_world_y >
                (uint8_t)TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
            return false;
        }
        coordinate.x = (int16_t)source->raw_world_x;
        coordinate.y = (int16_t)source->raw_world_y;
        if (!scene_actor_coordinate_valid(&coordinate)) return false;
        candidate_actors[actor].position = coordinate;
        candidate_actors[actor].anchor = coordinate;
    }
    /* TGFL does not define the shooter's pose. Preserve its pose family while
       deriving effective facing from the validated scoring team's goal for
       native held-ball attachment. */
    if (!scene_goal_facing_right_for_team(
            scene, (TecmoGameplayTeam)candidate_actors[shooter].team,
            &candidate_actors[shooter].facing_right)) {
        return false;
    }
    if (!scene_attached_ball_position(
            &candidate_actors[shooter], &candidate_ball) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &candidate_actors[shooter].position, &focus)) {
        return false;
    }
    candidate_camera = scene->camera_state;
    if (!tecmo_gameplay_camera_settle_court(
            &scene->camera_assets, &candidate_camera, &focus,
            scene->orientation_state.current_direction, false) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &candidate_camera)) {
        return false;
    }
    memcpy(candidate_controlled, scene->controlled_actor,
           sizeof(candidate_controlled));
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        uint8_t team = scene->launch.controller_team[controller];
        if (team == (uint8_t)scene->state.free_throws.scoring_team) {
            candidate_controlled[controller] = shooter;
        } else if (team != TECMO_GAMEPLAY_SCENE_NO_TEAM) {
            candidate_controlled[controller] = secondary;
        }
    }

    memcpy(previous_actors, scene->actors, sizeof(previous_actors));
    previous_camera = scene->camera_state;
    previous_ball = scene->ball_position;
    memcpy(previous_controlled, scene->controlled_actor,
           sizeof(previous_controlled));
    previous_holder = scene->ball_holder;
    previous_transition_serial =
        scene->free_throw_lineup_transition_serial;
    previous_orientation = scene->free_throw_lineup_orientation;
    previous_shooter = scene->free_throw_shooter;
    previous_secondary = scene->free_throw_secondary;
    previous_active = scene->free_throw_lineup_active;

    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    scene->camera_state = candidate_camera;
    scene->ball_position = candidate_ball;
    memcpy(scene->controlled_actor, candidate_controlled,
           sizeof(candidate_controlled));
    scene->ball_holder = shooter;
    scene->free_throw_lineup_transition_serial =
        scene->orientation_state.transition_serial;
    scene->free_throw_lineup_orientation =
        scene->orientation_state.current_direction;
    scene->free_throw_shooter = shooter;
    scene->free_throw_secondary = secondary;
    scene->free_throw_lineup_active = true;
    if (scene_court_free_throw_lineup_matches(scene)) return true;

    memcpy(scene->actors, previous_actors, sizeof(previous_actors));
    scene->camera_state = previous_camera;
    scene->ball_position = previous_ball;
    memcpy(scene->controlled_actor, previous_controlled,
           sizeof(previous_controlled));
    scene->ball_holder = previous_holder;
    scene->free_throw_lineup_transition_serial =
        previous_transition_serial;
    scene->free_throw_lineup_orientation = previous_orientation;
    scene->free_throw_shooter = previous_shooter;
    scene->free_throw_secondary = previous_secondary;
    scene->free_throw_lineup_active = previous_active;
    return false;
}

static bool scene_settle_foul_with_free_throw_lineup(
    TecmoGameplayScene *scene)
{
    TecmoGameplayState previous_state;
    TecmoGameplayCourtOrientationState previous_orientation;
    TecmoGameplayCameraState previous_camera;
    TecmoGameplaySceneActor
        previous_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 previous_ball;
    uint8_t previous_controlled[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t previous_holder;
    uint16_t previous_free_throw_frame;
    uint32_t previous_lineup_transition_serial;
    uint8_t previous_lineup_orientation;
    uint8_t previous_shooter;
    uint8_t previous_secondary;
    bool previous_lineup_active;
    TecmoGameplayTeam scoring_team;
    uint8_t shooter;
    uint8_t secondary;
    bool succeeded = false;
    if (scene == NULL ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED) {
        return false;
    }
    scoring_team = scene->state.free_throws.scoring_team;
    if (!scene_free_throw_lineup_policy_slots(
            scene, scoring_team, &shooter, &secondary)) {
        return false;
    }
    previous_state = scene->state;
    previous_orientation = scene->orientation_state;
    previous_camera = scene->camera_state;
    memcpy(previous_actors, scene->actors, sizeof(previous_actors));
    previous_ball = scene->ball_position;
    memcpy(previous_controlled, scene->controlled_actor,
           sizeof(previous_controlled));
    previous_holder = scene->ball_holder;
    previous_free_throw_frame = scene->free_throw_frame;
    previous_lineup_transition_serial =
        scene->free_throw_lineup_transition_serial;
    previous_lineup_orientation = scene->free_throw_lineup_orientation;
    previous_shooter = scene->free_throw_shooter;
    previous_secondary = scene->free_throw_secondary;
    previous_lineup_active = scene->free_throw_lineup_active;

    if (!tecmo_gameplay_settle_foul_presentation(
            &scene->state, scoring_team,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50)) {
        goto restore;
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE) {
        if (!scene_handoff_possession(scene, scoring_team, shooter) ||
            !scene_apply_free_throw_lineup(
                scene, shooter, secondary)) {
            goto restore;
        }
    } else {
        if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
            !scene_handoff_possession(
                scene, scene->state.possession,
                scene_first_actor_for_team(scene->state.possession))) {
            goto restore;
        }
        scene_clear_free_throw_lineup_binding(scene);
    }
    scene->free_throw_frame = 0U;
    succeeded = true;

restore:
    if (!succeeded) {
        scene->state = previous_state;
        scene->orientation_state = previous_orientation;
        scene->camera_state = previous_camera;
        memcpy(scene->actors, previous_actors, sizeof(previous_actors));
        scene->ball_position = previous_ball;
        memcpy(scene->controlled_actor, previous_controlled,
               sizeof(previous_controlled));
        scene->ball_holder = previous_holder;
        scene->free_throw_frame = previous_free_throw_frame;
        scene->free_throw_lineup_transition_serial =
            previous_lineup_transition_serial;
        scene->free_throw_lineup_orientation =
            previous_lineup_orientation;
        scene->free_throw_shooter = previous_shooter;
        scene->free_throw_secondary = previous_secondary;
        scene->free_throw_lineup_active = previous_lineup_active;
    }
    return succeeded;
}

static bool scene_apply_restart_events(TecmoGameplayScene *scene,
                                       bool *restart_applied)
{
    size_t event_index;
    if (scene == NULL || restart_applied == NULL) return false;
    *restart_applied = false;
    for (event_index = 0U; event_index < scene->events.count;
         ++event_index) {
        const TecmoGameplayEvent *event = &scene->events.events[event_index];
        TecmoGameplayTeam restart;
        if (event->kind != TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST) {
            continue;
        }
        if (*restart_applied ||
            event->value != TECMO_GAMEPLAY_RESTART_PLAY_ID ||
            event->detail >= TECMO_GAMEPLAY_TEAM_COUNT) {
            return false;
        }
        restart = (TecmoGameplayTeam)event->detail;
        if (scene->state.possession != restart ||
            !scene_handoff_possession(
                scene, restart, scene_first_actor_for_team(restart))) {
            return false;
        }
        *restart_applied = true;
    }
    return true;
}

static bool scene_process_events(TecmoGameplayScene *scene,
                                 bool free_throw_team_captured,
                                 TecmoGameplayTeam free_throw_team)
{
    size_t event_index;
    for (event_index = 0U; event_index < scene->events.count;
         ++event_index) {
        const TecmoGameplayEvent *event = &scene->events.events[event_index];
        switch (event->kind) {
        case TECMO_GAMEPLAY_EVENT_SFX_REQUEST:
            if (event->value == TECMO_GAMEPLAY_SFX_EXPIRY_ID) {
                (void)tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER);
            } else if (event->value == TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID) {
                (void)tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_COUNTDOWN);
            }
            break;
        case TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST:
            if (event->value == TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID &&
                scene->audio_player.music != NULL) {
                (void)tecmo_music_queue_track(
                    scene->audio_player.music,
                    TECMO_MUSIC_TRACK_PRESENTATION);
            }
            break;
        case TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST:
            /* Applied from event.detail before any live action this frame. */
            break;
        case TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED:
            /* The phase transition below owns the single violation request. */
            break;
        case TECMO_GAMEPLAY_EVENT_FREE_THROW_RESULT:
            if (!free_throw_team_captured ||
                !scene_shot_queue_result_audio(scene, free_throw_team)) {
                return false;
            }
            break;
        case TECMO_GAMEPLAY_EVENT_GAME_COMPLETE:
            scene->result.source = scene->launch.source;
            scene->result.game_index = scene->launch.game_index;
            scene->result.away_team = scene->launch.away_team;
            scene->result.home_team = scene->launch.home_team;
            scene->result.away_score =
                scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
            scene->result.home_score =
                scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
            scene->result.overtime_count = scene->state.overtime_count;
            scene->result_ready = true;
            break;
        case TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED:
        case TECMO_GAMEPLAY_EVENT_KIND_COUNT:
        default:
            break;
        }
    }
    return true;
}

static bool scene_phase_requires_audio_reset(TecmoGameplayPhase before,
                                             TecmoGameplayPhase after)
{
    if (before == after) return false;
    if (after == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        after == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
        return true;
    }
    if (before != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT &&
        before != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        return false;
    }
    return after == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
           after == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
           after == TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN;
}

static void scene_apply_phase_audio_reset(TecmoGameplayScene *scene,
                                          TecmoGameplayPhase before)
{
    if (scene != NULL &&
        scene_phase_requires_audio_reset(before, scene->state.phase)) {
        tecmo_gameplay_audio_stop_all(&scene->audio_player);
    }
}

static void scene_process_phase_audio(TecmoGameplayScene *scene,
                                      TecmoGameplayPhase before)
{
    TecmoGameplayPhase after = scene->state.phase;
    if (before == after) return;
    if (after == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        (void)tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE);
    } else if (after == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
               scene->launch.game_music_enabled &&
               (before == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
                before == TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED)) {
        /* The bounded free-throw observation requests gameplay track 5 at
           setup. It does not request the same-numbered Bank05 $9FEC SFX. */
        (void)tecmo_gameplay_audio_queue_game_music(&scene->audio_player);
    } else if (after == TECMO_GAMEPLAY_PHASE_LIVE &&
               scene->launch.game_music_enabled &&
               before != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
               before !=
                   TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED) {
        /* The neutral Bank05 $9FEC cue belongs only to the gated reset/restart
           boundary, never to foul-presentation entry. This function runs once
           per scene frame, so one boundary can queue it at most once. */
        if (before == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            before == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
            before == TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
            before == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER) {
            (void)tecmo_gameplay_audio_queue_event(
                &scene->audio_player,
                TECMO_GAMEPLAY_AUDIO_BANK05_9FEC_CUE);
        }
        (void)tecmo_gameplay_audio_queue_game_music(&scene->audio_player);
    }
}

static bool scene_update_free_throw(TecmoGameplayScene *scene,
                                    const TecmoControlFrame *player_one,
                                    const TecmoControlFrame *player_two)
{
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    size_t controller;
    bool launch_attempt;

    controls[0] = player_one;
    controls[1] = player_two;
    controller = scene_controller_for_team(
        scene, scene->state.free_throws.scoring_team);
    if (controller < TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        /* Bank05's human state-20 path checks the scoring side's current NES B
           level. It does not read the other pad, an edge/release bit, or a
           direction at this gate, and it has no arbitrary timeout. */
        launch_attempt = scene_controls_held_b(controls[controller]);
    } else {
        /* The bounded slot-3 trace spans 125 inclusive updates from CPU
           state-18 entry through launch. The original positioning/script
           system is not modeled by this native scene yet. */
        if (scene->free_throw_frame <
            TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES) {
            ++scene->free_throw_frame;
        }
        launch_attempt = scene->free_throw_frame >=
            TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
    }
    if (!launch_attempt) return true;

    /* Retain the existing implementation-owned serial make/miss policy. The
       launch itself requests no SFX; a made-result event may queue the proven
       crowd response through the normal state-event path below. */
    ++scene->action_serial;
    scene->free_throw_frame = 0U;
    if (!tecmo_gameplay_record_free_throw_result(
            &scene->state,
            (scene->action_serial +
             scene->state.free_throws.attempts_remaining) % 3U != 0U,
            &scene->events)) {
        return false;
    }
    if (scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED) {
        TecmoGameplayTeam next = scene_other_team(
            scene->state.free_throws.scoring_team);
        if (!tecmo_gameplay_settle_free_throws(
                &scene->state, next,
                TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50)) {
            return false;
        }
        if (!scene_handoff_possession(
                scene, next, scene_first_actor_for_team(next))) {
            return false;
        }
        scene_clear_free_throw_lineup_binding(scene);
    }
    return true;
}

static bool scene_phase_allows_live_action(TecmoGameplayPhase phase)
{
    return phase == TECMO_GAMEPLAY_PHASE_LIVE ||
           phase == TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
}

static bool scene_follow_live_camera_once(TecmoGameplayScene *scene)
{
    TecmoGameplayCameraState followed;
    if (scene == NULL) return false;
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        tecmo_gameplay_scene_in_dunk_presentation(scene)) {
        return tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state);
    }
    followed = scene->camera_state;
    if (!tecmo_gameplay_camera_follow_court(
            &scene->camera_assets, &followed, &scene->ball_position,
            scene->orientation_state.current_direction, 0U, false) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &followed)) {
        return false;
    }
    scene->camera_state = followed;
    ++scene->camera_follow_count;
    return true;
}

static bool scene_pretip_cpu_should_sample(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam team)
{
    if (scene == NULL ||
        (team != TECMO_GAMEPLAY_TEAM_AWAY &&
         team != TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene_launch_valid(&scene->launch) ||
        !tecmo_gameplay_pretip_state_validate(
            &scene->pretip_assets, &scene->pretip_state) ||
        scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        scene->pretip_state.contest_frame !=
            TECMO_GAMEPLAY_PRETIP_CPU_SAMPLE_FRAME) {
        return false;
    }
    return scene_controller_for_team(scene, team) >=
           TECMO_GAMEPLAY_CONTROLLER_COUNT;
}

static bool scene_update_pretip_frame(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *player_one,
    const TecmoControlFrame *player_two)
{
    TecmoGameplayPreTipPhase prior_phase = scene->pretip_state.phase;
    bool held_one = player_one != NULL && player_one->held.cancel;
    bool held_two = player_two != NULL && player_two->held.cancel;
    bool pretip_away_held = false;
    bool pretip_home_held = false;

    if (prior_phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD) {
        /* Card cancellation consumes the raw pad levels. */
        pretip_away_held = held_one;
        pretip_home_held = held_two;
    } else if (prior_phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        /* Contest B is team-routed; an unassigned pad contributes nothing. */
        pretip_away_held =
            (scene->launch.controller_team[0] ==
                 TECMO_GAMEPLAY_TEAM_AWAY && held_one) ||
            (scene->launch.controller_team[1] ==
                 TECMO_GAMEPLAY_TEAM_AWAY && held_two);
        pretip_home_held =
            (scene->launch.controller_team[0] ==
                 TECMO_GAMEPLAY_TEAM_HOME && held_one) ||
            (scene->launch.controller_team[1] ==
                 TECMO_GAMEPLAY_TEAM_HOME && held_two);
        /* A team-routed human level always wins this decision seam.  Only a
           team with no assigned controller receives the fixed-frame CPU
           approximation; the presentation arc below remains unconditional
           for both selected jumpers. */
        if (!pretip_away_held) {
            pretip_away_held = scene_pretip_cpu_should_sample(
                scene, TECMO_GAMEPLAY_TEAM_AWAY);
        }
        if (!pretip_home_held) {
            pretip_home_held = scene_pretip_cpu_should_sample(
                scene, TECMO_GAMEPLAY_TEAM_HOME);
        }
    }
    if (!tecmo_gameplay_pretip_update(
            &scene->pretip_assets, &scene->pretip_state,
            pretip_away_held, pretip_home_held)) {
        scene_set_status(scene, "pre-tip update rejected");
        return false;
    }
    if (scene->pretip_state.aborted) {
        scene->pretip_abort_pending = true;
        scene->active = false;
        tecmo_gameplay_audio_stop_all(&scene->audio_player);
        scene_set_status(scene, "pre-tip aborted by NES B");
        return true;
    }
    if (scene->pretip_state.phase ==
            TECMO_GAMEPLAY_PRETIP_BALL_DESCENT) {
        scene->ball_position.y_q8 = scene_pretip_descent_ball_y_q8(
            scene->pretip_state.phase_frame);
    } else if (scene->pretip_state.phase ==
               TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        uint16_t frame = scene->pretip_state.phase_frame;
        uint8_t winner;
        if (!scene_pretip_apply_jump_frame(scene, frame)) {
            scene_set_status(scene, "pre-tip jump presentation rejected");
            return false;
        }
        scene->ball_position.y_q8 = (int32_t)(72U + frame) * 256;
        if (tecmo_gameplay_pretip_tip_winner(
                &scene->pretip_assets, &scene->pretip_state, &winner)) {
            uint8_t error =
                winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER
                    ? scene->pretip_state.home_tip_error
                    : scene->pretip_state.away_tip_error;
            uint16_t contact = (uint16_t)error + 8U;
            uint16_t travel = frame > contact
                                  ? (uint16_t)(frame - contact) : 0U;
            if (travel > 8U) travel = 8U;
            scene->ball_position.x_q8 =
                (int32_t)(384 +
                    (winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER
                         ? (int)travel : -(int)travel)) * 256;
        }
    } else if (prior_phase == TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
        scene->ball_position.y_q8 =
            (int32_t)(108U - scene->pretip_state.phase_frame) * 256;
    }
    ++scene->frame;
    if (scene->pretip_state.live_handoff) {
        TecmoGameplayTeam possession;
        uint8_t winner;
        uint8_t holder;
        if (!scene_pretip_land_jump(scene)) {
            scene_set_status(scene, "pre-tip jump landing rejected");
            return false;
        }
        if (!tecmo_gameplay_pretip_tip_winner(
                &scene->pretip_assets, &scene->pretip_state, &winner)) {
            scene_set_status(scene, "pre-tip winner handoff rejected");
            return false;
        }
        possession = winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER
                         ? TECMO_GAMEPLAY_TEAM_HOME
                         : TECMO_GAMEPLAY_TEAM_AWAY;
        holder = possession == TECMO_GAMEPLAY_TEAM_HOME
                     ? TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT : 0U;
        if (!scene_initialize_actors(scene)) {
            scene_set_status(
                scene, "pre-tip actor movement handoff rejected");
            return false;
        }
        if (!scene_handoff_possession(scene, possession, holder)) {
            scene_set_status(scene, "pre-tip possession handoff rejected");
            return false;
        }
        if (!tecmo_gameplay_camera_settle_court(
                &scene->camera_assets, &scene->camera_state,
                &scene->ball_position,
                scene->orientation_state.current_direction, false)) {
            scene_set_status(scene, "pre-tip live camera handoff rejected");
            return false;
        }
        if (scene->launch.game_music_enabled &&
            !tecmo_gameplay_audio_queue_game_music(
                &scene->audio_player)) {
            scene_set_status(scene, "gameplay track 5 handoff rejected");
            return false;
        }
        scene_set_status(scene, "native gameplay active");
    }
    {
        TecmoGameplaySceneCourtCoordinates coordinates;
        if (!tecmo_gameplay_scene_court_coordinates(scene, &coordinates)) {
            scene_set_status(scene, "pre-tip court coordinates rejected");
            return false;
        }
    }
    return true;
}

static bool scene_advance_state_and_restarts(
    TecmoGameplayScene *scene,
    const TecmoGameplayFrameInput *input,
    const TecmoGameplayLiveContext *live_context,
    TecmoGameplayPhase *phase_before_out,
    bool *free_throw_team_captured_out,
    TecmoGameplayTeam *captured_free_throw_team_out,
    bool *restart_frame_out)
{
    TecmoGameplayPhase phase_before = scene->state.phase;
    TecmoGameplayTeam captured_free_throw_team =
        TECMO_GAMEPLAY_TEAM_AWAY;
    bool free_throw_team_captured =
        phase_before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE;
    bool restart_applied;

    if (free_throw_team_captured) {
        captured_free_throw_team = scene->state.free_throws.scoring_team;
    }
    if (!tecmo_gameplay_update(&scene->state, input, live_context,
                               &scene->events)) {
        scene_set_status(scene, "gameplay state update rejected");
        return false;
    }
    if (!scene_apply_restart_events(scene, &restart_applied)) {
        scene_set_status(scene, "gameplay restart event rejected");
        return false;
    }
    if (phase_before == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        !restart_applied) {
        scene_set_status(scene, "gameplay restart event missing");
        return false;
    }
    if (phase_before == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        !scene_handoff_possession(
            scene, scene->state.possession,
            scene_first_actor_for_team(scene->state.possession))) {
        scene_set_status(scene, "period restart synchronization rejected");
        return false;
    }
    if (phase_before == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE) {
        restart_applied = true;
    }
    *phase_before_out = phase_before;
    *free_throw_team_captured_out = free_throw_team_captured;
    *captured_free_throw_team_out = captured_free_throw_team;
    *restart_frame_out = restart_applied;
    return true;
}

static bool scene_update_live_action_ordered(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT],
    TecmoGameplaySceneCpuShotRequest *cpu_shot_request,
    bool *jump_miss_settled_out,
    TecmoGameplayTeam *jump_miss_shooting_team_out)
{
    bool boundary_settled = false;
    size_t controller;

    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        bool terminal_jump_miss =
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
            !scene->jump_make_route &&
            scene->shot_frame + 1U == scene->shot_duration &&
            scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
        const TecmoControlFrame *shooting_controls =
            scene->shot_controller < TECMO_GAMEPLAY_CONTROLLER_COUNT
                ? controls[scene->shot_controller]
                : NULL;
        if (terminal_jump_miss) {
            *jump_miss_shooting_team_out =
                (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
        }
        if (!scene_update_shot(scene, shooting_controls)) {
            scene_set_status(scene, "shot animation update rejected");
            return false;
        }
        *jump_miss_settled_out = terminal_jump_miss &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    } else if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE) {
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
             ++controller) {
            if (!scene_move_controlled_actor(
                    scene, controller, controls[controller])) {
                scene_set_status(
                    scene, "controlled movement update rejected");
                return false;
            }
        }
        if (!scene_settle_boundary_latch(scene, &boundary_settled)) {
            scene_set_status(
                scene, "out-of-bounds settlement rejected");
            return false;
        }
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
                 !boundary_settled;
             ++controller) {
            if (scene_controls_pressed_a(controls[controller])) {
                if (!scene_pass_or_switch(scene, controller)) {
                    scene_set_status(
                        scene, "pass ball coordinate rejected");
                    return false;
                }
            }
        }
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
                 !boundary_settled;
             ++controller) {
            if (scene_controls_pressed_b(controls[controller]) &&
                controls[controller] != NULL &&
                controls[controller]->held.cancel &&
                scene->launch.controller_team[controller] ==
                    scene->state.possession &&
                scene_start_shot(scene, controller)) {
                break;
            }
        }
        if (!boundary_settled &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            for (controller = 0U;
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
                 ++controller) {
                if (!scene_controls_pressed_b(controls[controller]) ||
                    scene->launch.controller_team[controller] ==
                        TECMO_GAMEPLAY_SCENE_NO_TEAM ||
                    scene->launch.controller_team[controller] ==
                        scene->state.possession) {
                    continue;
                }
                if (scene_try_defense_action(scene, controller)) continue;
                scene_set_status(scene, "defensive action rejected");
                return false;
            }
        }
        if (!boundary_settled &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !scene_update_ai(scene, cpu_shot_request)) {
            scene_set_status(scene, "native offense update rejected");
            return false;
        }
        if (!boundary_settled && cpu_shot_request->requested) {
            /* CPU policy reports the same launch decision as before; the
               orchestrator owns the transition into shot playback. */
            (void)scene_start_shot_actor(
                scene, 0U, cpu_shot_request->actor_index);
        }
        if (!boundary_settled &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !scene_settle_boundary_latch(scene, &boundary_settled)) {
            scene_set_status(
                scene, "CPU out-of-bounds settlement rejected");
            return false;
        }
        if (!boundary_settled &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            TecmoGameplayBallDribbleFrame dribble = {0};
            if (!scene_attach_ball(scene)) {
                scene_set_status(
                    scene, "held ball coordinate rejected");
                return false;
            }
            if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
                !scene_settle_backcourt(scene, &boundary_settled)) {
                scene_set_status(
                    scene, "backcourt settlement rejected");
                return false;
            }
            if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
                !boundary_settled &&
                !scene_live_ball_frame_for_actors(
                    scene, scene->actors, scene->ball_holder,
                    &dribble)) {
                scene_set_status(
                    scene, "held ball animation rejected");
                return false;
            }
            if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
                !boundary_settled && dribble.sound_trigger) {
                (void)tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE);
            }
        }
    }
    return true;
}

static bool scene_update_post_action_phases(
    TecmoGameplayScene *scene,
    TecmoGameplayPhase phase_before,
    bool restart_frame,
    const TecmoControlFrame *player_one,
    const TecmoControlFrame *player_two)
{
    if (scene_phase_allows_live_action(phase_before) && !restart_frame &&
        !scene_tick_fatigue(scene)) {
        scene_set_status(scene, "gameplay fatigue update rejected");
        return false;
    }

    if (scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED) {
        if (!scene_settle_foul_with_free_throw_lineup(scene)) {
            scene_set_status(
                scene,
                "foul settlement/free-throw lineup rejected");
            return false;
        }
    }
    if (phase_before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
        !scene_update_free_throw(scene, player_one, player_two)) {
        scene_set_status(scene, "free-throw settlement rejected");
        return false;
    }
    return true;
}

static bool scene_dispatch_update_audio(
    TecmoGameplayScene *scene,
    TecmoGameplayPhase phase_before,
    bool free_throw_team_captured,
    TecmoGameplayTeam captured_free_throw_team,
    bool jump_miss_settled,
    TecmoGameplayTeam jump_miss_shooting_team)
{
    /* Fixed $EC06-style phase clears happen once, before a replacement event
       or cue can populate the native mailboxes. */
    scene_apply_phase_audio_reset(scene, phase_before);
    if (!scene_process_events(scene, free_throw_team_captured,
                              captured_free_throw_team)) {
        scene_set_status(scene, "gameplay audio event rejected");
        return false;
    }
    /* Qualifying restart-boundary audio remains last so an event emitted
       during the settling action cannot overwrite the one-shot Bank05 $9FEC
       mailbox. Final free throws are deliberately not qualifying returns. */
    scene_process_phase_audio(scene, phase_before);
    if (jump_miss_settled &&
        !scene_shot_queue_result_audio(scene, jump_miss_shooting_team)) {
        scene_set_status(scene, "jump-miss result audio rejected");
        return false;
    }
    return true;
}

bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two)
{
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext live_context;
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayPhase phase_before;
    TecmoGameplayTeam captured_free_throw_team = TECMO_GAMEPLAY_TEAM_AWAY;
    bool restart_frame;
    bool free_throw_team_captured;
    bool jump_miss_settled = false;
    TecmoGameplayTeam jump_miss_shooting_team = TECMO_GAMEPLAY_TEAM_AWAY;
    TecmoGameplaySceneCpuShotRequest cpu_shot_request = {
        false, TECMO_GAMEPLAY_SCENE_NO_ACTOR
    };

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene->active || scene->result_ready ||
        scene->pretip_abort_pending) {
        return false;
    }
    if (!tecmo_gameplay_pretip_state_validate(
            &scene->pretip_assets, &scene->pretip_state)) {
        scene_set_status(scene, "pre-tip state contract rejected");
        return false;
    }
    if (tecmo_gameplay_pretip_is_presentation(&scene->pretip_state)) {
        return scene_update_pretip_frame(scene, player_one, player_two);
    }
    if (!scene_ownership_valid(scene)) return false;
    controls[0] = player_one;
    controls[1] = player_two;
    tecmo_gameplay_frame_input_clear(&input);
    scene_pad_from_controls(&input.controllers[0], player_one);
    scene_pad_from_controls(&input.controllers[1], player_two);
    tecmo_gameplay_live_context_default(&live_context);
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        live_context.period_expiry =
            TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
        live_context.shot_clock_violation_exempt = true;
    } else if (scene->state.phase ==
               TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        live_context.period_expiry =
            TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    }

    if (!scene_advance_state_and_restarts(
            scene, &input, &live_context, &phase_before,
            &free_throw_team_captured, &captured_free_throw_team,
            &restart_frame)) {
        return false;
    }
    if (scene_phase_allows_live_action(scene->state.phase) &&
        !restart_frame &&
        !scene_update_live_action_ordered(
            scene, controls, &cpu_shot_request, &jump_miss_settled,
            &jump_miss_shooting_team)) {
        return false;
    }
    if (!scene_update_post_action_phases(
            scene, phase_before, restart_frame, player_one, player_two)) {
        return false;
    }
    if (!scene_dispatch_update_audio(
            scene, phase_before, free_throw_team_captured,
            captured_free_throw_team, jump_miss_settled,
            jump_miss_shooting_team)) {
        return false;
    }
    if (!scene_follow_live_camera_once(scene)) {
        scene_set_status(scene, "gameplay live camera update rejected");
        return false;
    }
    if (!scene_ownership_valid(scene)) {
        scene_set_status(scene, "gameplay ownership invariant rejected");
        return false;
    }
    scene->previous_phase = scene->state.phase;
    ++scene->frame;
    return true;
}
bool tecmo_gameplay_scene_result(const TecmoGameplayScene *scene,
                                 TecmoGameplaySceneResult *result)
{
    if (scene == NULL || result == NULL || !scene->result_ready ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return false;
    }
    *result = scene->result;
    return true;
}

bool tecmo_gameplay_scene_free_throw_lineup(
    const TecmoGameplayScene *scene,
    TecmoGameplayFreeThrowLineup *lineup_out)
{
    TecmoGameplayFreeThrowLineup lineup;
    if (lineup_out == NULL ||
        !scene_court_free_throw_lineup_matches(scene) ||
        !tecmo_gameplay_free_throw_lineup_derive(
            &scene->free_throw_lineup_assets,
            scene->free_throw_lineup_orientation,
            scene->free_throw_shooter,
            scene->free_throw_secondary, &lineup)) {
        return false;
    }
    *lineup_out = lineup;
    return true;
}

bool tecmo_gameplay_scene_consume_pretip_abort(TecmoGameplayScene *scene)
{
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->pretip_abort_pending) {
        return false;
    }
    scene->pretip_abort_pending = false;
    return true;
}

void tecmo_gameplay_scene_end(TecmoGameplayScene *scene)
{
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return;
    }
    scene->active = false;
    scene->result_ready = false;
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->free_throw_frame = 0U;
    scene_clear_free_throw_lineup_binding(scene);
    scene_shot_clear_jump_playback(scene);
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    scene_set_status(scene, scene->available
                                ? "native gameplay ready"
                                : "gameplay assets unavailable");
}

const char *tecmo_gameplay_scene_shot_name(TecmoGameplaySceneShotKind kind)
{
    switch (kind) {
    case TECMO_GAMEPLAY_SCENE_SHOT_NONE: return "none";
    case TECMO_GAMEPLAY_SCENE_SHOT_JUMP: return "jump";
    case TECMO_GAMEPLAY_SCENE_SHOT_DUNK: return "dunk";
    case TECMO_GAMEPLAY_SCENE_SHOT_LAYUP: return "layup";
    case TECMO_GAMEPLAY_SCENE_SHOT_KIND_COUNT:
    default:
        return "invalid";
    }
}

#include "tecmo_gameplay_scene_test_internal.h"

void tecmo_gameplay_scene_test_set_skip_pretip(bool skip)
{
    scene_self_test_skip_pretip = skip;
}

bool tecmo_gameplay_scene_test_follow_live_camera_once(
    TecmoGameplayScene *scene)
{
    return scene_follow_live_camera_once(scene);
}

uint32_t tecmo_gameplay_scene_test_pixels_fnv1a32(
    const uint32_t *pixels,
    size_t pixel_count)
{
    return scene_pixels_fnv1a32(pixels, pixel_count);
}

bool tecmo_gameplay_scene_self_test(const char *project_root,
                                    const char *asset_pack_path,
                                    TecmoMusicPlayer *music_player,
                                    char *message,
                                    size_t message_size)
{
    return tecmo_gameplay_scene_test_orchestrate(
        project_root, asset_pack_path, music_player, message, message_size);
}
