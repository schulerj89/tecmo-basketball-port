#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG 0x53434E31U
#define TECMO_GAMEPLAY_TEAM_LIMIT 27U
#define TECMO_GAMEPLAY_BALL_POSE 64U
#define TECMO_GAMEPLAY_SHOT_TARGET_Y 0x008F
#define TECMO_GAMEPLAY_INITIAL_CAMERA_X 0x0100
#define TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE 0x00DF
#define TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE 0x0220
#define TECMO_GAMEPLAY_MIN_Y TECMO_GAMEPLAY_COURT_WORLD_MIN_Y
#define TECMO_GAMEPLAY_MAX_Y TECMO_GAMEPLAY_COURT_WORLD_MAX_Y
#define TECMO_GAMEPLAY_CLOSE_DISTANCE_X 48
#define TECMO_GAMEPLAY_DRIBBLE_CADENCE 24U
#define TECMO_GAMEPLAY_JUMP_SLOT0_DURATION 87U
#define TECMO_GAMEPLAY_JUMP_RATTLE_DURATION 103U
#define TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME 73U
#define TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME 89U
#define TECMO_GAMEPLAY_JUMP_RATTLE_FRAME_SHIFT 16U
/* The visible side-0 route proves a negative incoming sign, not its exact
   horizontal magnitude. Only the sign affects state-$15 setup. */
#define TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6 (-1)
/* Bank05 $AD4E launches at the side target selected by $BDEF-$BDF2,
   with the shared target Y loaded as $8F. */
#define TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y 0x008F
#define TECMO_GAMEPLAY_JUMP_MAKE_DURATION 111U
#define TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME 9U
#define TECMO_GAMEPLAY_JUMP_MAKE_DECISION_FRAME 19U
#define TECMO_GAMEPLAY_JUMP_MAKE_FLIGHT_FRAME 20U
#define TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME 57U
#define TECMO_GAMEPLAY_JUMP_MAKE_NEUTRAL_FRAME 63U
#define TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME 85U
#define TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8 0x02E8U
#define TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8 0x02E8U
#define TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8 0x0308U
#define TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE 469U
#define TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE 325U
#define TECMO_GAMEPLAY_JUMP_MAKE_TURN_POSE 1060U
#define TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_POSE 1061U
#define TECMO_GAMEPLAY_JUMP_MAKE_FLIGHT_POSE 213U
#define TECMO_GAMEPLAY_SCENE_RENDER_FNV1A32 0xD79DCDADU
#define TECMO_GAMEPLAY_SCENE_CENTER_SLICE_FNV1A32 0x6E530421U
#define TECMO_GAMEPLAY_SCENE_LEFT_SLICE_FNV1A32 0x770FAE95U
#define TECMO_GAMEPLAY_SCENE_RIGHT_SLICE_FNV1A32 0x2DBDF155U
#define TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X 0x0066U
#define TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X 0x0198U
#define TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y 71
#define TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y 145
#define TECMO_GAMEPLAY_PRETIP_DESCENT_MOVE_FRAMES 60U
#define TECMO_GAMEPLAY_HUD_PRIMARY_ROW 2U
#define TECMO_GAMEPLAY_HUD_SECONDARY_ROW 3U
#define TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT 2U
#define TECMO_GAMEPLAY_HUD_COLUMN_COUNT 32U
#define TECMO_GAMEPLAY_HUD_AWAY_SCORE_COLUMN 6U
#define TECMO_GAMEPLAY_HUD_CLOCK_COLUMN 13U
#define TECMO_GAMEPLAY_HUD_HOME_SCORE_COLUMN 28U
#define TECMO_GAMEPLAY_HUD_AWAY_SHOT_COLUMN 1U
#define TECMO_GAMEPLAY_HUD_AWAY_PLAYER_COLUMN 4U
#define TECMO_GAMEPLAY_HUD_HOME_SHOT_COLUMN 17U
#define TECMO_GAMEPLAY_HUD_HOME_PLAYER_COLUMN 20U
#define TECMO_GAMEPLAY_HUD_SCORE_WIDTH 3U
#define TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH 2U
#define TECMO_GAMEPLAY_HUD_CLOCK_SECOND_WIDTH 2U
#define TECMO_GAMEPLAY_HUD_SHOT_WIDTH 2U
#define TECMO_GAMEPLAY_HUD_PLAYER_WIDTH 11U
#define TECMO_GAMEPLAY_HUD_SURNAME_WIDTH 9U
#define TECMO_GAMEPLAY_HUD_COLON_TILE 0x16U

