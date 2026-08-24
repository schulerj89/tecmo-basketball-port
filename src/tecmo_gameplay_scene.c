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
    tecmo_gameplay_rebound_audit_destroy(&scene->rebound_audit);
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
    tecmo_gameplay_rebound_audit_init(&scene->rebound_audit);
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
    tecmo_gameplay_rebound_audit_init(&scene->rebound_audit);
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
    if (!tecmo_gameplay_rebound_audit_load(&scene->rebound_audit, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->rebound_audit.status);
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
                     "native gameplay ready: TPTI-2/TGPL-1/TTDT-1/TWAR-1/TMUS-1/TGCT-1/TGCP-2/TGMO-1/TGBD-1/TGAI-3/TGFT-1/TPNL-1/TGVR-1/TGOR-1/TGFL-1/THUD-1/TGCS-1/TGDK-1/TGJS-2/TGSR-4/TGRB-1/TSFX-1/TDMC-1");
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

static bool scene_starter_binding_valid(
    const TecmoGameplaySceneLaunch *launch)
{
    bool seen[TECMO_TEAM_DATA_PLAYERS_PER_TEAM];
    size_t side;
    size_t starter;
    if (launch == NULL) return false;
    /* False is the documented source/default-initializer identity-lineup
       path for
       direct scene/test/render callers. Their zeroed arrays are normalized
       below before the launch is stored. */
    if (!launch->starter_binding_bound) return true;
    for (side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
        memset(seen, 0, sizeof(seen));
        for (starter = 0U;
             starter < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT; ++starter) {
            uint8_t roster_index =
                launch->starter_roster_index[side][starter];
            if (roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM ||
                seen[roster_index]) {
                return false;
            }
            seen[roster_index] = true;
        }
    }
    return true;
}

static bool scene_launch_prepare(
    const TecmoGameplaySceneLaunch *launch,
    TecmoGameplaySceneLaunch *prepared)
{
    TecmoGameplayConfig config;
    if (launch == NULL || prepared == NULL ||
        !scene_source_valid(launch->source) ||
        launch->away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->home_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->away_team == launch->home_team || launch->difficulty > 2U ||
        launch->control_mode > 6U || launch->speed_value > 2U ||
        !scene_court_controller_team_valid(launch->controller_team[0]) ||
        !scene_court_controller_team_valid(launch->controller_team[1]) ||
        (launch->controller_team[0] != TECMO_GAMEPLAY_SCENE_NO_TEAM &&
         launch->controller_team[0] == launch->controller_team[1]) ||
        !scene_starter_binding_valid(launch)) {
        return false;
    }
    if (!tecmo_gameplay_config_init(&config,
                                    launch->regulation_minutes)) {
        return false;
    }
    *prepared = *launch;
    if (!prepared->starter_binding_bound) {
        for (size_t side = 0U; side < TECMO_GAMEPLAY_TEAM_COUNT; ++side) {
            for (size_t starter = 0U;
                 starter < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
                 ++starter) {
                prepared->starter_roster_index[side][starter] =
                    (uint8_t)starter;
            }
        }
        prepared->starter_binding_bound = true;
    }
    return true;
}

