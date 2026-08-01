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
        item->facing_right = item->team == TECMO_GAMEPLAY_TEAM_AWAY;
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
    if (scene == NULL ||
        !tecmo_gameplay_pretip_tip_lineup(
            &scene->pretip_assets, &lineup) ||
        lineup.ball_sprite_slot_base != 0xC1U ||
        lineup.ball_pose_index != TECMO_GAMEPLAY_BALL_POSE) {
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

static uint16_t scene_pretip_jump_arc(uint16_t phase_frame,
                                      uint8_t timing_error)
{
    uint16_t active;
    if (phase_frame <= timing_error) return 0U;
    active = (uint16_t)(phase_frame - timing_error);
    if (active > 30U) return 0U;
    return active <= 15U ? active : (uint16_t)(30U - active);
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
        for (phase = 0U; phase < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++phase)
            scene->pretip_state.total_frame +=
                scene->pretip_assets.phase_frames[phase];
        scene->pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
        scene->pretip_state.phase_frame = 0U;
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
    /*
     * TGFL does not define the shooter's pose. Keep the existing pose and use
     * only the binary hoop direction for the native held-ball attachment.
     */
    candidate_actors[shooter].facing_right =
        scene->orientation_state.current_direction != 0U;
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

static bool scene_update_pretip_frame(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *player_one,
    const TecmoControlFrame *player_two)
{
    TecmoGameplayPreTipPhase prior_phase = scene->pretip_state.phase;
    bool held_one = player_one != NULL && player_one->held.cancel;
    bool held_two = player_two != NULL && player_two->held.cancel;
    bool pretip_away_held = held_one;
    bool pretip_home_held = held_two;

    if (prior_phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
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
    } else if (prior_phase == TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
        scene->ball_position.y_q8 =
            (int32_t)(108U - scene->pretip_state.phase_frame) * 256;
    } else if (prior_phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        uint16_t frame = scene->pretip_state.phase_frame;
        uint16_t away_arc = scene_pretip_jump_arc(
            frame, scene->pretip_state.away_tip_error);
        uint16_t home_arc = scene_pretip_jump_arc(
            frame, scene->pretip_state.home_tip_error);
        uint8_t winner;
        scene->actors[4].position.y =
            (int16_t)(scene->actors[4].anchor.y - away_arc / 2U);
        scene->actors[9].position.y =
            (int16_t)(scene->actors[9].anchor.y - home_arc / 2U);
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
    }
    ++scene->frame;
    if (scene->pretip_state.live_handoff) {
        TecmoGameplayTeam possession;
        uint8_t winner;
        uint8_t holder;
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

static bool scene_build_background_context(
    const TecmoGameplayScene *scene,
    TecmoGameplayLiveBackgroundContext *context)
{
    return tecmo_gameplay_scene_render_build_background_context(scene, context);
}

static bool scene_prepare_live_hud(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayPreparedHud *prepared)
{
    return tecmo_gameplay_scene_render_prepare_live_hud(
        scene, context, prepared);
}

static bool scene_resolve_pose(
    const TecmoGameplayScene *scene,
    uint16_t pointer_index,
    uint8_t actor_slot_base,
    uint8_t actor_attributes,
    uint8_t palette_group,
    bool apply_uniform_color,
    uint8_t uniform_color,
    TecmoGameplayResolvedPose *pose)
{
    return tecmo_gameplay_scene_render_resolve_pose(
        scene, pointer_index, actor_slot_base, actor_attributes,
        palette_group, apply_uniform_color, uniform_color, pose);
}

static bool scene_resolve_actor_pose(
    const TecmoGameplayScene *scene,
    size_t actor_index,
    TecmoGameplayResolvedPose *pose)
{
    return tecmo_gameplay_scene_render_resolve_actor_pose(
        scene, actor_index, pose);
}

static void scene_test_message(char *message, size_t message_size,
                               const char *text)
{
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size, "%s", text);
    }
}

static bool scene_test_live_hud_contract(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayLiveBackgroundContext context;
    TecmoGameplayPreparedHud prepared;
    TecmoGameplayPreparedHud dynamic_prepared;
    TecmoGameplayScene dynamic;
    size_t row;
    size_t column;
    size_t occupied[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT] = {0U, 0U};
    const uint8_t *font;
    unsigned char selected_initial;
    uint8_t selected_roster;
    uint8_t selected_number_bcd;
    if (scene == NULL || !scene_build_background_context(scene, &context) ||
        !scene_prepare_live_hud(scene, &context, &prepared)) {
        return false;
    }
    font = scene->hud_assets.font_tiles;
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (!prepared.occupied[row][column]) continue;
            ++occupied[row];
            if (prepared.chr_offsets[row][column] >
                    scene->assets.chr_storage_size ||
                scene->assets.chr_storage_size -
                        prepared.chr_offsets[row][column] < 16U) {
                return false;
            }
        }
    }
    selected_roster = scene->actors[scene->controlled_actor[0U]].roster_index;
    selected_initial = (unsigned char)
        scene->pretip_team_data
            ->players[scene->launch.away_team][selected_roster].name[0U];
    selected_number_bcd = scene->pretip_team_data
        ->players[scene->launch.away_team][selected_roster].attributes[1U];
    if ((selected_number_bcd >> 4U) > 9U ||
        (selected_number_bcd & 0x0FU) > 9U ||
        occupied[0U] != TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        occupied[1U] != TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        memcmp(&prepared.tiles[0U][1U],
               scene->hud_assets.team_label_tiles[scene->launch.away_team],
               TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) != 0 ||
        memcmp(&prepared.tiles[0U][23U],
               scene->hud_assets.team_label_tiles[scene->launch.home_team],
               TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) != 0 ||
        prepared.tiles[0U][6U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][13U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][14U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[0U][15U] != TECMO_GAMEPLAY_HUD_COLON_TILE ||
        prepared.tiles[1U][1U] !=
            font['0' + (selected_number_bcd >> 4U) -
                 TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][2U] !=
            font['0' + (selected_number_bcd & 0x0FU) -
                 TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][4U] !=
            font[selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][5U] !=
            font['.' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.chr_offsets[1U][4U] !=
            scene->pretip_team_data->font[
                selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST]
                .chr_offset ||
        prepared.tiles[1U][15U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][16U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][19U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][31U] !=
            font[' ' - TECMO_GAMEPLAY_HUD_FONT_FIRST]) {
        return false;
    }

    dynamic = *scene;
    dynamic.state.score[TECMO_GAMEPLAY_TEAM_AWAY] = 123U;
    dynamic.state.score[TECMO_GAMEPLAY_TEAM_HOME] = UINT16_MAX;
    dynamic.state.clock_minutes = 1U;
    dynamic.state.clock_seconds = 23U;
    dynamic.state.shot_clock = 9U;
    if (!scene_build_background_context(&dynamic, &context) ||
        !scene_prepare_live_hud(
            &dynamic, &context, &dynamic_prepared) ||
        dynamic_prepared.tiles[0U][6U] !=
            font['1' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][7U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][8U] !=
            font['3' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][13U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][14U] !=
            font['1' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][16U] !=
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][17U] !=
            font['3' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][28U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][29U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[0U][30U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        memcmp(dynamic_prepared.tiles[1U], prepared.tiles[1U],
               TECMO_GAMEPLAY_HUD_COLUMN_COUNT) != 0) {
        return false;
    }

    dynamic = *scene;
    dynamic.ball_holder = 1U;
    dynamic.controlled_actor[0U] = 1U;
    if (!scene_attach_ball(&dynamic) ||
        !scene_build_background_context(&dynamic, &context) ||
        !scene_prepare_live_hud(
            &dynamic, &context, &dynamic_prepared)) {
        return false;
    }
    selected_roster = dynamic.actors[1U].roster_index;
    selected_initial = (unsigned char)
        dynamic.pretip_team_data
            ->players[dynamic.launch.away_team][selected_roster].name[0U];
    selected_number_bcd = dynamic.pretip_team_data
        ->players[dynamic.launch.away_team][selected_roster].attributes[1U];
    return (selected_number_bcd >> 4U) <= 9U &&
           (selected_number_bcd & 0x0FU) <= 9U &&
           dynamic_prepared.tiles[1U][1U] ==
               font['0' + (selected_number_bcd >> 4U) -
                    TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.tiles[1U][2U] ==
               font['0' + (selected_number_bcd & 0x0FU) -
                    TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.tiles[1U][4U] ==
               font[selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST] &&
           dynamic_prepared.chr_offsets[1U][4U] ==
               dynamic.pretip_team_data->font[
                   selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST]
                   .chr_offset;
}

static bool scene_test_live_hud_equal(
    const TecmoGameplayScene *left,
    const TecmoGameplayScene *right)
{
    TecmoGameplayLiveBackgroundContext left_context;
    TecmoGameplayLiveBackgroundContext right_context;
    TecmoGameplayPreparedHud left_hud;
    TecmoGameplayPreparedHud right_hud;
    if (left == NULL || right == NULL ||
        !scene_build_background_context(left, &left_context) ||
        !scene_build_background_context(right, &right_context) ||
        !scene_prepare_live_hud(left, &left_context, &left_hud) ||
        !scene_prepare_live_hud(right, &right_context, &right_hud)) {
        return false;
    }
    return memcmp(&left_hud, &right_hud, sizeof(left_hud)) == 0;
}

static bool scene_test_projection_is_neutral(
    const TecmoGameplayActorProjection *projection)
{
    return projection != NULL && !projection->visible &&
           projection->screen_x == 0U && projection->screen_y == 0U;
}

static bool scene_test_stationary_projection_transition(
    const TecmoGameplaySceneCourtFrame *before,
    const TecmoGameplaySceneCourtFrame *after)
{
    int camera_delta;
    size_t actor;
    if (before == NULL || after == NULL ||
        before->contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        after->contract_tag != TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        before->slice.viewport.camera_x != before->projection.camera_x ||
        after->slice.viewport.camera_x != after->projection.camera_x) {
        return false;
    }
    camera_delta =
        (int)after->projection.camera_x -
        (int)before->projection.camera_x;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplayActorProjection *old_projection =
            &before->projection.players[actor];
        const TecmoGameplayActorProjection *new_projection =
            &after->projection.players[actor];
        if ((!old_projection->visible &&
             !scene_test_projection_is_neutral(old_projection)) ||
            (!new_projection->visible &&
             !scene_test_projection_is_neutral(new_projection))) {
            return false;
        }
        if (old_projection->visible && new_projection->visible &&
            ((int)new_projection->screen_x + camera_delta !=
                 (int)old_projection->screen_x ||
             new_projection->screen_y != old_projection->screen_y)) {
            return false;
        }
    }
    if ((!before->projection.ball.visible &&
         !scene_test_projection_is_neutral(
             &before->projection.ball)) ||
        (!after->projection.ball.visible &&
         !scene_test_projection_is_neutral(
             &after->projection.ball))) {
        return false;
    }
    return !before->projection.ball.visible ||
           !after->projection.ball.visible ||
           ((int)after->projection.ball.screen_x + camera_delta ==
                (int)before->projection.ball.screen_x &&
            after->projection.ball.screen_y ==
                before->projection.ball.screen_y);
}

static bool scene_test_pixels_equal(const uint32_t *pixels,
                                    size_t pixel_count,
                                    uint32_t expected)
{
    size_t pixel;
    if (pixels == NULL) return false;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != expected) return false;
    }
    return true;
}

static bool scene_test_outer_margin_equal(const uint32_t *pixels,
                                          int width,
                                          int height,
                                          int pitch,
                                          int origin_x,
                                          int origin_y,
                                          int view_width,
                                          int view_height,
                                          uint32_t expected)
{
    int y;
    int x;
    if (pixels == NULL || width <= 0 || height <= 0 || pitch < width ||
        origin_x < 0 || origin_y < 0 || view_width <= 0 || view_height <= 0 ||
        origin_x + view_width > width || origin_y + view_height > height) {
        return false;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            bool inside = x >= origin_x && x < origin_x + view_width &&
                          y >= origin_y && y < origin_y + view_height;
            if (!inside && pixels[(size_t)y * (size_t)pitch + (size_t)x] !=
                               expected) {
                return false;
            }
        }
    }
    return true;
}

static bool scene_test_draw_exact_step(const TecmoGameplayScene *scene)
{
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    TecmoFramebuffer framebuffer;
    uint32_t *pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    bool drawn;
    if (pixels == NULL) return false;
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    drawn = tecmo_gameplay_scene_draw(scene, &framebuffer, 0, 0, 1, true);
    free(pixels);
    return drawn;
}

static bool scene_test_has_close_semantic_event(
    const TecmoGameplayEventBuffer *events)
{
    size_t event_index;
    if (events == NULL) return false;
    for (event_index = 0U; event_index < events->count; ++event_index) {
        if (events->events[event_index].kind ==
            TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED) {
            return true;
        }
    }
    return false;
}

static bool scene_test_close_semantic_chain_untouched(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplayCloseShotState *shot;
    if (scene == NULL) return false;
    shot = &scene->state.close_shot_subtype01;
    return shot->phase == TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL &&
           shot->observation == TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY &&
           shot->observed_actor_pose_index == UINT16_MAX &&
           shot->observed_ball_pose_index == UINT16_MAX &&
           shot->transition_serial == 0U &&
           !shot->observed_pose_available && !shot->active;
}

static bool scene_test_jump_slot0_checkpoint(
    const TecmoGameplayScene *scene, uint16_t frame)
{
    if (scene == NULL || frame < 3U || frame >= 87U ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != frame || !scene->jump_oracle_active ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        !scene->jump_b_released ||
        (frame < 46U &&
         (scene->jump_pose_frame !=
              TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME ||
          scene->actors[scene->shot_actor].pose_index !=
              TECMO_GAMEPLAY_JUMP_FLIGHT_POSE)) ||
        (frame >= 46U &&
         (scene->jump_pose_frame != 0U ||
          scene->actors[scene->shot_actor].pose_index !=
              TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE))) {
        return false;
    }
    switch (frame) {
    case 3U:
        return scene->jump_actor_altitude_q8 == 0x02E8U &&
               scene->jump_actor_velocity_q8 == 0x02E8U &&
               scene->jump_actor_state == 0x0DU &&
               scene->jump_ball_state == 0x05U;
    case 4U:
        return scene->jump_actor_altitude_q8 == 0x05A8U &&
               scene->jump_actor_velocity_q8 == 0x02C0U &&
               !scene->jump_actor_landed;
    case 5U:
        return scene->jump_ball_state == 0x17U;
    case 21U:
        return scene->jump_actor_altitude_q8 == 0x1C80U &&
               scene->jump_actor_velocity_q8 == 0x0018U;
    case 22U:
        return scene->jump_actor_altitude_q8 == 0x1C70U &&
               scene->jump_actor_velocity_q8 == 0xFFF0U;
    case 39U:
        return !scene->jump_actor_landed &&
               scene->jump_actor_altitude_q8 == 0x0378U &&
               scene->jump_actor_velocity_q8 == 0xFD48U &&
               scene->jump_actor_state == 0x0DU;
    case 40U:
        return scene->jump_actor_landed &&
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0U &&
               scene->jump_actor_state == 0x0EU &&
               scene->jump_phase_counter == 0x56U;
    case 41U: return scene->jump_phase_counter == 0x46U;
    case 42U: return scene->jump_phase_counter == 0x36U;
    case 43U: return scene->jump_phase_counter == 0x26U;
    case 44U: return scene->jump_phase_counter == 0x16U;
    case 45U: return scene->jump_phase_counter == 0x06U;
    case 46U:
        return scene->jump_actor_state == 0x00U &&
               scene->jump_phase_counter == 0U &&
               scene->actors[scene->shot_actor].pose_index == 469U;
    case 72U: return scene->jump_ball_state == 0x17U;
    case 73U:
        return scene->jump_ball_state == 0x10U &&
               scene->jump_ball_bounce_q8 == 0U;
    case 74U:
        return scene->jump_ball_state == 0x10U &&
               scene->jump_ball_altitude_q8 == 0U &&
               scene->jump_ball_bounce_q8 == 0x0080U &&
               !scene->audio_player.dmc.active;
    case 75U:
        return scene->jump_ball_state == 0x10U &&
               scene->jump_ball_bounce_q8 == 0U &&
               scene->audio_player.dmc.active;
    case 86U: return scene->jump_ball_state == 0x10U;
    default: return true;
    }
}

static bool scene_test_jump_rattle_checkpoint(
    const TecmoGameplayScene *scene, uint16_t frame)
{
    const TecmoGameplayShotRimRattle *rattle;
    if (scene == NULL || frame < 2U || frame >= 103U ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != frame || !scene->jump_oracle_active ||
        !scene->jump_rim_rattle_debug ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }
    rattle = &scene->jump_rim_rattle;
    switch (frame) {
    case 72U:
        return scene->jump_ball_state == 0x17U &&
               !rattle->active && !rattle->complete;
    case 73U:
        return scene->jump_ball_state == 0x15U &&
               scene->shot_end_position.x_q8 ==
                   TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256 &&
               scene->shot_end_position.y_q8 ==
                   TECMO_GAMEPLAY_SHOT_TARGET_Y * 256 &&
               scene->ball_position.x_q8 == 0x009D * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->active && !rattle->complete &&
               rattle->x == 0x009D && rattle->y == 0x0093 &&
               rattle->horizontal_velocity_q6 == 0x0040 &&
               rattle->vertical_velocity_q6 == 0 &&
               rattle->timer_remaining == 4U &&
               rattle->passes_remaining == 4U &&
               rattle->animation_phase == 0x40U &&
               rattle->render_script_address == 0xBAB9U &&
               scene->jump_rim_rattle_audio_repeats == 0U;
    case 74U:
        return scene->jump_ball_state == 0x15U &&
               scene->ball_position.x_q8 == 0x009E * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->x == 0x009E &&
               rattle->timer_remaining == 3U &&
               rattle->render_script_address == 0xBAB9U;
    case 77U:
        return scene->jump_ball_state == 0x15U &&
               scene->ball_position.x_q8 == 0x00A1 * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->x == 0x00A1 &&
               rattle->horizontal_velocity_q6 == -0x0040 &&
               rattle->timer_remaining == 4U &&
               rattle->passes_remaining == 3U &&
               rattle->animation_phase == 0x30U &&
               rattle->render_script_address == 0xBAB9U &&
               scene->jump_rim_rattle_audio_repeats == 1U;
    case 81U:
        return scene->jump_ball_state == 0x15U &&
               scene->ball_position.x_q8 == 0x009D * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->x == 0x009DU &&
               rattle->horizontal_velocity_q6 == 0x0040 &&
               rattle->passes_remaining == 2U &&
               rattle->animation_phase == 0x20U &&
               scene->jump_rim_rattle_audio_repeats == 2U;
    case 85U:
        return scene->jump_ball_state == 0x15U &&
               scene->ball_position.x_q8 == 0x00A1 * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->x == 0x00A1 &&
               rattle->horizontal_velocity_q6 == -0x0040 &&
               rattle->passes_remaining == 1U &&
               rattle->animation_phase == 0x10U &&
               scene->jump_rim_rattle_audio_repeats == 3U;
    case 88U:
        return scene->jump_ball_state == 0x15U &&
               scene->ball_position.x_q8 == 0x009E * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               rattle->x == 0x009EU &&
               rattle->timer_remaining == 1U &&
               rattle->render_script_address == 0xBAB9U;
    case 89U:
        return scene->jump_ball_state == 0x10U &&
               scene->ball_position.x_q8 == 0x009D * 256 &&
               scene->ball_position.y_q8 == 0x0093 * 256 &&
               !rattle->active && rattle->complete &&
               rattle->x == 0x009DU &&
               rattle->passes_remaining == 0U &&
               rattle->animation_phase == 0U &&
               rattle->horizontal_velocity_q6 ==
                   TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6 &&
               rattle->vertical_velocity_q6 == 0 &&
               rattle->render_script_address == 0xBADDU &&
               scene->jump_rim_rattle_audio_repeats == 3U;
    case 90U:
        return scene->jump_ball_state == 0x10U &&
               scene->ball_position.x_q8 ==
                   TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256 &&
               scene->ball_position.y_q8 ==
                   TECMO_GAMEPLAY_SHOT_TARGET_Y * 256 &&
               scene->jump_ball_altitude_q8 == 0U &&
               scene->jump_ball_bounce_q8 == 0x0080U;
    case 91U:
        return scene->jump_ball_state == 0x10U &&
               scene->jump_ball_bounce_q8 == 0U &&
               scene->audio_player.dmc.active;
    case 102U:
        return scene->jump_ball_state == 0x10U &&
               scene->jump_rim_rattle_audio_repeats == 3U;
    default:
        return true;
    }
}

static bool scene_test_jump_make_checkpoint(
    const TecmoGameplayScene *scene, uint16_t frame)
{
    if (scene == NULL || frame == 0U ||
        frame >= TECMO_GAMEPLAY_JUMP_MAKE_DURATION ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != frame || !scene->jump_oracle_active ||
        !scene->jump_make_route) {
        return false;
    }
    switch (frame) {
    case 1U:
        return !scene->jump_b_released &&
               scene->jump_outcome ==
                   TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
               scene->jump_actor_state == 0x1EU &&
               scene->jump_phase_counter == 0x30U &&
               scene->jump_pose_frame == 1U &&
               scene->jump_entry_pose_index == 325U &&
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0x0308U &&
               scene->actors[scene->shot_actor].pose_index == 325U;
    case 4U:
        return scene->jump_phase_counter == 0x00U &&
               scene->jump_pose_frame == 4U &&
               scene->actors[scene->shot_actor].pose_index == 325U;
    case 5U:
        return scene->jump_phase_counter == 0x30U &&
               scene->jump_pose_frame == 5U &&
               scene->actors[scene->shot_actor].pose_index == 1060U;
    case 8U:
        return !scene->jump_b_released &&
               scene->jump_phase_counter == 0x00U &&
               scene->jump_pose_frame == 8U &&
               scene->actors[scene->shot_actor].pose_index == 1060U;
    case 9U:
        return scene->jump_b_released &&
               scene->jump_phase_counter == 0x30U &&
               scene->jump_pose_frame == 9U &&
               scene->actors[scene->shot_actor].pose_index == 1061U;
    case 10U:
        return scene->jump_actor_state == 0x0BU &&
               scene->jump_phase_counter == 0x31U &&
               scene->jump_pose_frame == 10U &&
               scene->actors[scene->shot_actor].pose_index == 213U;
    case 17U:
        return scene->jump_actor_state == 0x0BU &&
               scene->jump_phase_counter == 0x02U;
    case 18U:
        return scene->jump_actor_state == 0x0CU &&
               scene->jump_phase_counter == 0x34U;
    case 19U:
        return scene->jump_outcome ==
                   TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
               scene->jump_actor_state == 0x0DU &&
               scene->jump_phase_counter == 0x35U &&
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0x0308U;
    case 20U:
        return scene->jump_actor_altitude_q8 == 0x02E0U &&
               scene->jump_actor_velocity_q8 == 0x02E0U;
    case 39U:
        return scene->jump_actor_altitude_q8 == 0x1BD0U &&
               scene->jump_actor_velocity_q8 == 0xFFE8U;
    case 57U:
        return scene->jump_actor_landed &&
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0U &&
               scene->jump_actor_state == 0x0EU &&
               scene->jump_phase_counter == 0x56U;
    case 62U:
        return scene->jump_actor_state == 0x0EU &&
               scene->jump_phase_counter == 0x06U;
    case 63U:
        return scene->jump_actor_state == 0x00U &&
               scene->jump_phase_counter == 0x30U &&
               scene->jump_pose_frame == 0U &&
               scene->actors[scene->shot_actor].pose_index == 469U;
    case 85U:
        return scene->jump_outcome ==
                   TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
               scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] == 5U &&
               scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY &&
               scene->state.shot_clock ==
                   TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    case 110U:
        return scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] == 5U &&
               scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY &&
               !scene->audio_player.sfx_pending;
    default:
        return true;
    }
}