typedef struct TecmoGameplayPreparedHud {
    bool occupied[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                 [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    bool chr_resolved[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                     [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    uint8_t tiles[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                 [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    uint32_t chr_offsets[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                        [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
} TecmoGameplayPreparedHud;

static void scene_fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                            int width, int height, uint32_t color);

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
    tecmo_gameplay_cpu_steering_assets_init(
        &scene->cpu_steering_assets);
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
    tecmo_gameplay_cpu_steering_assets_init(
        &scene->cpu_steering_assets);
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
    if (!tecmo_gameplay_cpu_steering_assets_load(
            &scene->cpu_steering_assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->cpu_steering_assets.status);
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
                     "native gameplay ready: TPTI-1/TGPL-1/TTDT-1/TWAR-1/TMUS-1/TGCT-1/TGCP-2/TGMO-1/TGAI-1/TGOR-1/TGFL-1/THUD-1/TGCS-1/TGDK-1/TGJS-2/TGSR-3/TSFX-1/TDMC-1");
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

static void scene_clear_jump_playback(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    scene->jump_actor_altitude_q8 = 0U;
    scene->jump_actor_velocity_q8 = 0U;
    scene->jump_ball_altitude_q8 = 0U;
    scene->jump_ball_bounce_q8 = 0U;
    scene->jump_actor_state = 0U;
    scene->jump_ball_state = 0U;
    scene->jump_phase_counter = 0U;
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

static bool scene_controller_team_valid(uint8_t team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY ||
           team == TECMO_GAMEPLAY_TEAM_HOME ||
           team == TECMO_GAMEPLAY_SCENE_NO_TEAM;
}

static bool scene_launch_valid(const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayConfig config;
    if (launch == NULL || !scene_source_valid(launch->source) ||
        launch->away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->home_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        launch->away_team == launch->home_team || launch->difficulty > 2U ||
        launch->control_mode > 6U || launch->speed_value > 2U ||
        !scene_controller_team_valid(launch->controller_team[0]) ||
        !scene_controller_team_valid(launch->controller_team[1]) ||
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

static bool scene_movement_pose_index(
    const TecmoGameplayScene *scene,
    const TecmoGameplayMovementState *movement,
    uint16_t *pose_index_out)
{
    if (scene == NULL || movement == NULL || pose_index_out == NULL) {
        return false;
    }
    /* The base-pointer and animation-phase calculation is exact TGMO-1.
       The live scene uses the primary table half until the opponent-relative
       $8F02 choice has enough native state to be wired without guessing. */
    return tecmo_gameplay_movement_pose_index(
        &scene->movement_assets, movement, false, pose_index_out);
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
    size_t actor;
    if (scene == NULL || !scene->movement_assets.available ||
        !scene->cpu_steering_assets.available ||
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
            !scene_movement_pose_index(scene, &movement, &pose_index)) {
            return false;
        }
        item->pose_index = pose_index;
        item->movement_action_state = movement.action_state;
        item->movement_direction = movement.direction;
        item->movement_fractional_accumulator =
            movement.fractional_accumulator;
        item->movement_animation_phase = movement.animation_phase;
        item->condition = player->condition_seed;
        item->movement_boundary_latched =
            movement.boundary_violation_latched;
        cpu->contract_tag = TECMO_GAMEPLAY_SCENE_CPU_ACTOR_TAG;
        cpu->command_offset =
            TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
        cpu->linked_actor = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
            ? (uint8_t)(actor + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT)
            : (uint8_t)(actor - TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
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
    ball_position.x_q8 =
        (int32_t)(initialized[0].position.x + 7) * 256;
    ball_position.y_q8 =
        (int32_t)(initialized[0].position.y - 18) * 256;
    memcpy(scene->actors, initialized, sizeof(initialized));
    memcpy(scene->cpu_actors, initialized_cpu, sizeof(initialized_cpu));
    memcpy(scene->controlled_actor, controlled, sizeof(controlled));
    scene->ball_holder = 0U;
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
            &scene->camera_assets, &initial_camera)) {
        scene_set_status(scene, "gameplay state initialization rejected");
        return false;
    }
    scene->state = initial_state;
    scene->orientation_state = initial_orientation;
    scene->camera_state = initial_camera;
    if (!scene_initialize_actors(scene)) {
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
    scene_clear_jump_playback(scene);
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

static TecmoGameplayTeam scene_other_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? TECMO_GAMEPLAY_TEAM_HOME
               : TECMO_GAMEPLAY_TEAM_AWAY;
}

static bool scene_queue_result_audio(TecmoGameplayScene *scene,
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

static bool scene_actor_coordinate_valid(
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

static bool scene_actor_world_position_valid(
    const TecmoGameplaySceneActor *actor)
{
    return actor != NULL &&
           scene_actor_coordinate_valid(&actor->position);
}

static void scene_clamp_actor_world(TecmoGameplaySceneActor *actor)
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

static bool scene_actor_movement_state(
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

static bool scene_actor_apply_movement(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneActor *actor,
    const TecmoGameplayMovementState *movement,
    uint8_t held_direction_bits)
{
    uint16_t pose_index;
    if (scene == NULL || actor == NULL || movement == NULL ||
        !tecmo_gameplay_movement_input_valid(held_direction_bits) ||
        !tecmo_gameplay_movement_state_valid(
            &scene->movement_assets, movement) ||
        !scene_movement_pose_index(scene, movement, &pose_index)) {
        return false;
    }
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

static bool scene_move_controlled_actor(TecmoGameplayScene *scene,
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
    input.primary_selected_actor = true;
    if (!tecmo_gameplay_movement_step(
            &scene->movement_assets, &movement, &input) ||
        !scene_actor_apply_movement(
            scene, actor, &movement, direction_bits)) {
        return false;
    }
    return true;
}

static uint8_t scene_first_actor_for_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? 0U
               : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
}

static bool scene_attached_ball_position(
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

static bool scene_attach_ball(TecmoGameplayScene *scene)
{
    TecmoGameplayCourtCoordinateQ8 attached;
    if (scene == NULL ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene_attached_ball_position(
            &scene->actors[scene->ball_holder], &attached)) {
        return false;
    }
    scene->ball_position = attached;
    return true;
}

static uint32_t scene_distance_squared(const TecmoGameplaySceneActor *a,
                                       const TecmoGameplaySceneActor *b)
{
    int32_t dx = (int32_t)a->position.x - b->position.x;
    int32_t dy = (int32_t)a->position.y - b->position.y;
    return (uint32_t)(dx * dx + dy * dy);
}

static uint8_t scene_next_teammate(const TecmoGameplayScene *scene,
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

static uint8_t scene_nearest_actor_for_team(const TecmoGameplayScene *scene,
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

static bool scene_pass_or_switch(TecmoGameplayScene *scene,
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

static bool scene_shot_will_score(const TecmoGameplayScene *scene);

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

static bool scene_shot_is_close(TecmoGameplaySceneShotKind kind)
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

static bool scene_close_pose_for_step(const TecmoGameplayScene *scene,
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

static bool scene_start_shot_actor(TecmoGameplayScene *scene,
                                   size_t controller,
                                   uint8_t actor_index)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayCourtCoordinate shot_start;
    TecmoGameplayCourtCoordinate shot_end;
    TecmoGameplayCourtCoordinateQ8 shot_start_q8;
    TecmoGameplayCourtCoordinateQ8 shot_end_q8;
    uint16_t target_x;
    int approach_distance_x;
    int distance_y;
    uint8_t classified_points;
    bool close;
    TecmoGameplayCloseShotVariantInfo close_info;
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
    target_x = (uint16_t)scene->orientation_state.offensive_hoop.x;
    shot_start.x = (int16_t)(
        actor->position.x + (actor->facing_right ? 7 : -7));
    shot_start.y = (int16_t)(actor->position.y - 18);
    shot_end.x = scene->orientation_state.offensive_hoop.x;
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
        scene_clear_jump_playback(scene);
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
        /* Exact ordinary-jump playback is restricted to the one proven human,
           away-side, rightward family/profile/direction context and its two
           bounded terminal outcomes. Unknown directions and other outcomes
           remain unsupported instead of inheriting a synthetic schedule. */
        if (scene->launch.controller_team[controller] != actor->team ||
            actor->team != TECMO_GAMEPLAY_TEAM_AWAY ||
            !actor->facing_right) {
            return false;
        }
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
        memset(&close_info, 0, sizeof(close_info));
        scene_clear_jump_playback(scene);
        scene->shot_controller = (uint8_t)controller;
        scene->jump_family = TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0;
        scene->jump_profile = TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0;
        scene->jump_direction = TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1;
        if (!scene_jump_pose_for_context(scene, &initial_pose)) {
            scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
            scene_clear_jump_playback(scene);
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
        scene_clear_jump_playback(scene);
        return false;
    }
    scene->shot_duration = scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                               ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                               : (close ? close_info.step_count
                                        : (predicted_make
                                               ? (uint16_t)(
                                                     TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
                                                     scene->jump_shots.constants.made_update_count)
                                               : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION));
    if (predicted_make) initial_pose = TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE;
    actor->pose_index = initial_pose;
    if (!close) {
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

static bool scene_start_shot(TecmoGameplayScene *scene,
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

static bool scene_handoff_possession(TecmoGameplayScene *scene,
                                     TecmoGameplayTeam possession,
                                     uint8_t preferred_actor)
{
    TecmoGameplayState state_before;
    TecmoGameplayCourtOrientationState orientation_before;
    uint8_t first = scene_first_actor_for_team(possession);
    uint8_t holder = preferred_actor;
    size_t controller;
    if (scene == NULL ||
        (possession != TECMO_GAMEPLAY_TEAM_AWAY &&
         possession != TECMO_GAMEPLAY_TEAM_HOME)) {
        return false;
    }
    if (holder < first ||
        holder >= first + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        holder = first;
    }
    state_before = scene->state;
    orientation_before = scene->orientation_state;
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
    if (scene->orientation_state.transition_serial !=
        orientation_before.transition_serial) {
        /* Preserve camera position/stream ownership across possession. The
           next single live follow recomputes direction-specific thresholds
           and may establish the opposite endpoint latch. */
        scene->camera_state.thresholds_valid = false;
        scene->camera_state.endpoint_latched = false;
    }
    scene->ball_holder = holder;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == possession) {
            scene->controlled_actor[controller] = holder;
        }
    }
    return scene_attach_ball(scene);
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
    if (!scene_actor_movement_state(scene, actor, &movement) ||
        !scene_movement_pose_index(scene, &movement, &idle_pose)) {
        return false;
    }
    if (made) {
        if (!tecmo_gameplay_award_points(&scene->state, shooting_team,
                                         scene->shot_points)) {
            return false;
        }
        if (queue_side_result) {
            if (!scene_queue_result_audio(scene, shooting_team)) return false;
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
    scene_clear_jump_playback(scene);
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
    scene_clear_jump_playback(scene);

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
    scene_clear_jump_playback(scene);
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
    scene->jump_phase_counter =
        scene->jump_shots.constants.phase_seed_gather;
    actor->pose_index = TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_POSE;
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
            TECMO_GAMEPLAY_TEAM_AWAY) {
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
            if (scene->shot_frame == 5U) {
                actor->pose_index = TECMO_GAMEPLAY_JUMP_MAKE_TURN_POSE;
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
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_prepared;
        scene->jump_phase_counter = release_phases[next_frame - 10U];
        actor->pose_index = TECMO_GAMEPLAY_JUMP_MAKE_FLIGHT_POSE;
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

static bool scene_update_jump_miss(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayShotOutcome outcome;
    uint16_t next_frame;
    uint16_t route_frame;
    bool landed = false;
    bool rattle_position_owned = false;
    bool repeat_dmc = false;
    bool rattle_completed = false;
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
            TECMO_GAMEPLAY_TEAM_AWAY ||
        (!scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) ||
        (scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS)) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    if (!scene->jump_b_released) {
        /* Bank05 tests the current NES B level. No previous/released edge and
           no DMC request participates in this transition. */
        if (shooting_controls != NULL && shooting_controls->held.cancel) {
            return scene->shot_frame == 1U &&
                   scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_held &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_route1;
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
    scene->shot_frame = next_frame;
    route_frame = next_frame;

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
    }

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
        route_frame = 0U;
        rattle_position_owned = true;
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
        rattle_position_owned = true;
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
            route_frame = TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME;
        }
    } else if (scene->jump_rim_rattle_debug &&
               next_frame > TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        route_frame = (uint16_t)(
            next_frame - TECMO_GAMEPLAY_JUMP_RATTLE_FRAME_SHIFT);
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

static bool scene_update_jump_shot(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    if (scene == NULL) return false;
    return scene->jump_make_route
               ? scene_update_jump_make(scene, shooting_controls)
               : scene_update_jump_miss(scene, shooting_controls);
}

static bool scene_update_shot(TecmoGameplayScene *scene,
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

static bool scene_try_defense_action(TecmoGameplayScene *scene,
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

static size_t scene_controller_for_team(const TecmoGameplayScene *scene,
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

static bool scene_free_throw_lineup_matches(
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
        if (!scene_controller_team_valid(team)) {
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
    if (scene_free_throw_lineup_matches(scene)) return true;

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

static bool scene_cpu_actor_state_valid(
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
               TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR &&
           result->steering.target_actor == cpu->linked_actor;
}

static bool scene_update_ai(TecmoGameplayScene *scene)
{
    TecmoGameplayCpuSteeringHarnessInput steering_snapshot;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        candidate_cpu[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    size_t actor;
    if (scene == NULL || !scene->cpu_steering_assets.available ||
        !scene->movement_assets.available ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->launch.difficulty >=
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT) {
        return false;
    }
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
        if (!scene_actor_movement_state(
                scene, &scene->actors[actor], &input.movement)) {
            return false;
        }
        input.player_movement_rating = player->profile[0];
        input.condition = scene->actors[actor].condition;
        input.speed_value = scene->launch.speed_value;
        input.global_object_state = 0U;
        input.movement_flags = 0U;
        if (!tecmo_gameplay_cpu_steering_movement_step(
                &scene->cpu_steering_assets, &scene->movement_assets,
                &input, &result) ||
            !scene_cpu_result_coherent(scene, actor, &result) ||
            !scene_actor_apply_movement(
                scene, &candidate_actors[actor], &result.movement,
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

    if (!scene_attached_ball_position(
            &candidate_actors[scene->ball_holder], &candidate_ball)) {
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
        if (cpu->target_valid &&
            cpu->target_kind ==
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
            target_dx <= 2 && target_dy <= 2 &&
            scene->frame % shot_cadence == 0U) {
            /* CPU close shots remain available. An unsupported ordinary-jump
               context is simply not launched; it must not fall back to the
               former synthetic schedule. */
            (void)scene_start_shot_actor(scene, 0U,
                                         scene->ball_holder);
        }
    }
    return true;
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
                !scene_queue_result_audio(scene, free_throw_team)) {
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

static bool scene_ownership_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplayMovementState movement;
    size_t actor;
    size_t controller;
    if (scene == NULL ||
        !scene->camera_assets.available ||
        !scene->movement_assets.available ||
        !scene->cpu_steering_assets.available ||
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
             ? !scene_free_throw_lineup_matches(scene)
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
                  scene, &scene->actors[actor], &movement)))) {
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

bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two)
{
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext live_context;
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayPhase phase_before;
    TecmoGameplayTeam captured_free_throw_team = TECMO_GAMEPLAY_TEAM_AWAY;
    uint8_t moving_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    int16_t moving_holder_world_x = 0;
    int16_t moving_holder_world_y = 0;
    bool restart_applied;
    bool restart_frame;
    bool free_throw_team_captured;
    bool jump_miss_settled = false;
    TecmoGameplayTeam jump_miss_shooting_team = TECMO_GAMEPLAY_TEAM_AWAY;
    size_t controller;

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
            scene->ball_position.y_q8 =
                (int32_t)(72U + frame) * 256;
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
            if (!tecmo_gameplay_scene_court_coordinates(
                    scene, &coordinates)) {
                scene_set_status(scene, "pre-tip court coordinates rejected");
                return false;
            }
        }
        return true;
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

    phase_before = scene->state.phase;
    free_throw_team_captured =
        phase_before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE;
    if (free_throw_team_captured) {
        captured_free_throw_team = scene->state.free_throws.scoring_team;
    }
    if (!tecmo_gameplay_update(&scene->state, &input, &live_context,
                               &scene->events)) {
        scene_set_status(scene, "gameplay state update rejected");
        return false;
    }
    if (!scene_apply_restart_events(scene, &restart_applied)) {
        scene_set_status(scene, "gameplay restart event rejected");
        return false;
    }
    restart_frame = restart_applied;
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
        restart_frame = true;
    }
    if (scene_phase_allows_live_action(scene->state.phase) && !restart_frame) {
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
                jump_miss_shooting_team =
                    (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
            }
            if (!scene_update_shot(scene, shooting_controls)) {
                scene_set_status(scene, "shot animation update rejected");
                return false;
            }
            jump_miss_settled = terminal_jump_miss &&
                scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE;
        } else if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE) {
            moving_holder = scene->ball_holder;
            if (moving_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
                moving_holder_world_x =
                    scene->actors[moving_holder].position.x;
                moving_holder_world_y =
                    scene->actors[moving_holder].position.y;
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
            for (controller = 0U;
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
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
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
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
            if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
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
            if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
                !scene_update_ai(scene)) {
                scene_set_status(scene, "native offense update rejected");
                return false;
            }
            if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
                if (!scene_attach_ball(scene)) {
                    scene_set_status(
                        scene, "held ball coordinate rejected");
                    return false;
                }
                if (scene->ball_holder == moving_holder &&
                    moving_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
                    (scene->actors[moving_holder].position.x !=
                         moving_holder_world_x ||
                     scene->actors[moving_holder].position.y !=
                         moving_holder_world_y) &&
                    scene->frame % TECMO_GAMEPLAY_DRIBBLE_CADENCE == 0U) {
                    (void)tecmo_gameplay_audio_queue_event(
                        &scene->audio_player,
                        TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE);
                }
            }
        }
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
        !scene_queue_result_audio(scene, jump_miss_shooting_team)) {
        scene_set_status(scene, "jump-miss result audio rejected");
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
             ? !scene_free_throw_lineup_matches(scene)
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

bool tecmo_gameplay_scene_free_throw_lineup(
    const TecmoGameplayScene *scene,
    TecmoGameplayFreeThrowLineup *lineup_out)
{
    TecmoGameplayFreeThrowLineup lineup;
    if (lineup_out == NULL ||
        !scene_free_throw_lineup_matches(scene) ||
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

bool tecmo_gameplay_scene_in_pretip(const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->lifecycle_tag == TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG &&
           scene->active &&
           tecmo_gameplay_pretip_is_presentation(&scene->pretip_state);
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
    scene_clear_jump_playback(scene);
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    scene_set_status(scene, scene->available
                                ? "native gameplay ready"
                                : "gameplay assets unavailable");
}

static bool scene_framebuffer_valid(const TecmoFramebuffer *framebuffer,
                                    int origin_x, int origin_y, int scale)
{
    size_t pitch;
    size_t height;
    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width <= 0 || framebuffer->height <= 0 ||
        framebuffer->pitch_pixels < framebuffer->width || scale <= 0 ||
        scale > 8 || origin_x < 0 || origin_y < 0 ||
        origin_x > framebuffer->width -
                       TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale ||
        origin_y > framebuffer->height -
                       TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale) {
        return false;
    }
    pitch = (size_t)framebuffer->pitch_pixels;
    height = (size_t)framebuffer->height;
    return height == 0U || pitch <= SIZE_MAX / height;
}

static bool scene_build_background_context(
    const TecmoGameplayScene *scene,
    TecmoGameplayLiveBackgroundContext *context)
{
    bool pretip;
    if (scene->launch.home_team >= TECMO_GAMEPLAY_TEAM_LIMIT) return false;
    /*
     * The center-tip setup uses the neutral final R1 page. Team-specific
     * selectors are not installed until the live handoff.
     */
    pretip = tecmo_gameplay_scene_in_pretip(scene);
    if (!tecmo_gameplay_assets_build_live_background_context(
            &scene->assets,
            pretip ? 0x40U
                   : (uint8_t)(0x40U + scene->launch.home_team),
            context)) {
        return false;
    }
    if (pretip)
        context->pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT - 1U] = 0x3FU;
    return true;
}

static bool scene_background_tile_chr(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    unsigned row,
    uint8_t tile_id,
    uint32_t *chr_offset)
{
    uint8_t band;
    uint8_t pre_asl;
    uint8_t mmc3_bank;
    uint32_t offset;
    if (scene == NULL || context == NULL || chr_offset == NULL ||
        row >= TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES) {
        return false;
    }
    band = tecmo_gameplay_assets_live_band_for_scanline(
        (uint8_t)(row * 8U));
    pre_asl = tile_id < 0x80U ? context->pre_asl_r0[band]
                              : context->pre_asl_r1[band];
    mmc3_bank = (uint8_t)(pre_asl << 1U);
    offset = (uint32_t)mmc3_bank * 1024U +
             (uint32_t)(tile_id & 0x7FU) * 16U;
    if (offset > scene->assets.chr_storage_size ||
        scene->assets.chr_storage_size - offset < 16U) {
        return false;
    }
    *chr_offset = offset;
    return true;
}

static bool scene_hud_put_tile(TecmoGameplayPreparedHud *prepared,
                               unsigned row, unsigned column,
                               uint8_t tile)
{
    if (prepared == NULL ||
        row >= TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT ||
        column >= TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        prepared->occupied[row][column]) {
        return false;
    }
    prepared->occupied[row][column] = true;
    prepared->tiles[row][column] = tile;
    return true;
}

static bool scene_hud_put_font_character(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned row, unsigned column, unsigned char character)
{
    const TecmoStartGameMenuCell *font;
    size_t font_index;
    uint8_t tile;
    if (scene == NULL || prepared == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        character < TECMO_GAMEPLAY_HUD_FONT_FIRST ||
        character >= TECMO_GAMEPLAY_HUD_FONT_FIRST +
                         TECMO_GAMEPLAY_HUD_FONT_COUNT) {
        return false;
    }
    font_index = character - TECMO_GAMEPLAY_HUD_FONT_FIRST;
    tile = scene->hud_assets.font_tiles[font_index];
    font = &scene->pretip_team_data->font[font_index];
    if (font->tile_id != tile ||
        font->chr_offset > scene->assets.chr_storage_size ||
        scene->assets.chr_storage_size - font->chr_offset < 16U ||
        !scene_hud_put_tile(prepared, row, column, tile)) {
        return false;
    }
    prepared->chr_offsets[row][column] = font->chr_offset;
    prepared->chr_resolved[row][column] = true;
    return true;
}

static bool scene_hud_put_decimal(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned row, unsigned column, unsigned width, unsigned value)
{
    unsigned digit_index;
    if (scene == NULL || prepared == NULL || width == 0U ||
        column > TECMO_GAMEPLAY_HUD_COLUMN_COUNT ||
        width > TECMO_GAMEPLAY_HUD_COLUMN_COUNT - column) {
        return false;
    }
    for (digit_index = 0U; digit_index < width; ++digit_index) {
        unsigned destination = column + width - digit_index - 1U;
        unsigned char character =
            (unsigned char)('0' + (value % 10U));
        if (!scene_hud_put_font_character(
                scene, prepared, row, destination, character)) {
            return false;
        }
        value /= 10U;
    }
    return value == 0U;
}

static bool scene_hud_actor_valid_for_team(
    const TecmoGameplayScene *scene, uint8_t actor_index,
    TecmoGameplayTeam team)
{
    return scene != NULL && team < TECMO_GAMEPLAY_TEAM_COUNT &&
           actor_index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
           scene->actors[actor_index].active &&
           scene->actors[actor_index].team == (uint8_t)team &&
           scene->actors[actor_index].roster_index <
               TECMO_TEAM_DATA_PLAYERS_PER_TEAM;
}

static bool scene_hud_selected_actor(const TecmoGameplayScene *scene,
                                     TecmoGameplayTeam team,
                                     uint8_t *actor_out)
{
    uint8_t reference = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t candidate;
    size_t controller;
    if (scene == NULL || actor_out == NULL ||
        team >= TECMO_GAMEPLAY_TEAM_COUNT) {
        return false;
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] == (uint8_t)team) {
            candidate = scene->controlled_actor[controller];
            if (!scene_hud_actor_valid_for_team(scene, candidate, team)) {
                return false;
            }
            *actor_out = candidate;
            return true;
        }
    }

    if (scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        scene->actors[scene->ball_holder].active) {
        reference = scene->ball_holder;
    } else if (scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
               scene->actors[scene->shot_actor].active) {
        reference = scene->shot_actor;
    }
    if (reference < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        candidate = (uint8_t)(
            (uint8_t)team * TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT +
            reference % TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
    } else {
        candidate = (uint8_t)(
            (uint8_t)team * TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
    }
    if (!scene_hud_actor_valid_for_team(scene, candidate, team)) {
        return false;
    }
    *actor_out = candidate;
    return true;
}

static bool scene_hud_put_player_name(
    const TecmoGameplayScene *scene,
    TecmoGameplayPreparedHud *prepared,
    unsigned column, const char name[21])
{
    size_t length = 0U;
    size_t separator = SIZE_MAX;
    size_t index;
    if (scene == NULL || prepared == NULL || name == NULL ||
        scene->hud_assets.font_tiles == NULL) {
        return false;
    }
    while (length < 21U && name[length] != '\0') ++length;
    if (length == 0U || length == 21U) return false;
    for (index = 0U; index < length; ++index) {
        if (name[index] == ' ') {
            separator = index;
            break;
        }
    }
    if (separator == SIZE_MAX || separator + 1U >= length ||
        !scene_hud_put_font_character(
            scene, prepared, 1U, column, (unsigned char)name[0]) ||
        !scene_hud_put_font_character(
            scene, prepared, 1U, column + 1U, '.')) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SURNAME_WIDTH; ++index) {
        size_t source = separator + 1U + index;
        unsigned char character;
        if (source < length) {
            character = (unsigned char)name[source];
            if (character < TECMO_GAMEPLAY_HUD_FONT_FIRST ||
                character >= TECMO_GAMEPLAY_HUD_FONT_FIRST +
                                 TECMO_GAMEPLAY_HUD_FONT_COUNT) {
                return false;
            }
        } else {
            character = ' ';
        }
        /* Bank02 writes every in-range table value verbatim. The shared
           TTDT font binding preserves even unused zero-valued punctuation. */
        if (!scene_hud_put_font_character(
                scene, prepared, 1U,
                column + 2U + (unsigned)index, character)) {
            return false;
        }
    }
    return true;
}

static bool scene_prepare_live_hud(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayPreparedHud *prepared_out)
{
    TecmoGameplayPreparedHud prepared;
    uint8_t selected[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t team_ids[TECMO_GAMEPLAY_TEAM_COUNT];
    size_t team;
    unsigned row;
    unsigned column;
    if (scene == NULL || context == NULL || prepared_out == NULL ||
        !scene->hud_assets.available ||
        scene->hud_assets.team_label_tiles == NULL ||
        scene->hud_assets.font_tiles == NULL ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available ||
        scene->launch.away_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        scene->launch.home_team >= TECMO_GAMEPLAY_TEAM_LIMIT ||
        !tecmo_gameplay_state_valid(&scene->state) ||
        !scene_ownership_valid(scene)) {
        return false;
    }
    team_ids[TECMO_GAMEPLAY_TEAM_AWAY] = scene->launch.away_team;
    team_ids[TECMO_GAMEPLAY_TEAM_HOME] = scene->launch.home_team;
    memset(&prepared, 0, sizeof(prepared));

    for (team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT; ++team) {
        uint8_t ppu_low = scene->hud_assets.team_ppu_low[team];
        unsigned absolute_row = ppu_low >> 5U;
        unsigned team_column = ppu_low & 0x1FU;
        size_t tile_index;
        if (absolute_row != TECMO_GAMEPLAY_HUD_PRIMARY_ROW ||
            team_column > TECMO_GAMEPLAY_HUD_COLUMN_COUNT -
                              TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH ||
            !scene_hud_selected_actor(
                scene, (TecmoGameplayTeam)team, &selected[team])) {
            return false;
        }
        for (tile_index = 0U;
             tile_index < TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH;
             ++tile_index) {
            if (!scene_hud_put_tile(
                    &prepared, 0U,
                    team_column + (unsigned)tile_index,
                    scene->hud_assets.team_label_tiles[team_ids[team]]
                                                       [tile_index])) {
                return false;
            }
        }
    }
    if (!scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_AWAY_SCORE_COLUMN,
            TECMO_GAMEPLAY_HUD_SCORE_WIDTH,
            scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] > 999U
                ? 999U
                : scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY]) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN,
            TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH,
            scene->state.clock_minutes > 99U
                ? 99U : scene->state.clock_minutes) ||
        !scene_hud_put_tile(
            &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN +
                TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH,
            TECMO_GAMEPLAY_HUD_COLON_TILE) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_CLOCK_COLUMN +
                TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH + 1U,
            TECMO_GAMEPLAY_HUD_CLOCK_SECOND_WIDTH,
            scene->state.clock_seconds) ||
        !scene_hud_put_decimal(
            scene, &prepared, 0U,
            TECMO_GAMEPLAY_HUD_HOME_SCORE_COLUMN,
            TECMO_GAMEPLAY_HUD_SCORE_WIDTH,
            scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] > 999U
                ? 999U
                : scene->state.score[TECMO_GAMEPLAY_TEAM_HOME]) ||
        !scene_hud_put_decimal(
            scene, &prepared, 1U,
            TECMO_GAMEPLAY_HUD_AWAY_SHOT_COLUMN,
            TECMO_GAMEPLAY_HUD_SHOT_WIDTH,
            scene->state.shot_clock) ||
        !scene_hud_put_decimal(
            scene, &prepared, 1U,
            TECMO_GAMEPLAY_HUD_HOME_SHOT_COLUMN,
            TECMO_GAMEPLAY_HUD_SHOT_WIDTH,
            scene->state.shot_clock) ||
        !scene_hud_put_player_name(
            scene, &prepared,
            TECMO_GAMEPLAY_HUD_AWAY_PLAYER_COLUMN,
            scene->pretip_team_data
                ->players[scene->launch.away_team]
                         [scene->actors[selected[TECMO_GAMEPLAY_TEAM_AWAY]]
                              .roster_index]
                .name) ||
        !scene_hud_put_player_name(
            scene, &prepared,
            TECMO_GAMEPLAY_HUD_HOME_PLAYER_COLUMN,
            scene->pretip_team_data
                ->players[scene->launch.home_team]
                         [scene->actors[selected[TECMO_GAMEPLAY_TEAM_HOME]]
                              .roster_index]
                .name)) {
        return false;
    }
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (prepared.occupied[row][column] &&
                !prepared.chr_resolved[row][column] &&
                !scene_background_tile_chr(
                    scene, context,
                    TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row,
                    prepared.tiles[row][column],
                    &prepared.chr_offsets[row][column])) {
                return false;
            }
        }
    }
    *prepared_out = prepared;
    return true;
}

static void scene_draw_live_hud(
    const TecmoGameplayScene *scene, TecmoFramebuffer *view,
    const TecmoGameplayPreparedHud *prepared, int scale,
    const uint8_t live_palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE])
{
    uint32_t palette[4];
    uint32_t backing;
    unsigned row;
    unsigned column;
    size_t color;
    palette[0] = tecmo_nes_2c02_rgba(live_palette[0]);
    backing = tecmo_nes_2c02_rgba(live_palette[1]);
    for (color = 1U; color < 4U; ++color) {
        palette[color] = tecmo_nes_2c02_rgba(live_palette[color]);
    }
    for (row = 0U; row < TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT; ++row) {
        for (column = 0U; column < TECMO_GAMEPLAY_HUD_COLUMN_COUNT;
             ++column) {
            if (!prepared->occupied[row][column]) continue;
            /* The captured live nametable presents every dynamic HUD cell on
               the palette's black backing. Clear the replaced cell before
               drawing because the shared CHR helper treats color zero as
               transparent for sprite composition. */
            scene_fill_rect(
                view, (int)column * 8 * scale,
                (int)(TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row) * 8 * scale,
                8 * scale, 8 * scale, backing);
            tecmo_draw_chr_tile_at_offset_ex(
                view, scene->assets.chr_storage,
                scene->assets.chr_storage_size,
                prepared->chr_offsets[row][column],
                (int)column * 8 * scale,
                (int)(TECMO_GAMEPLAY_HUD_PRIMARY_ROW + row) * 8 * scale,
                scale, palette, false, false);
        }
    }
}

static bool scene_framebuffer_subview(
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    TecmoFramebuffer *view)
{
    if (!scene_framebuffer_valid(framebuffer, origin_x, origin_y, scale) ||
        view == NULL) {
        return false;
    }
    view->pixels = framebuffer->pixels +
        (size_t)origin_y * (size_t)framebuffer->pitch_pixels +
        (size_t)origin_x;
    view->width = TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale;
    view->height = TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale;
    view->pitch_pixels = framebuffer->pitch_pixels;
    return true;
}

static bool scene_actor_palette_binding(const TecmoGameplayScene *scene,
                                        size_t actor_index,
                                        uint8_t *palette_group_out,
                                        uint8_t *uniform_color_out)
{
    uint8_t uniform_colors[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t team_id;
    uint8_t palette_group;
    uint8_t uniform_color;
    const TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    if (scene == NULL || palette_group_out == NULL ||
        uniform_color_out == NULL ||
        scene->pretip_team_data == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_team_data_resolve_gameplay_uniform_colors(
            scene->pretip_team_data, scene->launch.away_team,
            scene->launch.home_team, uniform_colors)) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (actor->team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        actor->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return false;
    }
    team_id = actor->team == TECMO_GAMEPLAY_TEAM_AWAY
                  ? scene->launch.away_team : scene->launch.home_team;
    if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT) return false;
    player = &scene->pretip_team_data->players[team_id][actor->roster_index];
    /* Bank02 $A8AE-$A8C9 rotates profile byte 2 bit 7 into $04B0 bit 0.
       Bank01 $B0ED then selects $B138/$B148 with that exact bit. */
    palette_group = (uint8_t)((player->profile[2U] & 0x80U) >> 7U);
    uniform_color = uniform_colors[actor->team];
    if (palette_group >= TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT ||
        uniform_color > 0x3FU) {
        return false;
    }
    *palette_group_out = palette_group;
    *uniform_color_out = uniform_color;
    return true;
}

static bool scene_build_matchup_live_palette(
    const TecmoGameplayScene *scene,
    uint8_t palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE])
{
    static const uint8_t fixed_live_palette[
        TECMO_GAMEPLAY_COURT_PALETTE_SIZE] = {
            0x16U,0x0FU,0x27U,0x30U,
            0x16U,0x0FU,0x17U,0x30U,
            0x16U,0x0FU,0x27U,0x12U,
            0x16U,0x0FU,0x17U,0x12U
    };
    uint8_t uniform_colors[TECMO_GAMEPLAY_TEAM_COUNT];
    if (scene == NULL || palette == NULL ||
        scene->court.palette == NULL ||
        memcmp(scene->court.palette, fixed_live_palette,
               sizeof(fixed_live_palette)) != 0 ||
        !tecmo_team_data_resolve_gameplay_uniform_colors(
            scene->pretip_team_data, scene->launch.away_team,
            scene->launch.home_team, uniform_colors)) {
        return false;
    }
    memcpy(palette, fixed_live_palette, sizeof(fixed_live_palette));
    /* Fixed $F2E2-$F2F1 is four profile/side palettes. Fixed
       $DEAB-$DEDF supplies the selected matchup colors for their fourth
       entries; $04B0 & 3 selects one of these four groups per actor. */
    palette[3U] = uniform_colors[TECMO_GAMEPLAY_TEAM_AWAY];
    palette[7U] = uniform_colors[TECMO_GAMEPLAY_TEAM_AWAY];
    palette[11U] = uniform_colors[TECMO_GAMEPLAY_TEAM_HOME];
    palette[15U] = uniform_colors[TECMO_GAMEPLAY_TEAM_HOME];
    return true;
}

static bool scene_apply_matchup_live_palette(
    const TecmoGameplayScene *scene,
    TecmoGameplayResolvedPose *pose)
{
    uint8_t palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE];
    size_t piece;
    if (pose == NULL ||
        !scene_build_matchup_live_palette(scene, palette)) {
        return false;
    }
    memcpy(pose->palette, palette, sizeof(pose->palette));
    for (piece = 0U; piece < pose->piece_count; ++piece) {
        uint8_t palette_index = pose->pieces[piece].palette_index;
        if (palette_index >= 4U) return false;
        memcpy(pose->pieces[piece].palette,
               palette + (size_t)palette_index * 4U,
               sizeof(pose->pieces[piece].palette));
    }
    return true;
}

static bool scene_resolve_pose(const TecmoGameplayScene *scene,
                               uint16_t pointer_index,
                               uint8_t actor_slot_base,
                               uint8_t actor_attributes,
                               uint8_t palette_group,
                               bool apply_uniform_color,
                               uint8_t uniform_color,
                               TecmoGameplayResolvedPose *pose)
{
    TecmoGameplayPoseContext context;
    TecmoGameplayResolvedPose first;
    memset(&context, 0, sizeof(context));
    context.actor_slot_base = actor_slot_base;
    context.actor_attributes = actor_attributes;
    context.palette_group = palette_group;
    context.uniform_color = uniform_color;
    context.apply_uniform_color = apply_uniform_color;
    context.mmc3_r2_r5[0] = 0x40U;
    context.mmc3_r2_r5[1] = 0x41U;
    context.mmc3_r2_r5[2] = 0x42U;
    context.mmc3_r2_r5[3] = 0x43U;
    if (!tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                            &context, &first)) {
        return false;
    }
    context.mmc3_r2_r5[(actor_slot_base >> 6U) & 0x03U] =
        first.record_tag;
    return tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                              &context, pose);
}

static bool scene_resolve_actor_pose(const TecmoGameplayScene *scene,
                                     size_t actor_index,
                                     TecmoGameplayResolvedPose *pose)
{
    uint8_t palette_group;
    uint8_t uniform_color;
    uint8_t actor_attributes;
    const TecmoGameplaySceneActor *actor;
    TecmoGameplayResolvedPose resolved;
    if (scene == NULL || pose == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (actor->team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        !scene_actor_palette_binding(scene, actor_index, &palette_group,
                                     &uniform_color)) {
        return false;
    }
    actor_attributes = (uint8_t)(palette_group | (actor->team << 1U));
    if (!scene_resolve_pose(scene, actor->pose_index,
                            actor->sprite_slot_base,
                            actor_attributes, palette_group, true,
                            uniform_color, &resolved) ||
        !scene_apply_matchup_live_palette(scene, &resolved)) {
        return false;
    }
    *pose = resolved;
    return true;
}

bool tecmo_gameplay_scene_in_dunk_presentation(
    const TecmoGameplayScene *scene)
{
    return scene != NULL &&
           scene->lifecycle_tag == TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG &&
           scene->active &&
           scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK &&
           scene->shot_frame >= TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
           scene->shot_frame < TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME;
}

static void scene_fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                            int width, int height, uint32_t color)
{
    int row;
    int column;
    for (row = 0; row < height; ++row) {
        uint32_t *pixels = framebuffer->pixels +
            (size_t)(y + row) * (size_t)framebuffer->pitch_pixels +
            (size_t)x;
        for (column = 0; column < width; ++column) {
            pixels[column] = color;
        }
    }
}

static bool scene_draw_dunk_presentation(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale)
{
    uint8_t stage;
    uint8_t side;
    uint8_t palette_group;
    uint8_t uniform_color;
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->shot_actor].team >=
            TECMO_GAMEPLAY_DUNK_SIDE_COUNT ||
        !scene_actor_palette_binding(scene, scene->shot_actor,
                                     &palette_group, &uniform_color)) {
        return false;
    }
    side = scene->actors[scene->shot_actor].team;
    if (scene->shot_frame >= TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME &&
        scene->shot_frame <= TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME) {
        return tecmo_gameplay_dunk_cutaway_stage_for_frame(
                   &scene->dunk_cutaway, scene->shot_frame, &stage) &&
               tecmo_gameplay_dunk_cutaway_draw(
                   &scene->dunk_cutaway, scene->assets.chr_storage,
                   scene->assets.chr_storage_size, framebuffer,
                   origin_x, origin_y, scale, side, palette_group,
                   uniform_color, stage);
    }
    scene_fill_rect(framebuffer, origin_x, origin_y,
                    TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale,
                    TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale,
                    tecmo_nes_2c02_rgba(0x0FU));
    return true;
}

static void scene_draw_pose(const TecmoGameplayScene *scene,
                            TecmoFramebuffer *framebuffer,
                            const TecmoGameplayResolvedPose *pose,
                            int base_x, int base_y,
                            int origin_x, int origin_y, int scale,
                            bool mirror_horizontal)
{
    size_t piece_index;
    for (piece_index = 0U; piece_index < pose->piece_count; ++piece_index) {
        const TecmoGameplayResolvedPiece *piece = &pose->pieces[piece_index];
        uint32_t palette[4] = {0U, 0U, 0U, 0U};
        size_t color;
        int piece_x = mirror_horizontal ? -piece->dx - 8 : piece->dx;
        bool flip_horizontal = piece->flip_horizontal ^ mirror_horizontal;
        int x = origin_x + (base_x + piece_x) * scale;
        int y = origin_y + (base_y + piece->dy) * scale;
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(piece->palette[color]);
        }
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, scene->assets.chr_storage,
            scene->assets.chr_storage_size, piece->top_chr_offset,
            x, y, scale, palette, flip_horizontal, false);
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, scene->assets.chr_storage,
            scene->assets.chr_storage_size, piece->bottom_chr_offset,
            x, y + 8 * scale, scale, palette,
            flip_horizontal, false);
    }
}

static void scene_make_bg_palette(uint32_t rgba[4],
                                  const uint8_t palette[16],
                                  uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    size_t color;
    rgba[0] = tecmo_nes_2c02_rgba(palette[0]);
    for (color = 1U; color < 4U; ++color)
        rgba[color] = tecmo_nes_2c02_rgba(palette[base + color]);
}

static void scene_make_sprite_palette(uint32_t rgba[4],
                                      const uint8_t palette[16],
                                      uint8_t index)
{
    size_t base = (size_t)(index & 3U) * 4U;
    size_t color;
    rgba[0] = 0U;
    for (color = 1U; color < 4U; ++color)
        rgba[color] = tecmo_nes_2c02_rgba(palette[base + color]);
}

static bool scene_draw_pretip_cell(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    const TecmoStartGameMenuCell *cell,
    const uint8_t palette[16],
    int palette_override,
    int x,
    int y,
    int scale)
{
    uint32_t rgba[4];
    uint8_t index;
    if (cell == NULL || cell->chr_offset + 16U >
            scene->assets.chr_storage_size) {
        return false;
    }
    index = palette_override >= 0
                ? (uint8_t)palette_override : cell->palette_index;
    if (index > 3U) return false;
    scene_make_bg_palette(rgba, palette, index);
    tecmo_draw_chr_tile_at_offset_ex(
        view, scene->assets.chr_storage, scene->assets.chr_storage_size,
        cell->chr_offset, x * scale, y * scale, scale, rgba, false, false);
    return true;
}

static bool scene_draw_pretip_text(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    const char *text,
    int x,
    int y,
    int scale,
    const uint8_t palette[16],
    int palette_index)
{
    size_t index;
    size_t length;
    if (text == NULL) return false;
    length = strlen(text);
    if (length > 16U || x < 0 || y < 0 ||
        x + (int)length * 16 > TECMO_GAMEPLAY_SCENE_NES_WIDTH ||
        y + 16 > TECMO_GAMEPLAY_SCENE_NES_HEIGHT) {
        return false;
    }
    for (index = 0U; text[index] != '\0'; ++index) {
        unsigned c = (unsigned char)text[index];
        uint8_t glyph;
        size_t tile_index;
        if (c == '.' || c == ' ') {
            glyph = 0x18U;
        } else if (c == '-') {
            glyph = 0x25U;
        } else if (c >= 0x17U && c < ':') {
            glyph = (uint8_t)(c - 0x17U);
        } else if (c >= 'A' && c <= 'Z') {
            size_t map_offset = c - 0x1DU;
            glyph = scene->pretip_assets.character_map[map_offset];
        } else {
            return false;
        }
        if (glyph >= TECMO_GAMEPLAY_PRETIP_GLYPH_COUNT) return false;
        for (tile_index = 0U;
             tile_index < TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT;
             ++tile_index) {
            uint8_t tile = scene->pretip_assets.character_tiles[
                (size_t)glyph * TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT +
                tile_index];
            uint8_t selector =
                scene->pretip_assets.card_chr_selector[
                    tile < 0x80U ? 0U : 1U];
            uint32_t chr_offset =
                (uint32_t)selector * 1024U +
                (uint32_t)(tile & 0x7FU) * 16U;
            uint32_t rgba[4];
            if (chr_offset + 16U > scene->assets.chr_storage_size ||
                palette_index < 0 || palette_index > 3) {
                return false;
            }
            scene_make_bg_palette(rgba, palette, (uint8_t)palette_index);
            tecmo_draw_chr_tile_at_offset_ex(
                view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, chr_offset,
                (x + (int)index * 16 +
                 (int)(tile_index % 2U) * 8) * scale,
                (y + (int)(tile_index / 2U) * 8) * scale,
                scale, rgba, false, false);
        }
    }
    return true;
}

static bool scene_draw_pretip_team(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *view,
    uint8_t team_id,
    int logo_x,
    int logo_y,
    int scale,
    bool dim)
{
    const TecmoTeamDataTeam *team;
    uint8_t logo_palette[16];
    size_t index;
    if (team_id >= TECMO_TEAM_DATA_REAL_TEAM_COUNT) return false;
    team = &scene->pretip_team_data->teams[team_id];
    if (team->logo_width == 0U || team->logo_height == 0U ||
        team->logo_count == 0U ||
        (size_t)team->logo_width * team->logo_height != team->logo_count ||
        team->logo_count > TECMO_TEAM_DATA_LOGO_CELL_LIMIT ||
        team->profile_palette_group >=
            TECMO_TEAM_DATA_PROFILE_PALETTE_COUNT ||
        logo_x < 0 || logo_y < 0 ||
        (int)team->logo_width * 8 >
            TECMO_GAMEPLAY_SCENE_NES_WIDTH - logo_x ||
        (int)team->logo_height * 8 >
            TECMO_GAMEPLAY_SCENE_NES_HEIGHT - logo_y) {
        return false;
    }
    memcpy(logo_palette,
           scene->pretip_team_data->profile_palettes[
               team->profile_palette_group],
           sizeof(logo_palette));
    logo_palette[0] = 0x0FU;
    if (dim) {
        for (index = 1U; index < sizeof(logo_palette); ++index) {
            logo_palette[index] = logo_palette[index] >= 0x10U
                                      ? (uint8_t)(logo_palette[index] - 0x10U)
                                      : 0x0FU;
        }
    }
    for (index = 0U; index < team->logo_count; ++index) {
        int column = (int)(index % team->logo_width);
        int row = (int)(index / team->logo_width);
        if (!scene_draw_pretip_cell(
                scene, view, &scene->pretip_team_data->logos[team_id][index],
                logo_palette, -1, logo_x + column * 8,
                logo_y + row * 8, scale)) {
            return false;
        }
    }
    return true;
}

static bool scene_draw_pretip_template(const TecmoGameplayScene *scene,
                                       TecmoFramebuffer *view,
                                       int scale)
{
    (void)scene;
    (void)scale;
    /*
     * Screen $15 is the ROM's blank dynamic-card nametable. Its visible
     * backdrop is universal black; all card lettering is subsequently written
     * by Bank06 $A125 rather than baked into the decoded nametable.
     */
    scene_fill_rect(view, 0, 0, view->width, view->height,
                    tecmo_nes_2c02_rgba(0x0FU));
    return true;
}

static uint8_t scene_pretip_closeup_motion(uint16_t phase_frame)
{
    uint16_t motion =
        phase_frame > 33U ? (uint16_t)((phase_frame - 33U) / 2U) : 0U;
    return motion < 25U ? (uint8_t)motion : 25U;
}

static bool scene_draw_pretip_closeup(const TecmoGameplayScene *scene,
                                      TecmoFramebuffer *view,
                                      int scale,
                                      uint16_t phase_frame)
{
    size_t index;
    uint8_t motion = scene_pretip_closeup_motion(phase_frame);
    if (scene->pretip_closeup == NULL ||
        !scene->pretip_closeup->available) return false;
    scene_fill_rect(view, 0, 0, view->width, view->height,
                    tecmo_nes_2c02_rgba(
                        scene->pretip_closeup->background_palette[0]));
    for (index = 0U; index < 960U; ++index) {
        const TecmoIntroWarriorsTile *cell =
            &scene->pretip_closeup->pages[0][index];
        uint32_t rgba[4];
        scene_make_bg_palette(
            rgba, scene->pretip_closeup->background_palette,
            cell->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, cell->moving_chr_offset,
            ((int)(index % 32U) * 8 + motion) * scale,
            (int)(index / 32U) * 8 * scale,
            scale, rgba, false, false);
    }
    scene_fill_rect(view, 0, 42 * scale, view->width, 4 * scale,
                    tecmo_nes_2c02_rgba(0x30U));
    scene_fill_rect(view, 0, 162 * scale, view->width, 4 * scale,
                    tecmo_nes_2c02_rgba(0x30U));
    for (index = 0U; index < TECMO_INTRO_WARRIORS_PIECE_COUNT; ++index) {
        const TecmoIntroWarriorsPiece *piece =
            &scene->pretip_closeup->pieces[index];
        uint32_t rgba[4];
        bool flip_x = (piece->flags & 1U) != 0U;
        bool flip_y = (piece->flags & 2U) != 0U;
        uint32_t top = flip_y ? piece->bottom_chr_offset
                              : piece->top_chr_offset;
        uint32_t bottom = flip_y ? piece->top_chr_offset
                                 : piece->bottom_chr_offset;
        int x = (98 - motion + piece->dx) * scale;
        /* D861 writes OAM Y; NES hardware displays the sprite one scanline
           below that stored coordinate. */
        int y = (93 + piece->dy) * scale;
        scene_make_sprite_palette(
            rgba, scene->pretip_closeup->sprite_palette,
            piece->palette_index);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, top,
            x, y, scale, rgba, flip_x, flip_y);
        tecmo_draw_chr_tile_at_offset_ex(
            view, scene->assets.chr_storage,
            scene->assets.chr_storage_size, bottom,
            x, y + 8 * scale, scale, rgba, flip_x, flip_y);
    }
    return true;
}

static int scene_centered_text_x(const char *text)
{
    size_t length = text != NULL ? strlen(text) : 0U;
    if (length > 16U) return -1;
    return (int)((16U - length) / 2U) * 16;
}

static void scene_make_pretip_card_palette(const TecmoGameplayScene *scene,
                                           uint8_t palette[16],
                                           bool dim)
{
    size_t index;
    memcpy(palette, scene->pretip_assets.palette, 16U);
    palette[0] = 0x0FU;
    if (!dim) return;
    for (index = 1U; index < 16U; ++index) {
        palette[index] = palette[index] >= 0x10U
                             ? (uint8_t)(palette[index] - 0x10U)
                             : 0x0FU;
    }
}

static bool scene_draw_pretip_cards(const TecmoGameplayScene *scene,
                                    TecmoFramebuffer *framebuffer,
                                    int origin_x,
                                    int origin_y,
                                    int scale)
{
    TecmoFramebuffer view;
    TecmoGameplayPreTipPhase phase = scene->pretip_state.phase;
    uint16_t phase_frame = scene->pretip_state.phase_frame;
    uint8_t palette[16];
    bool dim;
    const TecmoTeamDataTeam *away;
    const TecmoTeamDataTeam *home;
    const char *mode_text;
    if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view)) {
        return false;
    }
    if (phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
        if (phase_frame < 28U ||
            phase_frame + 30U >= scene->pretip_assets.phase_frames[phase]) {
            scene_fill_rect(&view, 0, 0, view.width, view.height,
                            tecmo_nes_2c02_rgba(0x0FU));
            return true;
        }
        return scene_draw_pretip_closeup(
            scene, &view, scale, phase_frame);
    }
    if (!scene_draw_pretip_template(scene, &view, scale)) return false;
    if (phase == TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
        phase_frame < 16U) {
        return true;
    }
    dim = (phase == TECMO_GAMEPLAY_PRETIP_MATCHUP && phase_frame < 30U) ||
          (phase == TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
           phase_frame < 29U);
    scene_make_pretip_card_palette(scene, palette, dim);
    if (phase == TECMO_GAMEPLAY_PRETIP_PRESEASON) {
        mode_text = scene->launch.source == TECMO_GAMEPLAY_SCENE_PRESEASON
                        ? "PRESEASON" : "REGULAR SEASON";
        return scene_draw_pretip_text(
            scene, &view, mode_text, scene_centered_text_x(mode_text), 112,
            scale, palette, 2);
    }
    if (phase == TECMO_GAMEPLAY_PRETIP_MATCHUP) {
        away = &scene->pretip_team_data->teams[scene->launch.away_team];
        home = &scene->pretip_team_data->teams[scene->launch.home_team];
        return scene_draw_pretip_team(
                   scene, &view, scene->launch.away_team,
                   16, 32, scale, dim) &&
               scene_draw_pretip_text(
                   scene, &view, away->city,
                   scene_centered_text_x(away->city), 80, scale, palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, away->nickname,
                   scene_centered_text_x(away->nickname), 96, scale,
                   palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, "VS", scene_centered_text_x("VS"), 144,
                   scale, palette, 2) &&
               scene_draw_pretip_team(
                   scene, &view, scene->launch.home_team,
                   16, 128, scale, dim) &&
               scene_draw_pretip_text(
                   scene, &view, home->city,
                   scene_centered_text_x(home->city), 176, scale,
                   palette, 2) &&
               scene_draw_pretip_text(
                   scene, &view, home->nickname,
                   scene_centered_text_x(home->nickname), 192, scale,
                   palette, 2);
    }
    return scene_draw_pretip_text(
        scene, &view, "1ST PERIOD", scene_centered_text_x("1ST PERIOD"), 112,
        scale, palette, 2);
}

static bool scene_draw_pretip_descriptor_screen(
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint8_t screen_index,
    uint8_t nametable_page)
{
    TecmoFramebuffer view;
    const TecmoGameplayScreenAsset *screen;
    unsigned row;
    unsigned column;
    if (screen_index >= TECMO_GAMEPLAY_ASSET_SCREEN_COUNT ||
        !scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view)) {
        return false;
    }
    screen = &scene->assets.screens[screen_index];
    scene_fill_rect(&view, 0, 0, view.width, view.height,
                    tecmo_nes_2c02_rgba(screen->palette[0]));
    for (row = 0U; row < 30U; ++row) {
        for (column = 0U; column < 32U; ++column) {
            TecmoGameplayResolvedOrientationTile tile;
            uint32_t rgba[4];
            size_t color;
            if (!tecmo_gameplay_assets_resolve_descriptor_tile(
                    &scene->assets, screen_index, nametable_page,
                    (uint8_t)row, (uint8_t)column, &tile)) {
                return false;
            }
            rgba[0] = tecmo_nes_2c02_rgba(screen->palette[0]);
            for (color = 1U; color < 4U; ++color)
                rgba[color] = tecmo_nes_2c02_rgba(tile.palette[color]);
            tecmo_draw_chr_tile_at_offset_ex(
                &view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, tile.chr_offset,
                (int)column * 8 * scale, (int)row * 8 * scale,
                scale, rgba, false, false);
        }
    }
    return true;
}

bool tecmo_gameplay_scene_draw(const TecmoGameplayScene *scene,
                               TecmoFramebuffer *framebuffer,
                               int origin_x,
                               int origin_y,
                               int scale,
                               bool include_actors)
{
    TecmoGameplayLiveBackgroundContext background_context;
    TecmoGameplaySceneCourtFrame court_frame;
    TecmoFramebuffer view;
    TecmoGameplayPreparedHud prepared_hud;
    TecmoGameplayResolvedPose actor_poses[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayResolvedPose ball_pose;
    uint8_t live_palette[TECMO_GAMEPLAY_COURT_PALETTE_SIZE];
    uint8_t order[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    bool draw_live_hud;
    unsigned row;
    unsigned column;
    size_t actor;
    size_t left;

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available ||
        !tecmo_gameplay_camera_state_live_valid(
            &scene->camera_assets, &scene->camera_state) ||
        !scene_framebuffer_valid(framebuffer, origin_x, origin_y, scale)) {
        return false;
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        scene->pretip_state.phase <= TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
        return scene_draw_pretip_cards(
            scene, framebuffer, origin_x, origin_y, scale);
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        (scene->pretip_state.phase ==
             TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
         (scene->pretip_state.phase ==
              TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP &&
          scene->pretip_state.phase_frame < 30U))) {
        if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                       scale, &view)) {
            return false;
        }
        scene_fill_rect(&view, 0, 0, view.width, view.height,
                        tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
        /* TGPL screen $1B page 1 is the ROM phase with ball X 176..239
           and hands X 67..159; page 0 is the preceding/opposite phase. */
        if (scene->assets.screens[0].screen_id != 0x1BU) return false;
        return scene_draw_pretip_descriptor_screen(
            scene, framebuffer, origin_x, origin_y, scale, 0U, 1U);
    }
    if (tecmo_gameplay_scene_in_dunk_presentation(scene)) {
        return scene_draw_dunk_presentation(
            scene, framebuffer, origin_x, origin_y, scale);
    }
    if (!scene_framebuffer_subview(framebuffer, origin_x, origin_y,
                                   scale, &view) ||
        !scene_build_matchup_live_palette(scene, live_palette)) {
        return false;
    }
    memset(&court_frame, 0, sizeof(court_frame));
    if (include_actors && scene->active) {
        if (!tecmo_gameplay_scene_court_frame(scene, &court_frame)) {
            return false;
        }
    } else if (!tecmo_gameplay_scene_court_slice(
                   scene, &court_frame.slice)) {
        return false;
    }
    if (!scene_build_background_context(scene, &background_context) ||
        court_frame.slice.viewport.column_count < 32U ||
        court_frame.slice.viewport.column_count >
            TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE) {
        return false;
    }
    draw_live_hud = include_actors && scene->active &&
                    !tecmo_gameplay_scene_in_pretip(scene);
    for (row = 0U; row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
        for (column = 0U;
             column < court_frame.slice.viewport.column_count;
             ++column) {
            size_t cell =
                (size_t)row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE +
                column;
            uint32_t offset;
            if (court_frame.slice.viewport.palette_indices[cell] > 3U ||
                !scene_background_tile_chr(
                    scene, &background_context, row,
                    court_frame.slice.viewport.tiles[cell], &offset) ||
                offset + 16U >
                    scene->assets.chr_storage_size) {
                return false;
            }
        }
    }
    if (draw_live_hud &&
        !scene_prepare_live_hud(
            scene, &background_context, &prepared_hud)) {
        return false;
    }
    if (include_actors && scene->active) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            order[actor] = (uint8_t)actor;
            if (!scene_resolve_actor_pose(scene, actor,
                                          &actor_poses[actor])) {
                return false;
            }
        }
        if (!scene_resolve_pose(scene, TECMO_GAMEPLAY_BALL_POSE, 0xC1U,
                                0U, 0U, false, 0U, &ball_pose) ||
            !scene_apply_matchup_live_palette(scene, &ball_pose)) {
            return false;
        }
    }

    scene_fill_rect(&view, 0, 0, view.width, view.height,
                    tecmo_nes_2c02_rgba(live_palette[0]));
    for (row = 0U; row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
        for (column = 0U;
             column < court_frame.slice.viewport.column_count;
             ++column) {
            size_t cell =
                (size_t)row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE +
                column;
            uint32_t offset;
            uint8_t palette_index =
                court_frame.slice.viewport.palette_indices[cell];
            uint32_t palette[4];
            size_t color;
            (void)scene_background_tile_chr(
                scene, &background_context, row,
                court_frame.slice.viewport.tiles[cell], &offset);
            palette[0] = tecmo_nes_2c02_rgba(live_palette[0]);
            for (color = 1U; color < 4U; ++color) {
                palette[color] = tecmo_nes_2c02_rgba(
                    live_palette[(size_t)palette_index * 4U + color]);
            }
            tecmo_draw_chr_tile_at_offset_ex(
                &view, scene->assets.chr_storage,
                scene->assets.chr_storage_size, offset,
                ((int)column * 8 -
                 (int)court_frame.slice.viewport.fine_scroll_x) * scale,
                (int)row * 8 * scale,
                scale, palette, false, false);
        }
    }
    if (draw_live_hud) {
        scene_draw_live_hud(
            scene, &view, &prepared_hud, scale, live_palette);
    }
    if (!include_actors || !scene->active) return true;

    for (left = 0U; left < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++left) {
        size_t right;
        for (right = left + 1U; right < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++right) {
            if (scene->actors[order[right]].position.y <
                scene->actors[order[left]].position.y) {
                uint8_t swap = order[left];
                order[left] = order[right];
                order[right] = swap;
            }
        }
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        uint8_t index = order[actor];
        if (!court_frame.projection.players[index].visible) continue;
        /* Actor-facing horizontal mirroring is an implementation-owned scene
           approximation. Live close shots still resolve only the explicitly
           supported TGCS profile-0/direction-0 slice. */
        scene_draw_pose(scene, &view, &actor_poses[index],
                        court_frame.projection.players[index].screen_x,
                        court_frame.projection.players[index].screen_y,
                        0, 0, scale,
                        !scene->actors[index].pose_orientation_encoded &&
                        !scene->actors[index].facing_right);
    }
    if (court_frame.projection.ball.visible) {
        scene_draw_pose(scene, &view, &ball_pose,
                        court_frame.projection.ball.screen_x,
                        court_frame.projection.ball.screen_y,
                        0, 0, scale, false);
    }
    return true;
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
    if (occupied[0U] != 21U || occupied[1U] != 26U ||
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
            font['2' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][2U] !=
            font['4' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][4U] !=
            font[selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.tiles[1U][5U] !=
            font['.' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        prepared.chr_offsets[1U][4U] !=
            scene->pretip_team_data->font[
                selected_initial - TECMO_GAMEPLAY_HUD_FONT_FIRST]
                .chr_offset) {
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
        dynamic_prepared.tiles[1U][1U] !=
            font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST] ||
        dynamic_prepared.tiles[1U][2U] !=
            font['9' - TECMO_GAMEPLAY_HUD_FONT_FIRST]) {
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
    return dynamic_prepared.tiles[1U][4U] ==
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
        !scene->jump_b_released) {
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
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0x0308U &&
               scene->actors[scene->shot_actor].pose_index == 325U;
    case 4U:
        return scene->jump_phase_counter == 0x00U &&
               scene->actors[scene->shot_actor].pose_index == 325U;
    case 5U:
        return scene->jump_phase_counter == 0x30U &&
               scene->actors[scene->shot_actor].pose_index == 1060U;
    case 8U:
        return !scene->jump_b_released &&
               scene->jump_phase_counter == 0x00U &&
               scene->actors[scene->shot_actor].pose_index == 1060U;
    case 9U:
        return scene->jump_b_released &&
               scene->jump_phase_counter == 0x30U &&
               scene->actors[scene->shot_actor].pose_index == 1061U;
    case 10U:
        return scene->jump_actor_state == 0x0BU &&
               scene->jump_phase_counter == 0x31U &&
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
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
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
        for (frame = 0U; frame < 400U &&
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
        scene.actors[0].pose_index != 181U ||
        scene.actors[TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT].pose_index !=
            85U ||
        scene.ball_position.x_q8 != 0x0167 * 256 ||
        scene.ball_position.y_q8 != 180 * 256 ||
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
        coordinates.ball.x_q8 != 0x0167 * 256 ||
        coordinates.ball.y_q8 != 180 * 256 ||
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
        court_projection.ball.screen_x != 0x67U ||
        court_projection.ball.screen_y != 180U) {
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
        court_projection.ball.screen_y != 180U) {
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
    if (!scene_handoff_possession(
            &camera_probe, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) ||
        camera_probe.camera_state.camera_x != camera_before.camera_x ||
        camera_probe.camera_state.scroll_x != camera_before.scroll_x ||
        camera_probe.camera_state.thresholds_valid ||
        camera_probe.camera_state.endpoint_latched ||
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

    scene.action_serial = 0U;
    scene.actors[scene.ball_holder].facing_right = false;
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.action_serial != 0U ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "left-facing ordinary jump was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[scene.ball_holder].facing_right = true;
    scene.actors[scene.ball_holder].position.x = 0x0108;
    scene.actors[scene.ball_holder].position.y = 0x0070;
    scene.action_serial = 1U;
    scene_attach_ball(&scene);
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
    scene.action_serial = 0U;
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        scene_test_message(message, message_size,
                           "home-side rejection setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[5].position.x = 0x013CU;
    scene.actors[5].position.y = 180;
    scene.actors[5].facing_right = true;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.action_serial != 0U || scene.ball_holder != 5U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "home-side ordinary jump was accepted");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    if (!scene_handoff_possession(
            &scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        scene_test_message(message, message_size,
                           "ordinary-jump rejection reset failed");
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
            TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_POSE) {
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
        scene.jump_actor_altitude_q8 != 0x02E8U ||
        scene.jump_actor_velocity_q8 != 0x02E8U ||
        scene.actors[scene.shot_actor].pose_index != 213U ||
        scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "NES B jump-shot contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_frame != 1U || scene.jump_b_released ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        scene.jump_actor_state != 0x0CU ||
        scene.jump_ball_state != 0x01U ||
        scene.jump_actor_altitude_q8 != 0x02E8U ||
        scene.jump_actor_velocity_q8 != 0x02E8U ||
        scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "current-B held jump state diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_frame != 2U || !scene.jump_b_released ||
        scene.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        scene.jump_actor_state != 0x0DU ||
        scene.jump_ball_state != 0x05U ||
        scene.jump_phase_counter != 0x04U ||
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
    if (!scene_queue_result_audio(&scene, TECMO_GAMEPLAY_TEAM_HOME) ||
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
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            !scene.audio_player.dmc.active ||
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
    for (frame = 0U; frame < 300U &&
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

    /* Held-ball/dribble DMC follows displacement of the actual holder at the
       native scene cadence; unrelated pad activity cannot trigger it. */
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
    p1.held.right = true;
    for (frame = 0U; frame <= TECMO_GAMEPLAY_DRIBBLE_CADENCE; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < TECMO_GAMEPLAY_DRIBBLE_CADENCE &&
             scene.audio_player.dmc.active)) {
            scene_test_message(
                message, message_size,
                "human holder TGMO cadence queued an early DMC");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!scene.audio_player.dmc.active) {
        scene_test_message(
            message, message_size,
            "human holder TGMO displacement missed cadence DMC");
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
    for (frame = 0U; frame <= TECMO_GAMEPLAY_DRIBBLE_CADENCE; ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            (frame < TECMO_GAMEPLAY_DRIBBLE_CADENCE &&
             scene.audio_player.dmc.active)) {
            scene_test_message(
                message, message_size,
                "CPU holder TGAI/TGMO cadence queued an early DMC");
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
            TECMO_GAMEPLAY_DRIBBLE_CADENCE + 1U) {
        scene_test_message(message, message_size,
                           "CPU holder TGMO displacement missed cadence DMC");
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
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
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