static bool scene_launch_valid(const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplaySceneLaunch prepared;
    return scene_launch_prepare(launch, &prepared);
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
    /* Bank04 $AC76-$ADDF player-object setup in stable scene topology. The
       staged $6023->$7B2E values select each side; the staged-entry to stable
       slot mapping is native-faithful/inferred, not directly proven. */
    static const TecmoGameplayCourtCoordinate
        initial_positions[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
            {528, 144}, {448, 144}, {362, 112}, {364, 192}, {392, 144},
            {176, 144}, {320, 144}, {408, 112}, {400, 192}, {372, 144}
    };
    static const TecmoGameplayCourtCoordinate
        legacy_positions[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
            {0x0160, 198}, {0x017C, 167}, {0x0197, 207},
            {0x01B2, 151}, {0x01CF, 183}, {0x016F, 214},
            {0x018B, 190}, {0x01A6, 169}, {0x01C2, 205},
            {0x01DE, 145}
    };
    static const uint8_t initial_directions[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
        1U, 1U, 2U, 5U, 1U, 0U, 0U, 2U, 5U, 0U
    };
    static const uint8_t fixed_links[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
        5U, 6U, 7U, 8U, 9U, 0U, 1U, 2U, 3U, 4U
    };
    TecmoGameplaySceneActor
        initialized[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        initialized_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t controlled[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t actor_team[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    const TecmoGameplayCourtCoordinate *positions;
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
    positions = scene->legacy_direct_launch
                    ? legacy_positions : initial_positions;
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
        item->position = positions[actor];
        item->anchor = positions[actor];
        item->team = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                         ? TECMO_GAMEPLAY_TEAM_AWAY
                         : TECMO_GAMEPLAY_TEAM_HOME;
        actor_team[actor] = item->team;
        item->roster_index = scene->launch.starter_roster_index[
            item->team][actor % TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT];
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
        linked_actor = fixed_links[actor];
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
                scene->legacy_direct_launch
                    ? (item->facing_right ? 0U : 1U)
                    : initial_directions[actor]) ||
            !scene_movement_pose_index(
                scene, &movement, &positions[linked_actor],
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
    if (!tecmo_gameplay_live_foundation_initialize(
            &scene->cpu_steering_assets, positions,
            scene->orientation_state.attack_direction,
            (uint8_t)scene->state.possession, scene->ball_holder,
            actor_team, scene->launch.controller_team, controlled,
            &scene->live_foundation)) {
        return false;
    }
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
        scene->pretip_jumper_selector[jumper] =
            lineup.player_facings[actor_index];
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

static bool scene_pretip_pose_base(uint8_t selector, uint16_t *base_out)
{
    static const uint16_t bases[8] = {
        579U, 547U, 563U, 571U, 555U, 531U, 587U, 539U
    };
    if (base_out == NULL || selector >= 8U) return false;
    *base_out = bases[selector];
    return true;
}

static bool scene_pretip_landing_pose(uint8_t selector, uint16_t *pose_out)
{
    static const uint16_t poses[8] = {
        501U, 469U, 485U, 493U, 477U, 453U, 509U, 461U
    };
    if (pose_out == NULL || selector >= 8U) return false;
    *pose_out = poses[selector];
    return true;
}

static uint8_t scene_pretip_animation_phase(
    uint16_t phase_frame, uint16_t commit_frame)
{
    uint16_t age = phase_frame > commit_frame
        ? (uint16_t)(phase_frame - commit_frame - 1U) : 0U;
    if (age == 0U) return 2U;
    if (age == 1U) return 3U;
    return 4U;
}

static bool scene_pretip_apply_jump_frame(
    TecmoGameplayScene *scene,
    uint16_t phase_frame)
{
    bool any_committed = false;
    size_t jumper;
    (void)phase_frame;
    if (scene == NULL ||
        !scene->pretip_state.simulation_active ||
        !scene_pretip_jumper_mapping_valid(scene)) {
        return false;
    }
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        bool committed = jumper == 0U
            ? scene->pretip_state.away_jump_committed
            : scene->pretip_state.home_jump_committed;
        uint16_t pose_base;
        uint8_t animation_phase;
        uint8_t actor_state = jumper == 0U
            ? scene->pretip_state.away_actor_state
            : scene->pretip_state.home_actor_state;
        TecmoGameplaySceneActor *actor = &scene->actors[
            scene->pretip_jumper_actor[jumper]];
        actor->position = actor->anchor;
        if (!committed) {
            scene->pretip_jumper_altitude_q8[jumper] = 0U;
            continue;
        }
        if (actor_state == 0x13U) {
            if (!scene_pretip_landing_pose(
                    scene->pretip_jumper_selector[jumper],
                    &actor->pose_index)) return false;
            actor->pose_orientation_encoded = true;
            scene->pretip_jumper_altitude_q8[jumper] = 0U;
            continue;
        }
        any_committed = true;
        if (!scene_pretip_pose_base(
                scene->pretip_jumper_selector[jumper], &pose_base)) {
            return false;
        }
        animation_phase = jumper == 0U
            ? scene->pretip_state.away_animation_phase
            : scene->pretip_state.home_animation_phase;
        actor->pose_index = (uint16_t)(pose_base + animation_phase);
        actor->pose_orientation_encoded = true;
        scene->pretip_jumper_altitude_q8[jumper] =
            jumper == 0U ? scene->pretip_state.away_jump_altitude_q8
                         : scene->pretip_state.home_jump_altitude_q8;
    }
    scene->pretip_jump_active = any_committed;
    return true;
}

bool tecmo_gameplay_scene_launch(TecmoGameplayScene *scene,
                                 const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayConfig config;
    TecmoGameplaySceneLaunch prepared_launch;
    TecmoGameplayState initial_state;
    TecmoGameplayCourtOrientationState initial_orientation;
    TecmoGameplayCameraState initial_camera;
    TecmoGameplayBackcourtState initial_backcourt;
    TecmoGameplaySceneCourtCoordinates initial_coordinates;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available ||
        !scene_launch_prepare(launch, &prepared_launch) ||
        !tecmo_gameplay_config_init(&config, launch->regulation_minutes)) {
        return false;
    }
    scene->launch = prepared_launch;
    scene->legacy_direct_launch = !launch->starter_binding_bound;
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
            scene->orientation_state.attack_direction, false) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state)) {
        scene_set_status(scene, "gameplay live camera initialization rejected");
        return false;
    }
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene_pass_clear(scene);
    scene_inbound_clear(scene);
    scene_loose_ball_clear(scene);
    scene->close_shot_step = 0U;
    scene->close_shot_profile = TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0;
    scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    scene_shot_clear_jump_playback(scene);
    scene->frame = 0U;
    scene->action_serial = 0U;
    scene->camera_follow_count = 0U;
    scene->free_throw_frame = 0U;
    memset(&scene->foul_presentation, 0,
           sizeof(scene->foul_presentation));
    memset(&scene->claimant_settlement_trace, 0,
           sizeof(scene->claimant_settlement_trace));
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
        size_t guard = 0U;
        /* Keep the legacy skip fixture fast at the scene boundary, but derive
           its LIVE state through the real transactional TPTI-2 API. This
           deterministic both-automatic route proves the full RNG/capture,
            claim, and pre-tip-to-LIVE invariants instead of fabricating fields. */
        while (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE &&
               guard < 700U) {
            if (!tecmo_gameplay_pretip_update_controlled(
                    &scene->pretip_assets, &scene->pretip_state,
                    false, false, true, true)) {
                scene_set_status(
                    scene, "skip-PRETIP transactional normalization rejected");
                scene->active = false;
                return false;
            }
            ++guard;
        }
        if (scene->pretip_state.phase != TECMO_GAMEPLAY_PRETIP_LIVE ||
            scene->pretip_state.first_cinematic_frame == UINT16_MAX ||
            scene->pretip_state.total_frame !=
                (uint32_t)scene->pretip_state.first_cinematic_frame + 90U ||
            !tecmo_gameplay_pretip_state_validate(
                &scene->pretip_assets, &scene->pretip_state)) {
            scene_set_status(scene, "skip-PRETIP valid-state regression failed");
            scene->active = false;
            return false;
        }
        if (!scene_initialize_actors(scene)) {
            scene_set_status(scene,
                             "self-test actor movement initialization rejected");
            scene->active = false;
            return false;
        }
        scene_pass_clear(scene);
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
    tecmo_player_stats_game_ledger_initialize(&scene->player_stats);
    tecmo_player_stats_game_ledger_initialize(&scene->result.player_stats);
    scene_set_status(scene, "native pre-tip active");
    return true;
}