static bool scene_test_close_clock_collision(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t shot_actor;
    if (!tecmo_gameplay_scene_launch(scene, launch)) return false;
    scene->actors[scene->ball_holder].position.x =
        (int16_t)(scene->orientation_state.offensive_hoop.x + 14U);
    scene->actors[scene->ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    scene->actors[scene->ball_holder].facing_right = false;
    scene_attach_ball(scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
        scene->close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene->close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene_test_has_close_semantic_event(&scene->events)) {
        return false;
    }
    shot_actor = scene->shot_actor;
    memset(&p1, 0, sizeof(p1));
    while (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           scene->shot_frame + 1U < scene->shot_duration) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->shot_actor != shot_actor ||
            !scene_test_close_semantic_chain_untouched(scene) ||
            scene_test_has_close_semantic_event(&scene->events)) {
            return false;
        }
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_frame + 1U != scene->shot_duration) {
        return false;
    }

    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 1U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->shot_frame != 0U || scene->shot_duration != 0U ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        scene->state.clock_minutes != 0U ||
        scene->state.clock_seconds != 0U || scene->state.shot_clock != 0U ||
        scene->events.count != 4U ||
        scene->events.events[0].kind != TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        scene->events.events[0].value != TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID ||
        scene->events.events[1].kind != TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        scene->events.events[1].value != TECMO_GAMEPLAY_SFX_EXPIRY_ID ||
        scene->events.events[2].kind !=
            TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED ||
        scene->events.events[2].value !=
            TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK ||
        scene->events.events[2].detail != 1U ||
        scene->events.events[3].kind != TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        scene->events.events[3].value != TECMO_GAMEPLAY_SFX_EXPIRY_ID ||
        scene_test_has_close_semantic_event(&scene->events) ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team != scene->state.possession ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_jump_period_expiry(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch)
{
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoGameplayTeam possession_before;
    uint16_t away_score_before;
    uint16_t home_score_before;
    uint8_t shooting_actor;

    if (scene == NULL || base_launch == NULL) return false;
    launch = *base_launch;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(scene, &launch) ||
        !tecmo_gameplay_set_score(
            &scene->state, TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        return false;
    }
    shooting_actor = scene->ball_holder;
    scene->actors[shooting_actor].position.x = 0x013CU;
    scene->actors[shooting_actor].position.y = 180;
    scene->actors[shooting_actor].facing_right = true;
    scene_attach_ball(scene);
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != 1U || scene->action_serial != 2U ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != 2U ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }
    while (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
           scene->shot_frame < 86U) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != 86U) {
        return false;
    }

    away_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    home_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    possession_before = scene->state.possession;
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = 12U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !scene->state.period_expiry_zero_action_observed ||
        scene->state.possession != possession_before ||
        scene->ball_holder != shooting_actor ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        scene->events.count != 2U ||
        scene->events.events[0].kind != TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        scene->events.events[0].value != TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID ||
        scene->events.events[1].kind != TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        scene->events.events[1].value != TECMO_GAMEPLAY_SFX_EXPIRY_ID ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 11U ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        scene->state.period != 2U ||
        scene->state.period_expiry_zero_action_observed ||
        scene->state.clock_minutes != launch.regulation_minutes ||
        scene->state.clock_seconds != 0U ||
        scene->state.clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES ||
        scene->state.possession != possession_before ||
        scene->ball_holder != shooting_actor ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_jump_make_period_expiry(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *base_launch,
    bool expiry_before_score)
{
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t shooting_actor;
    uint16_t expiry_setup_frame = expiry_before_score ? 83U : 85U;

    if (scene == NULL || base_launch == NULL) return false;
    launch = *base_launch;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(scene, &launch)) return false;
    shooting_actor = scene->ball_holder;
    scene->actors[shooting_actor].position.x = 0x013CU;
    scene->actors[shooting_actor].position.y = 180;
    scene->actors[shooting_actor].facing_right = true;
    scene_attach_ball(scene);
    scene->action_serial = 0U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != 1U || !scene->jump_make_route) {
        return false;
    }
    /* Use the documented early-release normalization so this helper also
       proves that route can safely reach the period boundary. */
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME) {
        return false;
    }
    while (scene->shot_frame < expiry_setup_frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    if (scene->shot_frame != expiry_setup_frame ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            (expiry_before_score ? 0U : 3U)) {
        return false;
    }

    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    scene->state.clock_divider = 1U;
    scene->state.shot_clock = expiry_before_score ? 12U : 24U;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !scene->state.period_expiry_zero_action_observed ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            (expiry_before_score ? 0U : 3U) ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    while (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 3U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != shooting_actor ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 11U ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene->state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        scene->state.period != 2U ||
        scene->state.period_expiry_zero_action_observed ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 3U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->ball_holder != shooting_actor ||
        scene->audio_player.sfx_pending ||
        scene->audio_player.dmc.active ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
}

static bool scene_test_combined_restart_is_inert(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch,
    uint16_t action_serial)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t frame;
    int16_t holder_x;
    if (!tecmo_gameplay_scene_launch(scene, launch)) return false;
    scene->state.shot_clock = 1U;
    scene->state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }

    /* Deliberately stale away holder plus a nearby away defender. If the
       dismissal frame leaks B processing, serial 1 steals and serial 3 fouls. */
    scene->ball_holder = 0U;
    scene->actors[0].position.x = scene->actors[5].position.x + 1;
    scene->actors[0].position.y = scene->actors[5].position.y;
    scene_attach_ball(scene);
    scene->action_serial = action_serial;
    holder_x = scene->actors[5].position.x;
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 5U || scene->controlled_actor[1] != 5U ||
        scene->actors[5].position.x != holder_x ||
        scene->action_serial != action_serial ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
    return true;
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
        scene->orientation_state.current_direction !=
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