bool tecmo_gameplay_scene_bind_opcode10_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneOpcode10FrameContext *context)
{
    if (scene == NULL || context == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        context->contract_tag !=
            TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG ||
        !context->available || context->sample_6a == 0U ||
        context->rate_index_075f >= 3U) {
        return false;
    }
    scene->opcode10_frame_context = *context;
    return true;
}

bool tecmo_gameplay_scene_bind_opcode16_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneOpcode16FrameContext *context)
{
    TecmoGameplayCpuSteeringPlayInput input;
    if (scene == NULL || context == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    if (!scene_cpu_opcode16_workspace_project(
            scene, &scene->live_foundation, context, &input) ||
        !input.pointer_workspace_valid) {
        return false;
    }
    scene->opcode16_frame_context = *context;
    return true;
}

bool tecmo_gameplay_scene_bind_a023_latch_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneA023LatchFrameContext *context)
{
    const TecmoGameplayActorCommandAssignmentSameFrameLatch *latch;
    if (scene == NULL || context == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        context->contract_tag !=
            TECMO_GAMEPLAY_SCENE_A023_LATCH_FRAME_CONTEXT_TAG ||
        !context->available) {
        return false;
    }
    latch = &context->latch;
    if (latch->contract_tag !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_TAG ||
        !latch->valid || latch->target.depth > UINT8_MAX ||
        latch->immediate_opcode20_actor_mask == 0U ||
        (latch->immediate_opcode20_actor_mask & ~0x03FFU) != 0U ||
        (latch->producer_kind !=
             TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721 &&
         latch->producer_kind !=
             TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B783) ||
        (latch->producer_kind ==
             TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B783 &&
         !latch->b783_bit20_clear_follows_assignment) ||
        (latch->producer_kind ==
             TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LATCH_PRODUCER_B721 &&
         latch->b783_bit20_clear_follows_assignment)) {
        return false;
    }
    scene->a023_latch_frame_context = *context;
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
        scene->orientation_state.attack_direction >=
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
            scene->orientation_state.attack_direction,
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
            scene->orientation_state.attack_direction, false) ||
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
        scene->orientation_state.attack_direction;
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
            !scene_begin_inbound(scene, restart)) {
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
            if (!tecmo_player_stats_game_ledger_valid(
                    &scene->player_stats)) {
                return false;
            }
            scene->result.source = scene->launch.source;
            scene->result.game_index = scene->launch.game_index;
            scene->result.away_team = scene->launch.away_team;
            scene->result.home_team = scene->launch.home_team;
            scene->result.away_score =
                scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
            scene->result.home_score =
                scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
            scene->result.overtime_count = scene->state.overtime_count;
            scene->result.player_stats = scene->player_stats;
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
    if ((after == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
         after == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION)) {
        TecmoGameplayPenaltyPresentation presentation;
        TecmoGameplayPenaltyPresentationKind kind =
            after == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION
                ? TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL
                : TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION;
        uint8_t selector = after == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION
                               ? 0U
                               : (uint8_t)scene->state.violation;
        if (tecmo_gameplay_penalties_get_presentation(
                &scene->penalty_assets, kind, selector, &presentation) &&
            scene->state.phase_frame ==
                presentation.presentation_sfx_delay_frames) {
            (void)tecmo_gameplay_audio_queue_sfx_id(
                &scene->audio_player, presentation.presentation_sfx_id);
        }
    }
    if (before == after) return;
    if (after == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
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
    TecmoGameplayScene previous_scene;
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    size_t controller;
    bool launch_attempt;
    bool made;
    uint8_t shooter;
    TecmoGameplayTeam scoring_team;

    /* CPU launch bookkeeping may advance free_throw_frame before the launch
       threshold is reached.  Every post-threshold transaction must roll back
       to the function-entry scene, including that frame advance. */
    previous_scene = *scene;
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
    scoring_team = scene->state.free_throws.scoring_team;
    shooter = scene->free_throw_shooter;
    made = (scene->action_serial +
            scene->state.free_throws.attempts_remaining) % 3U != 0U;
    if (!tecmo_gameplay_record_free_throw_result(
            &scene->state,
            made,
            &scene->events)) {
        *scene = previous_scene;
        return false;
    }
    if (shooter >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[shooter].team != (uint8_t)scoring_team ||
        scene->actors[shooter].roster_index >=
            TECMO_PLAYER_STATS_ROSTER_COUNT ||
        !tecmo_player_stats_record_free_throw(
            &scene->player_stats, (uint8_t)scoring_team,
            scene->actors[shooter].roster_index, made)) {
        *scene = previous_scene;
        return false;
    }
    if (scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED) {
        TecmoGameplayTeam next = scene_other_team(
            scene->state.free_throws.scoring_team);
        /* The free-throw lineup may have changed the semantic holder without
           an ordinary live-AI synchronization tick. Capture that selected
           scoring side before settlement changes rules possession so the
           source score transition can swap the actual selected pair. */
        if (!scene_sync_live_foundation(scene) ||
            !tecmo_gameplay_settle_free_throws(
                &scene->state, next,
                TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50)) {
            *scene = previous_scene;
            return false;
        }
        if (!scene_begin_scored_inbound(scene, next)) {
            *scene = previous_scene;
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
            scene->orientation_state.attack_direction, 0U, false) ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &followed)) {
        return false;
    }
    scene->camera_state = followed;
    ++scene->camera_follow_count;
    return true;
}

static bool scene_pretip_cpu_requested(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam team)
{
    if (scene == NULL ||
        (team != TECMO_GAMEPLAY_TEAM_AWAY &&
         team != TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene_launch_valid(&scene->launch) ||
        !tecmo_gameplay_pretip_state_validate(
            &scene->pretip_assets, &scene->pretip_state) ||
        scene->pretip_state.phase < TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        scene->pretip_state.phase > TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        return false;
    }
    return scene_controller_for_team(scene, team) >=
           TECMO_GAMEPLAY_CONTROLLER_COUNT;
}

static bool scene_apply_first_period_entry_seed(TecmoGameplayScene *scene)
{
    TecmoGameplayScene candidate;
    TecmoGameplayBallDribbleFrame ball_frame;
    size_t actor;
    if (scene == NULL || scene->state.period != 1U ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !scene->pretip_state.live_handoff ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene_cpu_common_tail_has_ordinary_live_zero(scene)) {
        if (scene != NULL) {
            scene_set_status(scene, "first-period seed admission rejected");
        }
        return false;
    }
    candidate = *scene;
    if (!tecmo_gameplay_live_foundation_first_period_entry_seed(
            &candidate.cpu_steering_assets, candidate.state.period, true,
            &candidate.live_foundation)) {
        scene_set_status(scene, "first-period seed foundation rejected");
        return false;
    }
    /* `$85EA` begins with `ORA #$0B / AND #$EB`: the newly established
       bit-$08 clamp exemption cannot inherit a prior selected-holder
       boundary latch. Clear only this exact primary at the atomic seed seam;
       later ordinary TGMO violations remain independently owned. */
    candidate.actors[candidate.live_foundation.primary_actor]
        .movement_boundary_latched = false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        bool source_state_changed =
            candidate.live_foundation.actor_team[actor] ==
                candidate.live_foundation.offense_side;
        bool position_changed =
            candidate.actors[actor].position.x !=
                candidate.live_foundation.actor_position[actor].x ||
            candidate.actors[actor].position.y !=
                candidate.live_foundation.actor_position[actor].y;
        if (!source_state_changed && !position_changed) {
            continue;
        }
        if (position_changed) {
            candidate.actors[actor].position =
                candidate.live_foundation.actor_position[actor];
            candidate.actors[actor].anchor = candidate.actors[actor].position;
        }
        candidate.cpu_actors[actor].decision_serial = 0U;
        candidate.cpu_actors[actor].snapshot_fingerprint = 0U;
        candidate.cpu_actors[actor].target_position.x = 0;
        candidate.cpu_actors[actor].target_position.y = 0;
        candidate.cpu_actors[actor].command_offset =
            TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
        candidate.cpu_actors[actor].linked_actor =
            candidate.live_foundation.play_state.fixed_link[actor];
        candidate.cpu_actors[actor].target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT;
        candidate.cpu_actors[actor].direction =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        candidate.cpu_actors[actor].held_direction_bits =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
        candidate.cpu_actors[actor].command_advance_pending = false;
        candidate.cpu_actors[actor].target_valid = false;
        candidate.cpu_actors[actor].writes_direction = false;
        if (!scene_actor_position_valid_for_scene(&candidate, actor) ||
            !scene_cpu_actor_state_valid(
                &candidate, actor, &candidate.cpu_actors[actor])) {
            char detail[96];
            (void)snprintf(detail, sizeof(detail),
                           "first-period seed actor %u %s validation rejected",
                           (unsigned)actor,
                           scene_actor_position_valid_for_scene(
                               &candidate, actor) ? "CPU" : "world");
            scene_set_status(scene, detail);
            return false;
        }
    }
    if (!scene_live_ball_frame_for_actors(
            &candidate, candidate.actors, candidate.ball_holder,
            &ball_frame) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &ball_frame.visible_position, &candidate.ball_position) ||
        !scene_ownership_valid(&candidate)) {
        scene_set_status(scene, "first-period seed scene validation rejected");
        return false;
    }
    *scene = candidate;
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
    bool pretip_away_held = false;
    bool pretip_home_held = false;
    bool pretip_away_automatic = false;
    bool pretip_home_automatic = false;
    uint32_t pretip_total_before = scene->pretip_state.total_frame;

    if (prior_phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD) {
        /* Card cancellation consumes the raw pad levels. */
        pretip_away_held = held_one;
        pretip_home_held = held_two;
    } else if (prior_phase >= TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP &&
               prior_phase <= TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
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
        /* A team-routed human level always wins this decision seam. A team
           with no assigned controller receives the validated automatic path;
           its threshold/commit is owned transactionally by the TPTI state. */
        if (prior_phase >= TECMO_GAMEPLAY_PRETIP_BALL_DESCENT) {
            pretip_away_automatic = !pretip_away_held &&
                scene_pretip_cpu_requested(scene, TECMO_GAMEPLAY_TEAM_AWAY);
            pretip_home_automatic = !pretip_home_held &&
                scene_pretip_cpu_requested(scene, TECMO_GAMEPLAY_TEAM_HOME);
        }
    }
    if (!tecmo_gameplay_pretip_update_controlled(
            &scene->pretip_assets, &scene->pretip_state,
            pretip_away_held, pretip_home_held,
            pretip_away_automatic, pretip_home_automatic)) {
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
    if (scene->pretip_state.simulation_active) {
        uint16_t frame = scene->pretip_state.simulation_tick;
        if (!scene_pretip_apply_jump_frame(scene, frame)) {
            scene_set_status(scene, "pre-tip jump presentation rejected");
            return false;
        }
        /* Ball slot 10 owns one coherent target-directed state-$17 path.
           Depth and altitude remain separate until this projection seam. */
        scene->ball_position.x_q8 =
            scene->pretip_state.ball_world_x_q8;
        scene->ball_position.y_q8 =
            scene->pretip_state.ball_world_depth_q8 -
            (int32_t)scene->pretip_state.ball_height_q8;
        if (scene->ball_position.y_q8 < 0)
            scene->ball_position.y_q8 = 0;
    }
    if (scene->pretip_state.total_frame != pretip_total_before)
        ++scene->frame;
    if (scene->pretip_state.live_handoff) {
        TecmoGameplayState state_before;
        TecmoGameplayCourtOrientationState orientation_before;
        TecmoGameplayCameraState camera_before;
        TecmoGameplayBackcourtState backcourt_before;
        TecmoGameplayAudioPlayer audio_before;
        TecmoGameplayLiveFoundation foundation_before;
        TecmoGameplaySceneActor actors_before[
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
        TecmoGameplaySceneCpuActor cpu_before[
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
        TecmoGameplayCourtCoordinateQ8 ball_before;
        uint8_t controlled_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
        uint16_t altitude_before[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
        uint8_t holder_before;
        bool jump_active_before;
        TecmoGameplayTeam possession;
        uint8_t claimant_jumper;
        uint8_t claimant_actor;
        uint8_t holder;
        if (!tecmo_gameplay_pretip_claimant_jumper(
                &scene->pretip_assets, &scene->pretip_state,
                &claimant_jumper) ||
            claimant_jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT ||
            !scene_pretip_jumper_mapping_valid(scene)) {
            scene_set_status(scene, "pre-tip winner handoff rejected");
            return false;
        }
        claimant_actor = scene->pretip_jumper_actor[claimant_jumper];
        if (claimant_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            (scene->actors[claimant_actor].team != TECMO_GAMEPLAY_TEAM_AWAY &&
             scene->actors[claimant_actor].team != TECMO_GAMEPLAY_TEAM_HOME)) {
            scene_set_status(scene, "pre-tip claimant actor rejected");
            return false;
        }
        possession = (TecmoGameplayTeam)scene->actors[claimant_actor].team;
        /* $A274's jumper selection and $0380/$037F receiver selection are
           separate seams. Do not treat center slot 4/9 or team slot 0/5 as
           the possession receiver. */
        holder = scene->pretip_state.receiver_actor;
        if (holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[holder].team != possession) {
            scene_set_status(scene, "pre-tip receiver actor rejected");
            return false;
        }
        /* Bank04 $AC8C is launch-only.  Bank05 $A274 changes the ball and
           selection fields, while $86BB-$879A recovers the existing jumpers
           and Bank06 $827E continues the other actor objects.  Never replay
           the cold initializer at this transition. */
        state_before = scene->state;
        orientation_before = scene->orientation_state;
        camera_before = scene->camera_state;
        backcourt_before = scene->backcourt_state;
        audio_before = scene->audio_player;
        foundation_before = scene->live_foundation;
        memcpy(actors_before, scene->actors, sizeof(actors_before));
        memcpy(cpu_before, scene->cpu_actors, sizeof(cpu_before));
        memcpy(controlled_before, scene->controlled_actor,
               sizeof(controlled_before));
        memcpy(altitude_before, scene->pretip_jumper_altitude_q8,
               sizeof(altitude_before));
        ball_before = scene->ball_position;
        holder_before = scene->ball_holder;
        jump_active_before = scene->pretip_jump_active;
        /* $A274-$A2DE changes ball/selection state only.  Jumper landing is
           owned by normal $8732/$8745 airborne recovery, never by cinematic
           completion or the live handoff. */
        {
            const char *handoff_reject = NULL;
            if (!scene_handoff_tip_possession(scene, possession, holder)) {
                handoff_reject = "pre-tip possession handoff rejected";
            } else if (!scene_sync_live_foundation(scene)) {
                handoff_reject = "pre-tip foundation sync rejected";
            } else if (!scene_apply_first_period_entry_seed(scene)) {
                handoff_reject = scene->status;
            } else if (!tecmo_gameplay_camera_settle_court(
                    &scene->camera_assets, &scene->camera_state,
                    &scene->ball_position,
                    scene->orientation_state.attack_direction, false)) {
                handoff_reject = "pre-tip seeded camera settle rejected";
            } else if (scene->launch.game_music_enabled &&
                       !tecmo_gameplay_audio_queue_game_music(
                           &scene->audio_player)) {
                handoff_reject = "pre-tip game music queue rejected";
            }
            if (handoff_reject != NULL) {
                char handoff_reject_copy[128];
                (void)snprintf(handoff_reject_copy,
                               sizeof(handoff_reject_copy), "%s",
                               handoff_reject);
            scene->state = state_before;
            scene->orientation_state = orientation_before;
            scene->camera_state = camera_before;
            scene->backcourt_state = backcourt_before;
            scene->audio_player = audio_before;
            scene->live_foundation = foundation_before;
            memcpy(scene->actors, actors_before, sizeof(actors_before));
            memcpy(scene->cpu_actors, cpu_before, sizeof(cpu_before));
            memcpy(scene->controlled_actor, controlled_before,
                   sizeof(controlled_before));
            memcpy(scene->pretip_jumper_altitude_q8, altitude_before,
                   sizeof(altitude_before));
            scene->ball_position = ball_before;
            scene->ball_holder = holder_before;
            scene->pretip_jump_active = jump_active_before;
                scene_set_status(scene, handoff_reject_copy);
            return false;
            }
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
    /* Once Bank07's restart event has entered the explicit setup, it owns
       the next visible gather/flight/catch frames. Do not feed held controls
       to the pure state machine while that transport is active: both clocks,
       AI, and normal court mutation remain frozen until the B24F-shaped
       catch commits. The prior event has already been dispatched. */
    if (scene_inbound_active(scene)) {
        if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
            scene_set_status(scene, "inbound phase/state contract rejected");
            return false;
        }
        tecmo_gameplay_events_clear(&scene->events);
        *phase_before_out = phase_before;
        *free_throw_team_captured_out = false;
        *captured_free_throw_team_out = captured_free_throw_team;
        *restart_frame_out = true;
        return true;
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

    if (scene_inbound_active(scene)) {
        if (!scene_update_inbound(scene)) {
            scene_set_status(scene, "inbound transport update rejected");
            return false;
        }
    } else if (scene_pass_active(scene)) {
        if (!scene_update_pass(scene)) {
            scene_set_status(scene, "pass animation update rejected");
            return false;
        }
    } else if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        bool terminal_jump_miss =
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
            !scene->predicted_make_route &&
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
    } else if (scene->loose_ball_state.active) {
        if (!scene_update_loose_ball(scene, controls)) {
            scene_set_status(scene, "loose-ball claimant update rejected");
            return false;
        }
    } else if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE) {
        /* Fixed $F031 calls Bank05 $81F2 before source player movement.
           $8209-$8217/$833B/$9054 therefore snapshots the selected primary
           once here; every later Bank06 opcode-16 dispatch shares it. */
        memset(&scene->opcode16_frame_context, 0,
               sizeof(scene->opcode16_frame_context));
        if (!scene->legacy_direct_launch &&
            !scene_cpu_opcode16_workspace_capture(
                scene, &scene->opcode16_frame_context)) {
            scene_set_status(scene, "opcode-16 pre-motion workspace rejected");
            return false;
        }
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
            /* Fixed-loop order: Bank06 map, $B139 offense, $B104 defense,
               then downstream pass/switch consumption. */
            if (controller == 0U &&
                !scene_update_selection_candidates(scene, controls)) {
                scene_set_status(scene, "candidate selection rejected");
                return false;
            }
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
        if (!boundary_settled && !scene_pass_active(scene) &&
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
        if (!boundary_settled && !scene_pass_active(scene) &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !scene_update_ai(scene, cpu_shot_request)) {
            scene_set_status(scene, "native offense update rejected");
            return false;
        }
        if (!boundary_settled && !scene_pass_active(scene) &&
            cpu_shot_request->requested) {
            /* scene_update_ai already attempted the excluded playback once
               on its complete candidate. Never call shots.c a second time.
               Unsupported jump/far/controller-dependent requests are an
               explicit nonfatal deferred classification. */
            if (cpu_shot_request->playback_supported) {
                if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
                    (!scene_shot_is_close(scene->shot_kind) &&
                     scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) ||
                    scene->shot_actor != cpu_shot_request->actor_index) {
                    scene_set_status(
                        scene, "CPU shot request playback classification rejected");
                    return false;
                }
            } else if (cpu_shot_request->deferred) {
                scene_set_status(
                    scene, "CPU shot request deferred/non-launch");
            } else {
                scene_set_status(
                    scene, "CPU shot request playback classification rejected");
                return false;
            }
        } else if (!boundary_settled && !scene_pass_active(scene) &&
                   cpu_shot_request->deferred) {
            scene_set_status(
                scene, "CPU shot request deferred/non-launch");
        }
        if (!boundary_settled && !scene_pass_active(scene) &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !scene_settle_boundary_latch(scene, &boundary_settled)) {
            scene_set_status(
                scene, "CPU out-of-bounds settlement rejected");
            return false;
        }
        if (!boundary_settled && !scene_pass_active(scene) &&
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

static bool scene_update_bound_frame(TecmoGameplayScene *scene,
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
    TecmoGameplaySceneCpuShotRequest cpu_shot_request;
    TecmoGameplayState previous_state;
    TecmoGameplayEventBuffer previous_events;

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene->active || scene->result_ready ||
        scene->pretip_abort_pending) {
        return false;
    }
    previous_state = scene->state;
    previous_events = scene->events;
    memset(&cpu_shot_request, 0, sizeof(cpu_shot_request));
    cpu_shot_request.actor_index = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (!tecmo_gameplay_pretip_state_validate(
            &scene->pretip_assets, &scene->pretip_state)) {
        scene_set_status(scene, "pre-tip state contract rejected");
        return false;
    }
    if (tecmo_gameplay_pretip_is_presentation(&scene->pretip_state)) {
        return scene_update_pretip_frame(scene, player_one, player_two);
    }
    if (scene->pretip_jump_active) {
        if (!tecmo_gameplay_pretip_update_live_jumpers(
                &scene->pretip_assets, &scene->pretip_state) ||
            !scene_pretip_apply_jump_frame(
                scene, scene->pretip_state.simulation_tick)) {
            scene_set_status(scene, "live tip-jumper recovery rejected");
            return false;
        }
    }
    if (!scene_ownership_valid(scene)) {
        scene_set_status(scene,
                         "gameplay ownership pre-action invariant rejected");
        return false;
    }
    controls[0] = player_one;
    controls[1] = player_two;
    tecmo_gameplay_frame_input_clear(&input);
    scene_pad_from_controls(&input.controllers[0], player_one);
    scene_pad_from_controls(&input.controllers[1], player_two);
    tecmo_gameplay_live_context_default(&live_context);
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->loose_ball_state.active) {
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
        (!restart_frame ||
         (scene_inbound_active(scene) &&
          phase_before == TECMO_GAMEPLAY_PHASE_LIVE)) &&
        !scene_update_live_action_ordered(
            scene, controls, &cpu_shot_request, &jump_miss_settled,
            &jump_miss_shooting_team)) {
        return false;
    }
    if (!scene_update_post_action_phases(
            scene, phase_before, restart_frame, player_one, player_two)) {
        if (phase_before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE) {
            scene->state = previous_state;
            scene->events = previous_events;
        }
        return false;
    }
    /* PRETIP owns transient presentation coordinates. Foundation sync is
       restricted to LIVE (or the explicit pre-tip handoff above); a shot
       playback frame with no holder preserves the last validated binding. */
    if (!scene->legacy_direct_launch &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        !scene_sync_live_foundation(scene)) {
        scene_set_status(scene, "LIVE state synchronization rejected");
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
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
        /* The retained Bank02 identity is scoped strictly to its accepted
           display phase. Foul settlement/free throws own no overlay data. */
        memset(&scene->foul_presentation, 0,
               sizeof(scene->foul_presentation));
    }
    scene->previous_phase = scene->state.phase;
    ++scene->frame;
    return true;
}

bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two)
{
    bool result = scene_update_bound_frame(scene, player_one, player_two);
    /* A runtime sample is single-frame input. Publish timer output first,
       then consume availability on success or failure so direct callers
       cannot silently reuse stale `$6A`. */
    if (scene != NULL &&
        scene->lifecycle_tag == TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        scene->opcode10_frame_context.available = false;
        scene->opcode16_frame_context.available = false;
        scene->a023_latch_frame_context.available = false;
    }
    return result;
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

bool tecmo_gameplay_scene_possession_trace_snapshot(
    const TecmoGameplayScene *scene,
    TecmoGameplayScenePossessionTraceSnapshot *snapshot_out)
{
    const TecmoGameplayLiveFoundation *live;
    TecmoGameplayScenePossessionTraceSnapshot candidate;
    if (scene == NULL || snapshot_out == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        return false;
    }
    live = &scene->live_foundation;
    memset(&candidate, 0, sizeof(candidate));
    candidate.contract_tag = TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG;
    candidate.sync_serial = live->sync_serial;
    candidate.raw_0308_primary_actor = live->primary_actor;
    candidate.raw_0309_defender_actor = live->defender_actor;
    candidate.raw_030a_offense_side = live->offense_side;
    candidate.raw_030b_defense_side = live->defense_side;
    memcpy(candidate.raw_030c_030d_control_mode, live->control_mode,
           sizeof(candidate.raw_030c_030d_control_mode));
    memcpy(candidate.raw_000e_000f_selected_actor,
           live->selected_actor_by_side,
           sizeof(candidate.raw_000e_000f_selected_actor));
    memcpy(candidate.raw_037f_0380_candidate_actor,
           live->candidate_actor_by_side,
           sizeof(candidate.raw_037f_0380_candidate_actor));
    memcpy(candidate.raw_04b0_selector_flags,
           live->actor_selector_flags,
           sizeof(candidate.raw_04b0_selector_flags));
    memcpy(candidate.raw_06cb_dynamic_link, live->dynamic_link,
           sizeof(candidate.raw_06cb_dynamic_link));
    memcpy(candidate.raw_0547_0551_stream_offset,
           live->play_state.stream_offset,
           sizeof(candidate.raw_0547_0551_stream_offset));
    memcpy(candidate.raw_057c_actor_state,
           live->play_state.actor_state,
           sizeof(candidate.raw_057c_actor_state));
    candidate.opcode15_trace = live->opcode15_trace;
    candidate.semantic_scene_possession = (uint8_t)scene->state.possession;
    candidate.semantic_ball_holder = scene->ball_holder;
    candidate.semantic_live_last_possession = live->last_possession;
    candidate.semantic_live_last_ball_holder = live->last_ball_holder;
    candidate.semantic_live_synchronized =
        !live->first_sync_pending && live->sync_serial != 0U &&
        live->primary_actor == scene->ball_holder &&
        live->last_possession == (uint8_t)scene->state.possession;
    *snapshot_out = candidate;
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
    scene_loose_ball_clear(scene);
    scene->close_shot_step = 0U;
    scene->free_throw_frame = 0U;
    memset(&scene->foul_presentation, 0,
           sizeof(scene->foul_presentation));
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