bool tecmo_gameplay_scene_self_test(const char *project_root,
                                    const char *asset_pack_path,
                                    TecmoMusicPlayer *music_player,
                                    char *message,
                                    size_t message_size)
{
    TecmoGameplayScene scene;
    TecmoGameplayScene missing_scene;
    TecmoGameplayScene camera_probe;
    TecmoGameplayScene fine_scroll_probe;
    TecmoGameplayScene left_slice_probe;
    TecmoGameplayScene right_slice_probe;
    TecmoGameplayScene backcourt_probe;
    TecmoGameplayScene draw_probe;
    TecmoGameplayScene rattle_before;
    TecmoGameplayState gameplay_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayCameraState frozen_camera;
    TecmoGameplayActorProjection projection;
    TecmoGameplayCourtViewport viewport;
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplaySceneCourtCoordinates unchanged_coordinates;
    TecmoGameplaySceneCourtProjection court_projection;
    TecmoGameplaySceneCourtProjection unchanged_court_projection;
    TecmoGameplaySceneCourtSlice court_slice;
    TecmoGameplaySceneCourtSlice unchanged_court_slice;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoGameplaySceneCourtFrame previous_court_frame;
    TecmoGameplaySceneCourtFrame unchanged_court_frame;
    TecmoGameplayPreTipLineup tip_lineup;
    TecmoGameplaySceneActor boundary_actor;
    TecmoGameplayResolvedPose resolved_pose;
    TecmoGameplaySceneLaunch launch;
    TecmoGameplaySceneResult result;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoFramebuffer framebuffer;
    TecmoFramebuffer invalid_framebuffer;
    TecmoFramebuffer guarded_framebuffer;
    uint32_t *pixels;
    uint32_t *guarded_pixels;
    uint32_t render_hash;
    uint32_t center_slice_hash;
    uint32_t left_slice_hash;
    uint32_t right_slice_hash;
    uint32_t frozen_follow_count;
    uint32_t close_transition_serial;
    uint16_t original_pose;
    uint16_t expected_pose;
    uint16_t jump_entry_pose;
    uint16_t away_score_before;
    uint16_t home_score_before;
    uint8_t holder;
    uint8_t shot_actor;
    uint8_t failed_difficulty;
    int16_t x;
    int16_t y;
    int16_t cpu_holder_start_x;
    bool saw_coarse_crossing;
    bool saw_fine_scroll;
    bool saw_visibility_transition;
    size_t frame;
    size_t pixel;
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    const int guard_width = TECMO_GAMEPLAY_SCENE_NES_WIDTH + 24;
    const int guard_height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT + 20;
    const int guard_origin_x = 12;
    const int guard_origin_y = 10;
    const size_t guarded_pixel_count =
        (size_t)guard_width * (size_t)guard_height;

    scene_self_test_skip_pretip = false;

    tecmo_gameplay_scene_init(&missing_scene);
    if (tecmo_gameplay_scene_load(&missing_scene, project_root,
                                  "?:\\missing-gameplay.assetpack",
                                  music_player) || missing_scene.available) {
        scene_test_message(message, message_size,
                           "missing gameplay pack was accepted");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    memset(&court_slice, 0xA5, sizeof(court_slice));
    unchanged_court_slice = court_slice;
    if (tecmo_gameplay_scene_court_slice(
            &missing_scene, &court_slice) ||
        tecmo_gameplay_scene_court_slice(NULL, &court_slice) ||
        tecmo_gameplay_scene_court_slice(&missing_scene, NULL) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        scene_test_message(
            message, message_size,
            "unavailable TGCT scene slice mutated output");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    memset(&court_frame, 0xA5, sizeof(court_frame));
    unchanged_court_frame = court_frame;
    if (tecmo_gameplay_scene_court_frame(
            &missing_scene, &court_frame) ||
        tecmo_gameplay_scene_court_frame(NULL, &court_frame) ||
        tecmo_gameplay_scene_court_frame(&missing_scene, NULL) ||
        memcmp(&court_frame, &unchanged_court_frame,
               sizeof(court_frame)) != 0) {
        scene_test_message(
            message, message_size,
            "unavailable camera-coherent court frame mutated output");
        tecmo_gameplay_scene_destroy(&missing_scene);
        return false;
    }
    tecmo_gameplay_scene_destroy(&missing_scene);
    tecmo_gameplay_scene_destroy(&missing_scene);

    tecmo_gameplay_scene_init(&scene);
    if (!tecmo_gameplay_scene_load(&scene, project_root, asset_pack_path,
                                   music_player)) {
        scene_test_message(message, message_size, scene.status);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene.shot_resolution.available ||
        scene.shot_resolution.outcome_flag_mask !=
            scene.jump_shots.constants.outcome_flag_mask ||
        scene.shot_resolution.gameplay_core_fingerprint !=
            scene.jump_shots.gameplay_core_fingerprint) {
        scene_test_message(message, message_size,
                           "TGSR-3 scene dependency contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene.camera_assets.available ||
        !scene.cpu_steering_assets.available ||
        scene.cpu_steering_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        scene.cpu_steering_assets.movement_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 ||
        scene.camera_assets.storage_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        scene.camera_assets.gameplay_core_fingerprint != 0x2047CCE0U ||
        scene.camera_assets.gameplay_court_fingerprint != 0xECAB7A93U ||
        scene.court_world.contract_tag !=
            TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG ||
        scene.court_world.width_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES ||
        scene.court_world.height_tiles !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES ||
        scene.court_world.width_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS ||
        scene.court_world.height_pixels !=
            TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS ||
        scene.court_world.tiles_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
        scene.court_world.palette_indices_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32) {
        scene_test_message(message, message_size,
                           "TGCP-2/TGCT-1 live dependency contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.game_music_enabled = true;
    launch.home_team = launch.away_team;
    if (tecmo_gameplay_scene_launch(&scene, &launch) || scene.active) {
        scene_test_message(message, message_size,
                           "invalid gameplay launch was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    launch.home_team = 1U;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "gameplay pre-tip launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_pretip_tip_lineup(
            &scene.pretip_assets, &tip_lineup) ||
        tip_lineup.contract_tag != TECMO_GAMEPLAY_PRETIP_LINEUP_TAG ||
        !tecmo_gameplay_scene_in_pretip(&scene) ||
        scene.pretip_state.phase != TECMO_GAMEPLAY_PRETIP_PRESEASON ||
        scene.pretip_state.card_cancel_enabled ||
        scene.state.clock_minutes != 2U || scene.state.clock_seconds != 0U ||
        scene.state.shot_clock != 24U ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER ||
        scene.ball_position.x_q8 !=
            (int32_t)tip_lineup.ball.x * 256 ||
        scene.ball_position.y_q8 !=
            (int32_t)tip_lineup.ball.y * 256) {
        scene_test_message(message, message_size,
                           "pre-tip freeze/track-8 launch contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++frame) {
        if (scene.actors[frame].position.x !=
                tip_lineup.players[frame].x ||
            scene.actors[frame].position.y !=
                tip_lineup.players[frame].y ||
            scene.actors[frame].anchor.x !=
                tip_lineup.players[frame].x ||
            scene.actors[frame].anchor.y !=
                tip_lineup.players[frame].y ||
            scene.actors[frame].pose_index !=
                tip_lineup.player_pose_indices[frame] ||
            scene.actors[frame].sprite_slot_base !=
                tip_lineup.player_sprite_slot_bases[frame] ||
            !scene.actors[frame].pose_orientation_encoded) {
            scene_test_message(message, message_size,
                               "ROM tip-off player lineup contract failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !scene.active || scene.pretip_abort_pending ||
        scene.pretip_state.aborted ||
        scene.pretip_state.phase_frame != 1U) {
        scene_test_message(message, message_size,
                           "preseason NES-B ignore contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    p1.held.cancel = false;
    tecmo_gameplay_scene_end(&scene);
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "preseason pre-tip relaunch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 481U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip descent entry update rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.pretip_state.phase !=
            TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        scene.pretip_state.phase_frame != 0U ||
        scene.pretip_state.total_frame != 481U ||
        scene.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y * 256 ||
        scene.state.clock_minutes != 2U || scene.state.clock_seconds != 0U ||
        scene.state.shot_clock != 24U) {
        scene_test_message(message, message_size,
                           "pre-tip descent entry/freeze contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip descent midpoint update rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.pretip_state.phase_frame != 30U ||
        scene.ball_position.y_q8 < 107 * 256 ||
        scene.ball_position.y_q8 > 109 * 256) {
        scene_test_message(message, message_size,
                           "pre-tip descent midpoint bounds failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip descent endpoint update rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.pretip_state.phase_frame != 60U ||
        scene.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        scene_test_message(message, message_size,
                           "pre-tip descent endpoint clamp failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 59U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip descent hold update rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.pretip_state.phase_frame != 119U ||
        scene.ball_position.y_q8 !=
            TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y * 256) {
        scene_test_message(message, message_size,
                           "pre-tip descent hold contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 600U; frame < 691U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip live handoff update rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (tecmo_gameplay_scene_in_pretip(&scene) ||
        scene.pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
        !scene.pretip_state.live_handoff ||
        scene.pretip_state.total_frame != 691U ||
        scene.frame != 691U ||
        scene.state.clock_minutes != 2U || scene.state.clock_seconds != 0U ||
        scene.state.shot_clock != 24U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.ball_holder != 0U ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "pre-tip 691-frame track-8-to-5 handoff failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.source = TECMO_GAMEPLAY_SCENE_SEASON;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "gameplay pre-tip abort relaunch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.active || !scene.pretip_abort_pending ||
        !scene.pretip_state.card_cancel_enabled ||
        !tecmo_gameplay_scene_consume_pretip_abort(&scene) ||
        tecmo_gameplay_scene_consume_pretip_abort(&scene) ||
        scene.result_ready ||
        scene.state.clock_minutes != 2U || scene.state.clock_seconds != 0U ||
        scene.state.shot_clock != 24U) {
        scene_test_message(message, message_size,
                           "pre-tip NES-B abort contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    p1.held.cancel = false;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "pre-tip timing relaunch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 437U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip timing advance rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !scene.pretip_state.home_tip_sampled ||
        scene.pretip_state.home_tip_error != 0U) {
        scene_test_message(message, message_size,
                           "pre-tip home timing sample rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    p1.held.cancel = false;
    for (frame = 438U; frame < 691U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "pre-tip timing handoff rejected");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.orientation_state.current_direction != 1U) {
        scene_test_message(message, message_size,
                           "pre-tip timing possession contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    scene_self_test_skip_pretip = true;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "gameplay scene canonical launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (scene.camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        scene.camera_state.scroll_x != 0U ||
        scene.camera_state.scroll_aux != 0U ||
        scene.camera_state.nametable_page != 0U ||
        scene.camera_state.aux != 0U ||
        scene.camera_state.stream_direction != 0U ||
        scene.camera_state.layout_cursor != 0x21U ||
        scene.camera_state.left_threshold != 0x50U ||
        scene.camera_state.right_threshold != 0xA0U ||
        !scene.camera_state.thresholds_valid ||
        scene.camera_state.endpoint_latched ||
        scene.camera_follow_count != 0U ||
        scene.actors[0].position.x != 0x0160 ||
        scene.actors[0].position.y != 198 ||
        scene.actors[0].pose_index != 117U ||
        scene.actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].pose_index !=
            85U ||
        scene.ball_position.x_q8 != 0x0166 * 256 ||
        scene.ball_position.y_q8 != 176 * 256 ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene.camera_assets, &scene.camera_state)) {
        scene_test_message(
            message, message_size,
            "TGCP-2 live prime/initial world-state contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_test_live_hud_contract(&scene)) {
        scene_test_message(
            message, message_size,
            "THUD live score/player/clock projection contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    backcourt_probe = scene;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    backcourt_probe.actors[0U].position.x = 368;
    backcourt_probe.actors[0U].position.y = 148;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.actors[0U].facing_right = true;
    backcourt_probe.actors[0U].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[0U].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[0U].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 375 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        scene_test_message(message, message_size,
                           "live backcourt frontcourt latch failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    backcourt_probe.actors[0U].position.x = 380;
    backcourt_probe.actors[0U].anchor =
        backcourt_probe.actors[0U].position;
    backcourt_probe.ball_position.x_q8 = 386 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_HOME ||
        backcourt_probe.state.phase_frame != 0U) {
        scene_test_message(message, message_size,
                           "live backcourt settlement route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    backcourt_probe = scene;
    holder = TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
    if (!scene_handoff_possession(
            &backcourt_probe, TECMO_GAMEPLAY_TEAM_HOME, holder) ||
        backcourt_probe.orientation_state.current_direction != 1U) {
        scene_test_message(message, message_size,
                           "reverse backcourt possession setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    backcourt_probe.actors[holder].position.x = 398;
    backcourt_probe.actors[holder].position.y = 148;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.actors[holder].facing_right = false;
    backcourt_probe.actors[holder].movement_action_state =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    backcourt_probe.actors[holder].movement_fractional_accumulator = 0U;
    backcourt_probe.actors[holder].movement_boundary_latched = false;
    backcourt_probe.ball_position.x_q8 = 392 * 256;
    backcourt_probe.ball_position.y_q8 = 131 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        backcourt_probe.backcourt_state.frontcourt_established != 1U) {
        scene_test_message(message, message_size,
                           "reverse live backcourt frontcourt latch failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    backcourt_probe.actors[holder].position.x = 387;
    backcourt_probe.actors[holder].anchor =
        backcourt_probe.actors[holder].position;
    backcourt_probe.ball_position.x_q8 = 383 * 256;
    if (!tecmo_gameplay_scene_update(&backcourt_probe, &p1, &p2) ||
        backcourt_probe.state.phase !=
            TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        backcourt_probe.state.violation !=
            TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        backcourt_probe.state.restart_possession !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        backcourt_probe.state.phase_frame != 0U) {
        scene_test_message(message, message_size,
                           "reverse live backcourt settlement route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_resolve_actor_pose(&scene, 0U, &resolved_pose) ||
        resolved_pose.record_tag != 0x25U ||
        resolved_pose.mmc3_r2_r5[1] != 0x25U ||
        resolved_pose.piece_count != 4U ||
        resolved_pose.palette_group != 1U ||
        resolved_pose.actor_attributes != 1U ||
        resolved_pose.pieces[0].palette_index != 1U ||
        !resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0x30U ||
        memcmp(resolved_pose.palette,
               "\x16\x0F\x27\x30\x16\x0F\x17\x30"
               "\x16\x0F\x27\x2A\x16\x0F\x17\x2A", 16U) != 0 ||
        !scene_resolve_actor_pose(
            &scene, TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT,
            &resolved_pose) ||
        resolved_pose.palette_group != 1U ||
        resolved_pose.actor_attributes != 3U ||
        resolved_pose.pieces[0].palette_index != 3U ||
        memcmp(resolved_pose.pieces[0].palette,
               "\x16\x0F\x17\x2A", 4U) != 0 ||
        !resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0x2AU ||
        memcmp(resolved_pose.palette,
               "\x16\x0F\x27\x30\x16\x0F\x17\x30"
               "\x16\x0F\x27\x2A\x16\x0F\x17\x2A", 16U) != 0 ||
        !scene_resolve_pose(
            &scene, TECMO_GAMEPLAY_BALL_POSE, 0xC1U, 0U, 0U, false, 0U,
            &resolved_pose) ||
        resolved_pose.record_tag != 0x81U ||
        resolved_pose.mmc3_r2_r5[3] != 0x81U ||
        resolved_pose.uniform_color_applied ||
        resolved_pose.uniform_color != 0U ||
        resolved_pose.piece_count != 1U ||
        resolved_pose.pieces[0].top_chr_offset != 0x20400U) {
        scene_test_message(
            message, message_size,
            "court actor/ball pose CHR-slot binding failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&coordinates, 0xA5, sizeof(coordinates));
    if (!tecmo_gameplay_scene_court_coordinates(
            &scene, &coordinates) ||
        coordinates.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG ||
        coordinates.players[0].x != 0x0160 ||
        coordinates.players[0].y != 198 ||
        coordinates.ball.x_q8 != 0x0166 * 256 ||
        coordinates.ball.y_q8 != 176 * 256 ||
        coordinates.hoops[0].x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        coordinates.hoops[0].y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        coordinates.hoops[1].x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        coordinates.hoops[1].y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        scene_test_message(
            message, message_size,
            "canonical player/ball/hoop coordinate snapshot failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&court_projection, 0xA5, sizeof(court_projection));
    if (!tecmo_gameplay_scene_court_projection(
            &scene, &court_projection) ||
        court_projection.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_PROJECTION_TAG ||
        court_projection.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_projection.reserved != 0U ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0x60U ||
        court_projection.players[0].screen_y != 198U ||
        !court_projection.ball.visible ||
        court_projection.ball.screen_x != 0x66U ||
        court_projection.ball.screen_y != 176U) {
        scene_test_message(
            message, message_size,
            "canonical TGCP scene projection snapshot failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&court_slice, 0xA5, sizeof(court_slice));
    if (!tecmo_gameplay_scene_court_slice(&scene, &court_slice) ||
        court_slice.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_SLICE_TAG ||
        court_slice.transition_serial != 0U ||
        court_slice.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        court_slice.direction != 0U ||
        court_slice.reserved != 0U ||
        court_slice.viewport.camera_x !=
            TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        court_slice.viewport.first_tile_x != 0x20U ||
        court_slice.viewport.fine_scroll_x != 0U ||
        court_slice.viewport.column_count != 32U ||
        court_slice.viewport.camera_x != court_projection.camera_x) {
        scene_test_message(
            message, message_size,
            "possession-aware TGCT center slice snapshot failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&court_frame, 0xA5, sizeof(court_frame));
    if (!tecmo_gameplay_scene_court_frame(
            &scene, &court_frame) ||
        court_frame.contract_tag !=
            TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG ||
        court_frame.scene_frame != scene.frame ||
        court_frame.camera_follow_count != scene.camera_follow_count ||
        court_frame.reserved != 0U ||
        memcmp(&court_frame.slice, &court_slice,
               sizeof(court_slice)) != 0 ||
        memcmp(&court_frame.projection, &court_projection,
               sizeof(court_projection)) != 0) {
        scene_test_message(
            message, message_size,
            "camera-coherent center court frame snapshot failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    unchanged_coordinates = coordinates;
    unchanged_court_projection = court_projection;
    unchanged_court_slice = court_slice;
    unchanged_court_frame = court_frame;
    scene.ball_position.x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X + 1;
    if (tecmo_gameplay_scene_court_coordinates(
            &scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0 ||
        tecmo_gameplay_scene_court_projection(
            &scene, &court_projection) ||
        memcmp(&court_projection, &unchanged_court_projection,
               sizeof(court_projection)) != 0 ||
        tecmo_gameplay_scene_court_frame(
            &scene, &court_frame) ||
        memcmp(&court_frame, &unchanged_court_frame,
               sizeof(court_frame)) != 0) {
        scene_test_message(
            message, message_size,
            "invalid coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    camera_before = scene.camera_state;
    scene.camera_state.camera_x =
        TECMO_GAMEPLAY_COURT_MAX_CAMERA_X + 1U;
    if (tecmo_gameplay_scene_court_slice(
            &scene, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        scene_test_message(
            message, message_size,
            "invalid camera mutated possession-aware TGCT slice");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.camera_state = camera_before;
    orientation_before = scene.orientation_state;
    scene.orientation_state.tracked_possession_team =
        TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME;
    if (tecmo_gameplay_scene_court_slice(
            &scene, &court_slice) ||
        memcmp(&court_slice, &unchanged_court_slice,
               sizeof(court_slice)) != 0) {
        scene_test_message(
            message, message_size,
            "mismatched possession mutated TGCT scene slice");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.orientation_state = orientation_before;
    scene.ball_position = unchanged_coordinates.ball;
    scene.actors[0].position.x = 0;
    if (!tecmo_gameplay_scene_court_projection(
            &scene, &court_projection) ||
        court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0U ||
        court_projection.players[0].screen_y != 0U) {
        scene_test_message(
            message, message_size,
            "canonical TGCP offscreen projection sentinel failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position =
        unchanged_coordinates.players[0];
    memset(&scene.shot_start_position, 0,
           sizeof(scene.shot_start_position));
    memset(&scene.shot_end_position, 0,
           sizeof(scene.shot_end_position));
    scene.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    scene.shot_actor = 0U;
    scene.jump_actor_altitude_q8 = 10U * 256U;
    if (!tecmo_gameplay_scene_court_projection(
            &scene, &court_projection) ||
        !court_projection.players[0].visible ||
        court_projection.players[0].screen_x != 0x60U ||
        court_projection.players[0].screen_y != 188U ||
        court_projection.ball.screen_y != 176U) {
        scene_test_message(
            message, message_size,
            "canonical TGCP jump-altitude projection failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene.shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene.jump_actor_altitude_q8 = 0U;
    scene.actors[0].position.x = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            &scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        scene_test_message(
            message, message_size,
            "invalid player coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position =
        unchanged_coordinates.players[0];
    scene.actors[0].anchor.y =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_Y + 1);
    if (tecmo_gameplay_scene_court_coordinates(
            &scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        scene_test_message(
            message, message_size,
            "invalid player anchor snapshot mutated output");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].anchor =
        unchanged_coordinates.players[0];
    scene.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    scene.shot_start_position.x_q8 = -1;
    if (tecmo_gameplay_scene_court_coordinates(
            &scene, &coordinates) ||
        memcmp(&coordinates, &unchanged_coordinates,
               sizeof(coordinates)) != 0) {
        scene_test_message(
            message, message_size,
            "invalid shot coordinate snapshot mutated output");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    memset(&scene.shot_start_position, 0,
           sizeof(scene.shot_start_position));

    memset(&boundary_actor, 0, sizeof(boundary_actor));
    boundary_actor.active = true;
    boundary_actor.position.y = TECMO_GAMEPLAY_MIN_Y;
    boundary_actor.position.x = -1;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x00DF ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        scene_test_message(message, message_size,
                           "page-0 scene-safety boundary diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    boundary_actor.position.y = 128;
    boundary_actor.position.x = 300;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 300 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        scene_test_message(message, message_size,
                           "page-1 scene-safety interior diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    boundary_actor.position.y = TECMO_GAMEPLAY_MAX_Y;
    boundary_actor.position.x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    scene_clamp_actor_world(&boundary_actor);
    if (boundary_actor.position.x != 0x0297 ||
        !scene_actor_world_position_valid(&boundary_actor)) {
        scene_test_message(message, message_size,
                           "page-2 scene-safety boundary diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    memset(&projection, 0xA5, sizeof(projection));
    if (!tecmo_gameplay_camera_project_actor(
            &scene.camera_assets, &scene.camera_state,
            0x0100U, 100U, 10U, &projection) ||
        !projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 90U ||
        !tecmo_gameplay_camera_project_actor(
            &scene.camera_assets, &scene.camera_state,
            0x01FFU, 100U, 0U, &projection) ||
        !projection.visible || projection.screen_x != 0xFFU ||
        !tecmo_gameplay_camera_project_actor(
            &scene.camera_assets, &scene.camera_state,
            0x00FFU, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U ||
        !tecmo_gameplay_camera_project_actor(
            &scene.camera_assets, &scene.camera_state,
            0x0200U, 100U, 0U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U) {
        scene_test_message(message, message_size,
                           "live actor projection/offscreen contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    left_slice_probe = scene;
    left_slice_probe.actors[left_slice_probe.ball_holder].position.x =
        0x00F3;
    left_slice_probe.actors[left_slice_probe.ball_holder].facing_right =
        true;
    if (!scene_attach_ball(&left_slice_probe) ||
        !tecmo_gameplay_scene_court_frame(
            &left_slice_probe, &previous_court_frame)) {
        scene_test_message(message, message_size,
                           "left-possession ball placement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    saw_coarse_crossing = false;
    saw_fine_scroll = false;
    saw_visibility_transition = false;
    for (frame = 0U; frame < 200U; ++frame) {
        size_t projection_actor;
        uint16_t previous_camera_x =
            left_slice_probe.camera_state.camera_x;
        if (!scene_follow_live_camera_once(&left_slice_probe) ||
            !tecmo_gameplay_scene_court_frame(
                &left_slice_probe, &court_frame) ||
            court_frame.scene_frame !=
                previous_court_frame.scene_frame ||
            court_frame.camera_follow_count !=
                previous_court_frame.camera_follow_count + 1U ||
            court_frame.slice.possession !=
                TECMO_GAMEPLAY_TEAM_AWAY ||
            court_frame.slice.direction != 0U ||
            !scene_test_stationary_projection_transition(
                &previous_court_frame, &court_frame)) {
            scene_test_message(message, message_size,
                               "left-possession camera follow failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        saw_coarse_crossing =
            saw_coarse_crossing ||
            court_frame.slice.viewport.first_tile_x !=
                previous_court_frame.slice.viewport.first_tile_x;
        saw_fine_scroll =
            saw_fine_scroll ||
            court_frame.slice.viewport.fine_scroll_x !=
                previous_court_frame.slice.viewport.fine_scroll_x;
        for (projection_actor = 0U;
             projection_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++projection_actor) {
            saw_visibility_transition =
                saw_visibility_transition ||
                court_frame.projection.players[
                    projection_actor].visible !=
                    previous_court_frame.projection.players[
                        projection_actor].visible;
        }
        previous_court_frame = court_frame;
        if (left_slice_probe.camera_state.camera_x ==
            previous_camera_x) {
            break;
        }
    }
    if (frame == 200U ||
        left_slice_probe.camera_state.camera_x != 0x0066U ||
        !saw_coarse_crossing || !saw_fine_scroll ||
        !saw_visibility_transition ||
        previous_court_frame.slice.possession !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        previous_court_frame.slice.direction != 0U ||
        previous_court_frame.slice.transition_serial != 0U ||
        previous_court_frame.slice.viewport.camera_x != 0x0066U ||
        previous_court_frame.projection.camera_x != 0x0066U ||
        previous_court_frame.slice.viewport.first_tile_x != 0x0CU ||
        previous_court_frame.slice.viewport.fine_scroll_x != 6U ||
        previous_court_frame.slice.viewport.column_count != 33U) {
        scene_test_message(
            message, message_size,
            "left-possession actor/court projection sweep failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    right_slice_probe = left_slice_probe;
    if (!scene_handoff_possession(
            &right_slice_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        !tecmo_gameplay_scene_court_frame(
            &right_slice_probe, &court_frame) ||
        court_frame.scene_frame != previous_court_frame.scene_frame ||
        court_frame.camera_follow_count !=
            previous_court_frame.camera_follow_count ||
        court_frame.slice.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        court_frame.slice.direction != 1U ||
        court_frame.slice.transition_serial != 1U ||
        memcmp(&court_frame.slice.viewport,
               &previous_court_frame.slice.viewport,
               sizeof(court_frame.slice.viewport)) != 0 ||
        memcmp(&court_frame.projection.players,
               &previous_court_frame.projection.players,
               sizeof(court_frame.projection.players)) != 0) {
        scene_test_message(message, message_size,
                           "possession reversal changed stationary actors");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    right_slice_probe.actors[right_slice_probe.ball_holder].position.x =
        0x020D;
    right_slice_probe.actors[right_slice_probe.ball_holder].facing_right =
        false;
    if (!scene_attach_ball(&right_slice_probe) ||
        !tecmo_gameplay_scene_court_frame(
            &right_slice_probe, &previous_court_frame)) {
        scene_test_message(message, message_size,
                           "right-possession ball placement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    saw_coarse_crossing = false;
    saw_fine_scroll = false;
    saw_visibility_transition = false;
    for (frame = 0U; frame < 200U; ++frame) {
        size_t projection_actor;
        uint16_t previous_camera_x =
            right_slice_probe.camera_state.camera_x;
        if (!scene_follow_live_camera_once(&right_slice_probe) ||
            !tecmo_gameplay_scene_court_frame(
                &right_slice_probe, &court_frame) ||
            court_frame.scene_frame !=
                previous_court_frame.scene_frame ||
            court_frame.camera_follow_count !=
                previous_court_frame.camera_follow_count + 1U ||
            court_frame.slice.possession !=
                TECMO_GAMEPLAY_TEAM_HOME ||
            court_frame.slice.direction != 1U ||
            !scene_test_stationary_projection_transition(
                &previous_court_frame, &court_frame)) {
            scene_test_message(message, message_size,
                               "right-possession camera follow failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        saw_coarse_crossing =
            saw_coarse_crossing ||
            court_frame.slice.viewport.first_tile_x !=
                previous_court_frame.slice.viewport.first_tile_x;
        saw_fine_scroll =
            saw_fine_scroll ||
            court_frame.slice.viewport.fine_scroll_x !=
                previous_court_frame.slice.viewport.fine_scroll_x;
        for (projection_actor = 0U;
             projection_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++projection_actor) {
            saw_visibility_transition =
                saw_visibility_transition ||
                court_frame.projection.players[
                    projection_actor].visible !=
                    previous_court_frame.projection.players[
                        projection_actor].visible;
        }
        previous_court_frame = court_frame;
        if (right_slice_probe.camera_state.camera_x ==
            previous_camera_x) {
            break;
        }
    }
    if (frame == 200U ||
        right_slice_probe.camera_state.camera_x != 0x0198U ||
        !saw_coarse_crossing || !saw_fine_scroll ||
        !saw_visibility_transition ||
        previous_court_frame.slice.possession !=
            TECMO_GAMEPLAY_TEAM_HOME ||
        previous_court_frame.slice.direction != 1U ||
        previous_court_frame.slice.transition_serial != 1U ||
        previous_court_frame.slice.viewport.camera_x != 0x0198U ||
        previous_court_frame.projection.camera_x != 0x0198U ||
        previous_court_frame.slice.viewport.first_tile_x != 0x33U ||
        previous_court_frame.slice.viewport.fine_scroll_x != 0U ||
        previous_court_frame.slice.viewport.column_count != 32U) {
        scene_test_message(
            message, message_size,
            "right-possession actor/court projection sweep failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_test_live_hud_equal(&scene, &left_slice_probe) ||
        !scene_test_live_hud_equal(&scene, &right_slice_probe)) {
        scene_test_message(
            message, message_size,
            "live HUD changed across possession camera endpoints");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    camera_probe = scene;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01B9;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
        camera_probe.camera_follow_count != 1U ||
        camera_probe.camera_state.camera_x != 0x0107U ||
        camera_probe.camera_state.scroll_x != 0x07U ||
        camera_probe.camera_state.layout_cursor != 0x21U ||
        !camera_probe.camera_state.thresholds_valid ||
        !tecmo_gameplay_camera_state_live_valid(
            &camera_probe.camera_assets, &camera_probe.camera_state)) {
        scene_test_message(message, message_size,
                           "single live camera follow/fine-scroll contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    fine_scroll_probe = camera_probe;
    camera_before = camera_probe.camera_state;
    frozen_follow_count = camera_probe.camera_follow_count;
    camera_probe.backcourt_state.frontcourt_established = 1U;
    if (!scene_handoff_possession(
            &camera_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        camera_probe.camera_state.camera_x != camera_before.camera_x ||
        camera_probe.camera_state.scroll_x != camera_before.scroll_x ||
        camera_probe.camera_state.thresholds_valid ||
        camera_probe.camera_state.endpoint_latched ||
        camera_probe.backcourt_state.frontcourt_established != 0U ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        scene_test_message(message, message_size,
                           "possession camera continuity/latch reset failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    camera_probe.actors[camera_probe.ball_holder].position.x = 0x01FF;
    scene_attach_ball(&camera_probe);
    if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U ||
        camera_probe.camera_state.camera_x != camera_before.camera_x + 2U ||
        !camera_probe.camera_state.thresholds_valid ||
        !camera_probe.camera_state.endpoint_latched) {
        scene_test_message(message, message_size,
                           "first opposite-possession live follow failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 200U; ++frame) {
        uint16_t previous_camera_x = camera_probe.camera_state.camera_x;
        if (!tecmo_gameplay_scene_update(&camera_probe, &p1, &p2) ||
            camera_probe.camera_state.camera_x >
                TECMO_GAMEPLAY_COURT_MAX_CAMERA_X ||
            camera_probe.camera_state.layout_cursor > 0x34U ||
            !tecmo_gameplay_camera_state_live_valid(
                &camera_probe.camera_assets, &camera_probe.camera_state)) {
            scene_test_message(message, message_size,
                               "live camera endpoint bounds failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        if (camera_probe.camera_state.camera_x == previous_camera_x) break;
    }
    if (frame == 200U ||
        camera_probe.camera_state.layout_cursor != 0x34U) {
        scene_test_message(message, message_size,
                           "live camera did not settle at its cursor bound");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    frozen_camera = camera_probe.camera_state;
    frozen_follow_count = camera_probe.camera_follow_count;
    camera_probe.state.phase = TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE;
    if (!scene_follow_live_camera_once(&camera_probe) ||
        memcmp(&camera_probe.camera_state, &frozen_camera,
               sizeof(frozen_camera)) != 0 ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        scene_test_message(message, message_size,
                           "free-throw camera freeze contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    camera_probe.state.phase = TECMO_GAMEPLAY_PHASE_LIVE;
    camera_probe.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_DUNK;
    camera_probe.shot_actor = camera_probe.ball_holder;
    camera_probe.shot_frame = TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME;
    if (!scene_follow_live_camera_once(&camera_probe) ||
        memcmp(&camera_probe.camera_state, &frozen_camera,
               sizeof(frozen_camera)) != 0 ||
        camera_probe.camera_follow_count != frozen_follow_count) {
        scene_test_message(message, message_size,
                           "TGDK cutaway camera freeze contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    camera_probe.shot_frame = TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME;
    if (!scene_follow_live_camera_once(&camera_probe) ||
        camera_probe.camera_follow_count != frozen_follow_count + 1U) {
        scene_test_message(message, message_size,
                           "TGDK first-live-return camera resume failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    if (!tecmo_gameplay_court_orientation_state_valid(
            &scene.court_orientation, &scene.orientation_state) ||
        scene.orientation_state.current_direction != 0U ||
        scene.orientation_state.previous_direction != 0U ||
        scene.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene.orientation_state.transition_serial != 0U ||
        scene.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        scene_test_message(message, message_size,
                           "court-orientation fresh-launch contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    orientation_before = scene.orientation_state;
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        memcmp(&scene.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !tecmo_gameplay_reset_possession(
            &scene.state, TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        scene.orientation_state.current_direction != 1U ||
        scene.orientation_state.previous_direction != 0U ||
        scene.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME ||
        scene.orientation_state.transition_serial != 1U ||
        scene.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        scene.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        scene_test_message(
            message, message_size,
            "court-orientation changed-first handoff contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    orientation_before = scene.orientation_state;
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        memcmp(&scene.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        scene.orientation_state.current_direction != 0U ||
        scene.orientation_state.previous_direction != 1U ||
        scene.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene.orientation_state.transition_serial != 2U ||
        scene.orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene.orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y) {
        scene_test_message(message, message_size,
                           "court-orientation no-op/roundtrip contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    gameplay_before = scene.state;
    orientation_before = scene.orientation_state;
    if (scene_handoff_possession(
            &scene, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT, 0U) ||
        memcmp(&scene.state, &gameplay_before,
               sizeof(gameplay_before)) != 0 ||
        memcmp(&scene.orientation_state, &orientation_before,
               sizeof(orientation_before)) != 0 ||
        !scene_ownership_valid(&scene)) {
        scene_test_message(message, message_size,
                           "court-orientation invalid handoff mutated scene");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        scene.orientation_state.current_direction != 0U ||
        scene.orientation_state.transition_serial != 0U ||
        scene.orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        scene.camera_state.camera_x != TECMO_GAMEPLAY_INITIAL_CAMERA_X ||
        scene.camera_state.layout_cursor != 0x21U ||
        !scene.camera_state.thresholds_valid ||
        scene.camera_state.endpoint_latched ||
        scene.camera_follow_count != 0U) {
        scene_test_message(
            message, message_size,
            "court-orientation restart launch contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        scene_test_message(message, message_size,
                           "gameplay render test allocation failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    framebuffer.height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT;
    framebuffer.pitch_pixels = TECMO_GAMEPLAY_SCENE_NES_WIDTH;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &scene, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        scene_test_message(message, message_size,
                           "center-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    center_slice_hash = scene_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &left_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        scene_test_message(message, message_size,
                           "left-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    left_slice_hash = scene_pixels_fnv1a32(pixels, pixel_count);
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &right_slice_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        scene_test_message(message, message_size,
                           "right-possession TGCT slice render rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    right_slice_hash = scene_pixels_fnv1a32(pixels, pixel_count);
    if (center_slice_hash !=
            TECMO_GAMEPLAY_SCENE_CENTER_SLICE_FNV1A32 ||
        left_slice_hash !=
            TECMO_GAMEPLAY_SCENE_LEFT_SLICE_FNV1A32 ||
        right_slice_hash !=
            TECMO_GAMEPLAY_SCENE_RIGHT_SLICE_FNV1A32 ||
        center_slice_hash == left_slice_hash ||
        center_slice_hash == right_slice_hash ||
        left_slice_hash == right_slice_hash) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "possession TGCT slice hashes changed: center=%08X left=%08X right=%08X",
            (unsigned)center_slice_hash, (unsigned)left_slice_hash,
            (unsigned)right_slice_hash);
        free(pixels);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(&scene, &framebuffer, 0, 0, 1, true)) {
        free(pixels);
        scene_test_message(message, message_size,
                           "canonical gameplay render rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    render_hash = scene_pixels_fnv1a32(pixels, pixel_count);
    if (render_hash != TECMO_GAMEPLAY_SCENE_RENDER_FNV1A32) {
        char failure[128];
        (void)snprintf(failure, sizeof(failure),
                       "gameplay render hash mismatch: %08X",
                       (unsigned)render_hash);
        free(pixels);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    draw_probe = scene;
    draw_probe.ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, false)) {
        free(pixels);
        scene_test_message(
            message, message_size,
            "background-only draw rejected invalid live ownership");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        scene_test_message(
            message, message_size,
            "live actor draw accepted invalid ownership or rendered partially");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    guarded_pixels = (uint32_t *)malloc(
        guarded_pixel_count * sizeof(*guarded_pixels));
    if (guarded_pixels == NULL) {
        free(pixels);
        scene_test_message(message, message_size,
                           "fine-scroll guarded render allocation failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    guarded_framebuffer.pixels = guarded_pixels;
    guarded_framebuffer.width = guard_width;
    guarded_framebuffer.height = guard_height;
    guarded_framebuffer.pitch_pixels = guard_width;
    for (pixel = 0U; pixel < guarded_pixel_count; ++pixel) {
        guarded_pixels[pixel] = 0xA5A5A5A5U;
    }
    if (!tecmo_gameplay_court_slice_viewport(
            &fine_scroll_probe.court_world,
            fine_scroll_probe.camera_state.camera_x, &viewport) ||
        viewport.camera_x != 0x0107U ||
        viewport.first_tile_x != 0x20U ||
        viewport.fine_scroll_x != 7U ||
        viewport.column_count != 33U ||
        !tecmo_gameplay_scene_draw(
            &fine_scroll_probe, &guarded_framebuffer,
            guard_origin_x, guard_origin_y, 1, true) ||
        !scene_test_outer_margin_equal(
            guarded_pixels, guard_width, guard_height, guard_width,
            guard_origin_x, guard_origin_y,
            TECMO_GAMEPLAY_SCENE_NES_WIDTH,
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT, 0xA5A5A5A5U) ||
        scene_test_pixels_equal(
            guarded_pixels, guarded_pixel_count, 0xA5A5A5A5U)) {
        free(guarded_pixels);
        free(pixels);
        scene_test_message(
            message, message_size,
            "33-column fine-scroll seam/margin render contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    free(guarded_pixels);

    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    draw_probe = scene;
    draw_probe.court_world.tiles_fingerprint ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        scene_test_message(
            message, message_size,
            "corrupt full-court world partially rendered");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    draw_probe = scene;
    draw_probe.camera_state.nametable_page ^= 1U;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        scene_test_message(
            message, message_size,
            "invalid live camera partially rendered");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    draw_probe = scene;
    draw_probe.hud_assets.available = false;
    if (tecmo_gameplay_scene_draw(
            &draw_probe, &framebuffer, 0, 0, 1, true) ||
        !scene_test_pixels_equal(
            pixels, pixel_count, 0xA5A5A5A5U)) {
        free(pixels);
        scene_test_message(
            message, message_size,
            "unavailable THUD asset partially rendered live scene");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        pixels[pixel] = 0xA5A5A5A5U;
    }
    original_pose = scene.actors[0].pose_index;
    scene.actors[0].pose_index = UINT16_MAX;
    if (tecmo_gameplay_scene_draw(&scene, &framebuffer, 0, 0, 1, true)) {
        scene.actors[0].pose_index = original_pose;
        free(pixels);
        scene_test_message(message, message_size,
                           "invalid gameplay pose was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].pose_index = original_pose;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != 0xA5A5A5A5U) {
            free(pixels);
            scene_test_message(message, message_size,
                               "failed render partially modified pixels");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    invalid_framebuffer = framebuffer;
    invalid_framebuffer.width = TECMO_GAMEPLAY_SCENE_NES_WIDTH - 1;
    if (tecmo_gameplay_scene_draw(&scene, &invalid_framebuffer,
                                  0, 0, 1, false)) {
        free(pixels);
        scene_test_message(message, message_size,
                           "undersized gameplay framebuffer was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    free(pixels);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    x = scene.actors[scene.controlled_actor[0]].position.x;
    p1.held.confirm = true;
    p1.pressed.confirm = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        scene_test_message(message, message_size,
                           "START changed live gameplay state");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x ||
        scene.actors[scene.controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x + 1) {
        scene_test_message(message, message_size,
                           "TGMO directional latency/movement contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    x = scene.actors[scene.controlled_actor[0]].position.x;
    y = scene.actors[scene.controlled_actor[0]].position.y;
    memset(&p1, 0, sizeof(p1));
    p1.held.left = true;
    p1.held.right = true;
    p1.held.up = true;
    p1.held.down = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x ||
        scene.actors[scene.controlled_actor[0]].position.y != y ||
        scene.actors[scene.controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
        scene_test_message(
            message, message_size,
            "contradictory-axis neutral integration policy failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    holder = scene.ball_holder;
    {
        uint8_t pass_target = scene_next_teammate(&scene, holder);
        if (pass_target >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            scene_test_message(message, message_size,
                               "NES A pass target setup failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        scene.actors[pass_target].position.x = 300;
        scene.actors[pass_target].anchor =
            scene.actors[pass_target].position;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.ball_holder == holder ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        scene_test_message(message, message_size,
                           "NES A pass contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (scene.controlled_actor[0] != scene.ball_holder) {
        scene_test_message(message, message_size,
                           "pass ownership invariant failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[scene.ball_holder].position.x = 174;
    scene.actors[scene.ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    scene.actors[scene.ball_holder].facing_right = true;
    /* The following close-shot checkpoints are deterministic makes. The later
       ordinary-jump checkpoint is the supported terminal miss. */
    scene.action_serial = 1U;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
        scene.shot_duration != TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME ||
        scene.close_shot_step != 0U ||
        scene.close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene.close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !scene_close_pose_for_step(&scene, 0U, &expected_pose) ||
        scene.actors[scene.shot_actor].pose_index != expected_pose ||
        !scene_test_draw_exact_step(&scene)) {
        scene_test_message(message, message_size,
                           "NES B dunk/TGCS variant-0 contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_1;
    if (scene_close_pose_for_step(&scene, 0U, &expected_pose)) {
        scene_test_message(message, message_size,
                           "unsupported live TGCS direction was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    close_transition_serial =
        scene.state.close_shot_subtype01.transition_serial;
    shot_actor = scene.shot_actor;
    for (frame = 0U; frame < 140U &&
         scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p1, 0, sizeof(p1));
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (tecmo_gameplay_scene_in_dunk_presentation(&scene) !=
                   (scene.shot_frame >=
                        TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
                    scene.shot_frame <
                        TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME) ||
               !scene_close_pose_for_step(&scene, scene.close_shot_step,
                                          &expected_pose) ||
               scene.actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(&scene) ||
               scene_test_has_close_semantic_event(&scene.events) ||
               scene.state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !scene_test_draw_exact_step(&scene)))) {
            scene_test_message(message, message_size,
                               "dunk/TGCS variant-0 replay failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            if ((scene.shot_frame == 22U && scene.close_shot_step != 22U) ||
                (scene.shot_frame == 23U && scene.close_shot_step != 22U) ||
                (scene.shot_frame == 70U && scene.close_shot_step != 22U) ||
                (scene.shot_frame == 71U && scene.close_shot_step != 23U) ||
                (scene.shot_frame == 79U && scene.close_shot_step != 31U) ||
                (scene.shot_frame == 86U && scene.audio_player.dmc.active) ||
                (scene.shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME &&
                 (!scene.audio_player.dmc.active ||
                  scene.audio_player.dmc.byte_index != 0U ||
                  scene.audio_player.dmc.byte_count !=
                      scene.audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].byte_count ||
                  scene.audio_player.dmc.pool_index !=
                      scene.audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].pool_index)) ||
                (scene.shot_frame == 88U &&
                 scene.audio_player.dmc.byte_index != 1U)) {
                scene_test_message(message, message_size,
                                   "dunk presentation timing/audio boundary failed");
                tecmo_gameplay_scene_destroy(&scene);
                return false;
            }
            if (scene.shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME) {
                /* A later accidental requeue would reset this sentinel. */
                scene.audio_player.dmc.byte_index = 1U;
            }
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.controlled_actor[1] != scene.ball_holder ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 12U ||
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "dunk/TGCS variant-0 settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 12U) {
        scene_test_message(message, message_size,
                           "dunk side-result mailbox was not last-write-wins");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    scene.actors[scene.ball_holder].position.x = 578;
    scene.actors[scene.ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    /* Left-facing animation mirrors the supported direction-0 slice; no ROM
       mapping to another TGCS direction entry is claimed by this milestone. */
    scene.actors[scene.ball_holder].facing_right = false;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_LAYUP ||
        scene.close_shot_step != 0U ||
        scene.close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene.close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !scene_close_pose_for_step(&scene, 0U, &expected_pose) ||
        scene.actors[scene.shot_actor].pose_index != expected_pose ||
        !scene_test_draw_exact_step(&scene)) {
        scene_test_message(message, message_size,
                           "NES B layup/TGCS variant-2 contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    close_transition_serial =
        scene.state.close_shot_subtype01.transition_serial;
    shot_actor = scene.shot_actor;
    for (frame = 0U; frame < 24U &&
         scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p2, 0, sizeof(p2));
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (!scene_close_pose_for_step(&scene, scene.close_shot_step,
                                          &expected_pose) ||
               scene.actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(&scene) ||
               scene_test_has_close_semantic_event(&scene.events) ||
               scene.state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !scene_test_draw_exact_step(&scene)))) {
            scene_test_message(message, message_size,
                               "layup/TGCS variant-2 replay failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.controlled_actor[0] != scene.ball_holder ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 11U ||
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "layup/TGCS variant-2 settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 11U) {
        scene_test_message(message, message_size,
                           "layup crowd-only mailbox boundary failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    scene.actors[scene.ball_holder].position.x = 0x013CU;
    scene.actors[scene.ball_holder].position.y = 180;
    scene.actors[scene.ball_holder].facing_right = true;
    scene_attach_ball(&scene);
    scene.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.action_serial != 1U ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        scene_test_message(message, message_size,
                           "pressed-only NES B started a jump shot");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    /* The player may have last moved away from the offensive hoop. Launch
       ownership, not stale movement facing, must select and face the target. */
    rattle_before = scene;
    rattle_before.actors[rattle_before.ball_holder].facing_right = true;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    camera_before = rattle_before.camera_state;
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        rattle_before.shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        rattle_before.actors[rattle_before.shot_actor].facing_right ||
        rattle_before.shot_start_position.x_q8 !=
            (0x013C - 7) * 256 ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256 ||
        rattle_before.shot_end_position.y_q8 !=
            TECMO_GAMEPLAY_SHOT_TARGET_Y * 256 ||
        abs((int)rattle_before.camera_state.camera_x -
            (int)camera_before.camera_x) > 7 ||
        !tecmo_gameplay_state_valid(&rattle_before.state)) {
        scene_test_message(message, message_size,
                           "away offensive-hoop shot ownership failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256) {
        scene_test_message(message, message_size,
                           "away mirrored jump route did not advance");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    scene.actors[scene.ball_holder].facing_right = true;
    scene.actors[scene.ball_holder].position.x = 0x0108;
    scene.actors[scene.ball_holder].position.y = 0x0070;
    scene.action_serial = 1U;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.action_serial != 1U ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "ordinary two-point make was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    rattle_before = scene;
    if (!scene_handoff_possession(
            &rattle_before, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        scene_test_message(message, message_size,
                           "home-side shot ownership setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    rattle_before.actors[5].position.x = 0x01C4;
    rattle_before.actors[5].position.y = 180;
    rattle_before.actors[5].facing_right = false;
    rattle_before.action_serial = 2U;
    scene_attach_ball(&rattle_before);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    camera_before = rattle_before.camera_state;
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        rattle_before.shot_actor != 5U ||
        !rattle_before.actors[5].facing_right ||
        rattle_before.shot_start_position.x_q8 !=
            (0x01C4 + 7) * 256 ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X * 256 ||
        rattle_before.shot_end_position.y_q8 !=
            TECMO_GAMEPLAY_SHOT_TARGET_Y * 256 ||
        abs((int)rattle_before.camera_state.camera_x -
            (int)camera_before.camera_x) > 7 ||
        !tecmo_gameplay_state_valid(&rattle_before.state)) {
        scene_test_message(message, message_size,
                           "home offensive-hoop shot ownership failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X * 256) {
        scene_test_message(message, message_size,
                           "home mirrored jump route did not advance");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position.x =
        (int16_t)(scene.orientation_state.offensive_hoop.x + 50U);
    scene.actors[0].position.y = 128;
    scene.actors[0].facing_right = true;
    scene_attach_ball(&scene);
    rattle_before = scene;
    if (tecmo_gameplay_scene_start_rim_rattle_debug(&scene) ||
        memcmp(&scene, &rattle_before, sizeof(scene)) != 0) {
        scene_test_message(
            message, message_size,
            "rim-rattle rejected diagnostic mutated scene");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position.x = 0x013CU;
    scene.actors[0].position.y = 180;
    scene.actors[0].facing_right = true;
    scene_attach_ball(&scene);

    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    scene.action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene.shot_frame != 1U || scene.action_serial != 1U ||
        !scene_test_jump_make_checkpoint(&scene, 1U) ||
        scene.audio_player.dmc.active) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-make launch: shot=%u frame=%u serial=%u oracle=%u make=%u outcome=%u state=%u phase=%u alt=%u vel=%u pose=%u",
            (unsigned)scene.shot_kind, (unsigned)scene.shot_frame,
            (unsigned)scene.action_serial,
            scene.jump_oracle_active ? 1U : 0U,
            scene.jump_make_route ? 1U : 0U,
            (unsigned)scene.jump_outcome,
            (unsigned)scene.jump_actor_state,
            (unsigned)scene.jump_phase_counter,
            (unsigned)scene.jump_actor_altitude_q8,
            (unsigned)scene.jump_actor_velocity_q8,
            scene.shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)scene.actors[scene.shot_actor].pose_index
                : UINT_MAX);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 2U; frame <= 8U; ++frame) {
        memset(&p1, 0, sizeof(p1));
        p1.held.cancel = true;
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            !scene_test_jump_make_checkpoint(
                &scene, (uint16_t)frame)) {
            scene_test_message(message, message_size,
                               "ordinary-jump make held-B schedule failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 9U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        bool terminal = frame == TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (!terminal &&
             !scene_test_jump_make_checkpoint(
                 &scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "ordinary-jump make checkpoint %u failed",
                           (unsigned)frame);
            scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.controlled_actor[1] != scene.ball_holder ||
        scene.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 11U ||
        scene.events.count != 0U || scene.jump_oracle_active ||
        scene.jump_make_route ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "ordinary-jump make settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 11U) {
        scene_test_message(message, message_size,
                           "ordinary-jump make crowd-only audio failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_set_score(
            &scene.state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        scene_test_message(message, message_size,
                           "ordinary-jump early-release setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position.x = 0x013CU;
    scene.actors[0].position.y = 180;
    scene.actors[0].facing_right = true;
    scene_attach_ball(&scene);
    scene.action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_frame != 1U || !scene.jump_make_route) {
        scene_test_message(message, message_size,
                           "ordinary-jump early-release launch failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_frame != TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME ||
        !scene.jump_b_released ||
        scene.actors[scene.shot_actor].pose_index !=
            TECMO_GAMEPLAY_JUMP_RELEASE_POSE) {
        scene_test_message(message, message_size,
                           "ordinary-jump early release did not normalize");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 10U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "ordinary-jump early-release route stalled");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 11U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "ordinary-jump early-release settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        scene_test_message(message, message_size,
                           "rim-rattle diagnostic reset failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position.x = 0x013CU;
    scene.actors[0].position.y = 180;
    scene.actors[0].facing_right = true;
    scene_attach_ball(&scene);
    away_score_before = scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_start_rim_rattle_debug(&scene) ||
        scene.shot_frame != 1U ||
        scene.shot_duration != TECMO_GAMEPLAY_JUMP_RATTLE_DURATION ||
        !scene.jump_rim_rattle_debug ||
        scene.jump_rim_rattle_raw_selector != 0x71U ||
        scene.jump_rim_rattle_audio_repeats != 0U) {
        scene_test_message(message, message_size,
                           "rim-rattle diagnostic launch failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 2U; frame <= TECMO_GAMEPLAY_JUMP_RATTLE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < TECMO_GAMEPLAY_JUMP_RATTLE_DURATION &&
             !scene_test_jump_rattle_checkpoint(
                 &scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "rim-rattle checkpoint %u diverged",
                           (unsigned)frame);
            scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            away_score_before ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 12U ||
        scene.jump_rim_rattle_debug ||
        scene.jump_rim_rattle_audio_repeats != 0U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "rim-rattle diagnostic settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    if (!tecmo_gameplay_set_score(
            &scene.state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        scene_test_message(message, message_size,
                           "ordinary-jump make test reset failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0].position.x = 0x013CU;
    scene.actors[0].position.y = 180;
    scene.actors[0].facing_right = true;
    scene_attach_ball(&scene);

    /* Slot 0 follows the implementation-owned serial-2 predicted-miss branch.
       Audio from earlier close-shot coverage must not mask the no-release-DMC
       test. */
    scene.action_serial = 1U;
    away_score_before = scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene.shot_frame != 1U || scene.shot_controller != 0U ||
        scene.action_serial != 2U || !scene.jump_oracle_active ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        scene.jump_b_released ||
        scene.jump_actor_state != 0x0CU ||
        scene.jump_ball_state != 0x01U ||
        scene.jump_phase_counter != 0x31U ||
        scene.jump_pose_frame != 1U ||
        scene.jump_entry_pose_index ==
            TECMO_GAMEPLAY_JUMP_FLIGHT_POSE ||
        scene.jump_actor_altitude_q8 != 0x02E8U ||
        scene.jump_actor_velocity_q8 != 0x02E8U ||
        scene.actors[scene.shot_actor].pose_index !=
            scene.jump_entry_pose_index ||
        scene.audio_player.dmc.active) {
        char failure[256];
        (void)snprintf(
            failure, sizeof(failure),
            "NES B jump launch failed: shot=%u frame=%u controller=%u serial=%u oracle=%u make=%u outcome=%u released=%u actor=%u ball=%u phase=%u pose_frame=%u entry=%u pose=%u alt=%u vel=%u dmc=%u",
            (unsigned)scene.shot_kind, (unsigned)scene.shot_frame,
            (unsigned)scene.shot_controller, (unsigned)scene.action_serial,
            scene.jump_oracle_active ? 1U : 0U,
            scene.jump_make_route ? 1U : 0U,
            (unsigned)scene.jump_outcome,
            scene.jump_b_released ? 1U : 0U,
            (unsigned)scene.jump_actor_state,
            (unsigned)scene.jump_ball_state,
            (unsigned)scene.jump_phase_counter,
            (unsigned)scene.jump_pose_frame,
            (unsigned)scene.jump_entry_pose_index,
            scene.shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)scene.actors[scene.shot_actor].pose_index
                : UINT_MAX,
            (unsigned)scene.jump_actor_altitude_q8,
            (unsigned)scene.jump_actor_velocity_q8,
            scene.audio_player.dmc.active ? 1U : 0U);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    jump_entry_pose = scene.jump_entry_pose_index;
    draw_probe = scene;
    draw_probe.jump_pose_frame = 0U;
    rattle_before = draw_probe;
    if (scene_update_jump_miss(&draw_probe, &p1) ||
        memcmp(&draw_probe, &rattle_before, sizeof(draw_probe)) != 0) {
        scene_test_message(message, message_size,
                           "malformed jump pose counter mutated playback");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 2U;
         frame <= TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME; ++frame) {
        uint16_t windup_pose =
            frame <= TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME
                ? jump_entry_pose
                : TECMO_GAMEPLAY_JUMP_TURN_POSE;
        memset(&p1, 0, sizeof(p1));
        p1.held.cancel = true;
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.shot_frame != 1U || scene.jump_b_released ||
            scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
            scene.jump_actor_state != 0x0CU ||
            scene.jump_ball_state != 0x01U ||
            scene.jump_pose_frame != frame ||
            scene.actors[scene.shot_actor].pose_index != windup_pose ||
            scene.jump_actor_altitude_q8 != 0x02E8U ||
            scene.jump_actor_velocity_q8 != 0x02E8U ||
            scene.audio_player.dmc.active) {
            scene_test_message(message, message_size,
                               "current-B held jump pose diverged");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_frame != 2U || !scene.jump_b_released ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        scene.jump_actor_state != 0x0DU ||
        scene.jump_ball_state != 0x05U ||
        scene.jump_phase_counter != 0x04U ||
        scene.jump_pose_frame != 9U ||
        scene.actors[scene.shot_actor].pose_index != 1061U ||
        scene.jump_actor_altitude_q8 != 0x02E8U ||
        scene.jump_actor_velocity_q8 != 0x02E8U ||
        scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "current-B jump release diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 3U; frame <= 87U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < 87U &&
             !scene_test_jump_slot0_checkpoint(&scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "jump-shot checkpoint %u diverged",
                           (unsigned)frame);
            scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        if (frame < 87U &&
            scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
            scene_test_message(message, message_size,
                               "jump-shot actor/ball lifetime ended early");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.controlled_actor[1] != scene.ball_holder ||
        scene.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        scene.state.clock_divider !=
            TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 12U ||
        scene.events.count != 0U || scene.jump_oracle_active ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-shot settlement failed: shot=%u score=%u-%u possession=%u holder=%u clock=%u/%u sfx=%u/%u events=%u outcome=%u",
            (unsigned)scene.shot_kind,
            (unsigned)scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY],
            (unsigned)scene.state.score[TECMO_GAMEPLAY_TEAM_HOME],
            (unsigned)scene.state.possession, (unsigned)scene.ball_holder,
            (unsigned)scene.state.shot_clock,
            (unsigned)scene.state.clock_divider,
            scene.audio_player.sfx_pending ? 1U : 0U,
            (unsigned)scene.audio_player.pending_sfx_id,
            (unsigned)scene.events.count, (unsigned)scene.jump_outcome);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 12U) {
        scene_test_message(message, message_size,
                           "jump-miss side-result mailbox was not consumed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    scene.state.clock_minutes = 0U;
    scene.state.clock_seconds = 1U;
    if (!scene_shot_queue_result_audio(&scene, TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 11U) {
        scene_test_message(message, message_size,
                           "side-result clock gate below two seconds failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    if (scene.active || scene.result_ready || !scene.available) {
        scene_test_message(message, message_size,
                           "scene end lifecycle contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_test_close_clock_collision(&scene, &launch)) {
        scene_test_message(
            message, message_size,
            "close-shot countdown/dual-expiry settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "single-controller gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    scene.fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 1U;
    scene.fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 10U;
    scene.actors[0U].condition = 10U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.fatigue_state.capacity[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene.fatigue_state.countdown[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene.fatigue_state.condition[TECMO_GAMEPLAY_TEAM_AWAY][0U] != 9U ||
        scene.actors[0U].condition != 9U) {
        scene_test_message(message, message_size,
                           "TGFT-1 live condition synchronization failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[0U].position.x = 149;
    scene.actors[0U].position.y = 148;
    scene.actors[0U].movement_fractional_accumulator = 15U;
    p1.held.left = true;
    if (!scene_attach_ball(&scene) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.actors[0U].position.x != 149 ||
        scene.actors[0U].movement_boundary_latched ||
        scene.actors[0U].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT) {
        scene_test_message(message, message_size,
                           "TGMO out-of-bounds approach setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene.state.violation != TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        scene.state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.actors[0U].position.x != 149 ||
        scene.actors[0U].movement_boundary_latched ||
        scene.ball_holder != 0U ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 6U) {
        char failure[384];
        (void)snprintf(
            failure, sizeof(failure),
            "TGMO/TPNL out-of-bounds entry failed: phase=%u violation=%u restart=%u x=%d latch=%u holder=%u sfx=%u/%u control=%u team=%u action=%u direction=%u shot=%u",
            (unsigned)scene.state.phase,
            (unsigned)scene.state.violation,
            (unsigned)scene.state.restart_possession,
            (int)scene.actors[0U].position.x,
            scene.actors[0U].movement_boundary_latched ? 1U : 0U,
            (unsigned)scene.ball_holder,
            scene.audio_player.sfx_pending ? 1U : 0U,
            (unsigned)scene.audio_player.pending_sfx_id,
            (unsigned)scene.controlled_actor[0U],
            (unsigned)scene.launch.controller_team[0U],
            (unsigned)scene.actors[0U].movement_action_state,
            (unsigned)scene.actors[0U].movement_direction,
            (unsigned)scene.shot_kind);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "out-of-bounds lead-in failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        scene.state.clock_divider != TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        scene.ball_holder != TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.actors[0U].movement_boundary_latched) {
        scene_test_message(message, message_size,
                           "out-of-bounds restart settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&scene.audio_player);
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        scene_test_message(message, message_size,
                           "out-of-bounds test possession reset failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.shot_clock = 1U;
    scene.state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 6U ||
        scene.audio_player.dmc.active ||
        scene.audio_player.music == NULL ||
        scene.audio_player.music->playing ||
        scene.audio_player.music->track_pending) {
        scene_test_message(message, message_size,
                           "shot-clock violation reset/cue ordering failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 6U ||
        !tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        scene_test_message(message, message_size,
                           "single violation cue consumption failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.audio_player.sfx_pending) {
            scene_test_message(message, message_size,
                               "violation reset/cue repeated after entry");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 5U ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "violation restart holder synchronization failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 5U ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "violation live-return audio restart failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "violation restart cue repeated");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    /* Allow the exact fatigue path to reach TGMO's minimum Q4 rate while
       retaining a bound below the 24-second possession clock. */
    for (frame = 0U; frame < 600U &&
         scene.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "native offense update failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.shot_actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        scene_test_message(message, message_size,
                           "native offense did not start a shot");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    if (!scene_test_cpu_offense_all_difficulties(
            &scene, &launch, &failed_difficulty)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "CPU offense stalled before a close shot at difficulty %u",
            (unsigned)failed_difficulty);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!scene_test_combined_restart_is_inert(&scene, &launch, 1U) ||
        !scene_test_combined_restart_is_inert(&scene, &launch, 3U)) {
        scene_test_message(
            message, message_size,
            "combined violation restart action suppression failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_test_jump_period_expiry(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "jump-miss period-expiry route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_test_jump_make_period_expiry(&scene, &launch, true) ||
        !scene_test_jump_make_period_expiry(&scene, &launch, false)) {
        scene_test_message(message, message_size,
                           "jump-make period-expiry route failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "period-expiry gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.clock_minutes = 0U;
    scene.state.clock_seconds = 1U;
    scene.state.clock_divider = 2U;
    scene.state.shot_clock = 20U;
    scene.actors[scene.ball_holder].position.x = 0x013CU;
    scene.actors[scene.ball_holder].position.y = 180;
    scene.actors[scene.ball_holder].facing_right = true;
    scene_attach_ball(&scene);
    scene.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene.shot_frame != 1U || scene.action_serial != 2U ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        scene_test_message(message, message_size,
                           "period-expiry live shot setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    shot_actor = scene.shot_actor;
    away_score_before = scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    home_score_before = scene.state.score[TECMO_GAMEPLAY_TEAM_HOME];
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U; frame < 96U &&
         scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "period-expiry shot update failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !scene.state.period_expiry_zero_action_observed ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 11U ||
        scene.ball_holder != shot_actor ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "period-expiry shot settlement diverged: shot=%u phase=%u holder=%u possession=%u valid=%u",
            (unsigned)scene.shot_kind, (unsigned)scene.state.phase,
            (unsigned)scene.ball_holder, (unsigned)scene.state.possession,
            tecmo_gameplay_state_valid(&scene.state) ? 1U : 0U);
        scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene.state.period != 2U ||
        scene.state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        scene.state.period_expiry_zero_action_observed ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != home_score_before ||
        scene.ball_holder != shot_actor ||
        scene.audio_player.sfx_pending || scene.audio_player.dmc.active ||
        scene.audio_player.music == NULL ||
        scene.audio_player.music->playing ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "period-expiry audio reset transition failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        scene_test_message(message, message_size,
                           "period exact-once DMC probe failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    scene.ball_holder = 5U;
    scene_attach_ball(&scene);
    scene.action_serial = 3U;
    x = scene.actors[0].position.x;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.ball_holder != 0U || scene.controlled_actor[0] != 0U ||
        scene.actors[0].position.x != x || scene.action_serial != 3U ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 5U ||
        !scene.audio_player.dmc.active ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "period restart action suppression failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (!scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "period live-return music restart failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_AWAY;
    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        scene.controlled_actor[0] < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.controlled_actor[1] >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        scene_test_message(message, message_size,
                           "swapped controller ownership mapping failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    x = scene.actors[scene.controlled_actor[0]].position.x;
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].position.x != x + 1) {
        scene_test_message(message, message_size,
                           "swapped controller TGMO movement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "combined-button gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    /* Keep the A-selected receiver inside the supported TGJS/TGSR slot-0 miss
       context so this test remains about A-before-B resolution. */
    scene.actors[1].position.x = 0x013CU;
    scene.actors[1].position.y = 180;
    scene.actors[1].facing_right = true;
    scene.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.shot_actor != 1U || scene.action_serial != 2U ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) {
        scene_test_message(message, message_size,
                           "combined NES A+B resolution failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    /* TGBD follows the actual holder's TGMO animation phase. Unrelated pad
       activity cannot trigger it; a stationary holder still dribbles. */
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "defender dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "defender movement queued holder DMC");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "NO_TEAM dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    p2.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "NO_TEAM pad movement queued holder DMC");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "human holder dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame <= 14U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < 14U &&
             scene.audio_player.dmc.active)) {
            scene_test_message(
                message, message_size,
                "human holder TGBD phase queued an early DMC");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        if ((frame == 0U &&
             scene.ball_position.y_q8 != 176 * 256) ||
            (frame == 3U &&
             scene.ball_position.y_q8 != 182 * 256) ||
            (frame == 7U &&
             scene.ball_position.y_q8 != 191 * 256) ||
            (frame == 11U &&
             scene.ball_position.y_q8 != 197 * 256)) {
            scene_test_message(
                message, message_size,
                "human holder TGBD visible bounce vector failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!scene.audio_player.dmc.active) {
        scene_test_message(
            message, message_size,
            "stationary human holder missed native TGBD phase DMC");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_HOME;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "CPU holder dribble-policy launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    cpu_holder_start_x = scene.actors[scene.ball_holder].position.x;
    for (frame = 0U; frame <= 17U; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < 17U &&
             scene.audio_player.dmc.active)) {
            scene_test_message(
                message, message_size,
                "CPU holder TGBD phase queued an early DMC");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
        if (frame == 0U &&
            (scene.actors[0].position.x != cpu_holder_start_x ||
             scene.actors[0].movement_action_state !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             scene.cpu_actors[0].decision_serial != 1U ||
             scene.cpu_actors[0].snapshot_fingerprint != 0xBD36E345U ||
             scene.cpu_actors[0].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH ||
             scene.cpu_actors[0].target_position.x != 208 ||
             scene.cpu_actors[0].target_position.y != 148 ||
             scene.cpu_actors[0].direction != 1U ||
             scene.cpu_actors[0].held_direction_bits !=
                 TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT ||
             !scene.cpu_actors[0].writes_direction ||
             scene.cpu_actors[1].decision_serial != 1U ||
             scene.cpu_actors[1].target_kind !=
                 TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR ||
             scene.cpu_actors[1].linked_actor != 6U ||
             scene.cpu_actors[1].target_position.x != 395 ||
             scene.cpu_actors[1].target_position.y != 190 ||
             scene.cpu_actors[5].decision_serial != 0U)) {
            scene_test_message(
                message, message_size,
                "live TGAI snapshot/target/TGMO-latency contract failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!scene.audio_player.dmc.active ||
        scene.actors[0].position.x >= cpu_holder_start_x ||
        scene.cpu_actors[0].decision_serial !=
            18U) {
        scene_test_message(message, message_size,
                           "CPU holder missed native TGBD phase DMC");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;

    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "music-off restart launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.shot_clock = 1U;
    scene.state.clock_divider = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 6U) {
        scene_test_message(message, message_size,
                           "music-off violation entry failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "music-off violation cue was not consumed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_VIOLATION_RELEASE_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "music-off violation lead-in failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "music-off restart queued neutral cue");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.game_music_enabled = true;

    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "native steal-policy launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[scene.controlled_actor[1]].position.x =
        scene.actors[scene.ball_holder].position.x + 1;
    scene.actors[scene.controlled_actor[1]].position.y =
        scene.actors[scene.ball_holder].position.y;
    scene.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.action_serial != 2U ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.ball_holder != scene.controlled_actor[1]) {
        scene_test_message(message, message_size,
                           "native action-serial steal policy diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        !scene_test_free_throw_lineup_unbound(&scene)) {
        scene_test_message(message, message_size,
                           "foul/free-throw gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[scene.controlled_actor[1]].position.x =
        scene.actors[scene.ball_holder].position.x + 1;
    scene.actors[scene.controlled_actor[1]].position.y =
        scene.actors[scene.ball_holder].position.y;
    scene.action_serial = 3U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene.state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene.state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME][0] != 1U ||
        scene.action_serial != 4U ||
        scene.audio_player.sfx_pending || scene.audio_player.dmc.active ||
        scene.audio_player.music == NULL ||
        scene.audio_player.music->playing ||
        scene.audio_player.music->track_pending) {
        scene_test_message(message, message_size,
                           "foul entry audio reset/policy diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &scene.audio_player,
            TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG)) {
        scene_test_message(message, message_size,
                           "foul exact-once DMC probe failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            !scene.audio_player.dmc.active) {
            scene_test_message(message, message_size,
                               "foul audio reset repeated after entry");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.released.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 2U ||
        scene.free_throw_frame != 0U || scene.audio_player.sfx_pending ||
        !scene_test_free_throw_lineup_bound(
            &scene, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        scene.audio_player.music == NULL ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "foul dismissal/free-throw handoff failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending) {
        scene_test_message(message, message_size,
                           "free-throw setup music was not consumed");
        tecmo_gameplay_scene_destroy(&scene);
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
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 2U ||
        scene.free_throw_frame != 0U || scene.action_serial != 4U ||
        scene.audio_player.sfx_pending ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "non-owner/free-throw non-B input launched");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame < TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES * 2U;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene.state.free_throws.attempts_remaining != 2U ||
            scene.free_throw_frame != 0U || scene.action_serial != 4U ||
            scene.audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&scene.state)) {
            scene_test_message(message, message_size,
                               "human free throw gained a timer fallback");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.free_throw_frame != 0U || scene.action_serial != 5U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !scene_test_free_throw_lineup_bound(
            &scene, 0U, 0U, 5U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X) ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 12U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "owned held-B free throw did not launch");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 12U) {
        scene_test_message(message, message_size,
                           "made away free-throw mailbox was not side-result 12");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.free_throw_frame != 0U || scene.action_serial != 5U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "pressed-only free throw input launched");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.free_throws.attempts_remaining != 0U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.free_throw_frame != 0U || scene.action_serial != 6U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene.ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.controlled_actor[1] != scene.ball_holder ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 12U ||
        scene.audio_player.music == NULL ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending ||
        !scene_test_free_throw_lineup_unbound(&scene) ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "human free-throw settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 12U ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 12U ||
        scene.audio_player.music->track_pending ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "final human free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.free_throw_frame = 7U;
    tecmo_gameplay_scene_end(&scene);
    if (scene.free_throw_frame != 0U) {
        scene_test_message(message, message_size,
                           "free-throw timer survived scene end");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    /* Home ownership uses its assigned pad, independently of controller index. */
    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        scene.free_throw_frame != 0U ||
        !scene_test_enter_free_throw_sequence(
            &scene, TECMO_GAMEPLAY_TEAM_HOME, 1U)) {
        scene_test_message(message, message_size,
                           "home free-throw ownership setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.action_serial != 0U || scene.free_throw_frame != 0U) {
        scene_test_message(message, message_size,
                           "home free throw accepted the wrong pad/edge");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.free_throws.attempts_remaining != 0U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.controlled_actor[0] != scene.ball_holder ||
        scene.action_serial != 1U || scene.free_throw_frame != 0U ||
        !scene_test_free_throw_lineup_unbound(&scene) ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 13U ||
        scene.audio_player.music == NULL ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "home owned held-B free throw failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 13U ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 13U ||
        scene.audio_player.music->track_pending ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "final home free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    /* With no controller assigned to the scoring side, use the observed
       125-update launch schedule and reset it for the following attempt. */
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        !scene_test_enter_free_throw_sequence(
            &scene, TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        scene_test_message(message, message_size,
                           "CPU free-throw timer setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene.state.free_throws.attempts_remaining != 2U ||
            scene.free_throw_frame != frame + 1U ||
            scene.action_serial != 0U || scene.audio_player.sfx_pending ||
            !tecmo_gameplay_state_valid(&scene.state)) {
            scene_test_message(message, message_size,
                               "CPU free throw launched before observed schedule");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.free_throw_frame != 0U || scene.action_serial != 1U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        !scene_test_free_throw_lineup_bound(
            &scene, 1U, 5U, 0U,
            TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X) ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 13U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "CPU free throw missed observed launch update");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 13U) {
        scene_test_message(message, message_size,
                           "missed home free-throw mailbox was not side-result 13");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.free_throw_frame != 1U || scene.action_serial != 1U) {
        scene_test_message(message, message_size,
                           "second CPU free-throw timer did not reset");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 1U;
         frame + 1U <
             TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene.state.free_throws.attempts_remaining != 1U ||
            scene.free_throw_frame != frame + 1U ||
            scene.action_serial != 1U || scene.audio_player.sfx_pending) {
            scene_test_message(message, message_size,
                               "second CPU free throw launched early");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.free_throws.attempts_remaining != 0U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 0U ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.controlled_actor[0] != scene.ball_holder ||
        scene.free_throw_frame != 0U || scene.action_serial != 2U ||
        !scene_test_free_throw_lineup_unbound(&scene) ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 13U ||
        scene.audio_player.music == NULL ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "CPU free-throw settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 13U ||
        !scene.audio_player.music->playing ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY ||
        scene.audio_player.music->track_pending ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 13U ||
        scene.audio_player.music->track_pending ||
        scene.audio_player.music->current_track_id !=
            TECMO_MUSIC_TRACK_GAMEPLAY) {
        scene_test_message(message, message_size,
                           "final CPU free-throw audio repeated or missing");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    launch.controller_team[1] = TECMO_GAMEPLAY_TEAM_HOME;

    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "halftime gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.period = 2U;
    scene.state.clock_minutes = 0U;
    scene.state.clock_seconds = 1U;
    scene.state.clock_divider = 1U;
    scene.state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT) {
        scene_test_message(message, message_size,
                           "halftime expiry entry failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 40U &&
         scene.state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "halftime expiry wait failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER) {
        scene_test_message(message, message_size,
                           "halftime banner transition failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES &&
         scene.state.phase == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "halftime banner update failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN ||
        !scene.audio_player.music->track_pending ||
        scene.audio_player.music->pending_track_id !=
            TECMO_MUSIC_TRACK_PRESENTATION) {
        scene_test_message(message, message_size,
                           "halftime score/music transition failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        scene.state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD) {
        scene_test_message(message, message_size,
                           "halftime dismissal failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

    if (!tecmo_gameplay_scene_launch(&scene, &launch) ||
        !tecmo_gameplay_set_score(&scene.state,
                                  TECMO_GAMEPLAY_TEAM_AWAY, 4U) ||
        !tecmo_gameplay_set_score(&scene.state,
                                  TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
        scene_test_message(message, message_size,
                           "final gameplay setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.period = 4U;
    scene.state.clock_minutes = 0U;
    scene.state.clock_seconds = 1U;
    scene.state.clock_divider = 1U;
    scene.state.shot_clock = 12U;
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
        scene_test_message(message, message_size,
                           "final expiry entry failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 40U &&
         scene.state.phase ==
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "final expiry wait failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        scene_test_message(message, message_size,
                           "final score transition failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    p1.released.shoot = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !tecmo_gameplay_scene_result(&scene, &result) ||
        result.source != launch.source ||
        result.game_index != launch.game_index ||
        result.away_team != launch.away_team ||
        result.home_team != launch.home_team ||
        result.away_score != 4U || result.home_score != 2U) {
        scene_test_message(message, message_size,
                           "final result handoff failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);
    tecmo_gameplay_scene_destroy(&scene);
    scene_self_test_skip_pretip = false;
    scene_test_message(message, message_size,
                       "GAMEPLAY SCENE SELF TEST PASS");
    return true;
}
