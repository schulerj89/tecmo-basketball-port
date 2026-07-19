#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG 0x53434E31U
#define TECMO_GAMEPLAY_TEAM_LIMIT 27U
#define TECMO_GAMEPLAY_POSE_NEUTRAL 509U
#define TECMO_GAMEPLAY_BALL_POSE 64U
#define TECMO_GAMEPLAY_HOOP_X 224
#define TECMO_GAMEPLAY_HOOP_Y 128
#define TECMO_GAMEPLAY_MIN_X 10
#define TECMO_GAMEPLAY_MAX_X 246
#define TECMO_GAMEPLAY_MIN_Y 54
#define TECMO_GAMEPLAY_MAX_Y 226
#define TECMO_GAMEPLAY_CLOSE_DISTANCE_X 48
#define TECMO_GAMEPLAY_DRIBBLE_CADENCE 24U
#define TECMO_GAMEPLAY_FREE_THROW_FRAMES 60U
#define TECMO_GAMEPLAY_SCENE_RENDER_FNV1A32 0x82031A59U

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
    tecmo_gameplay_close_shots_destroy(&scene->close_shots);
    tecmo_gameplay_court_destroy(&scene->court);
    tecmo_gameplay_assets_destroy(&scene->assets);
    memset(scene, 0, sizeof(*scene));
    scene->lifecycle_tag = TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG;
    tecmo_gameplay_assets_init(&scene->assets);
    tecmo_gameplay_court_init(&scene->court);
    tecmo_gameplay_close_shots_init(&scene->close_shots);
    scene_set_status(scene, "gameplay scene initialized; assets not loaded");
}

void tecmo_gameplay_scene_init(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    memset(scene, 0, sizeof(*scene));
    scene->lifecycle_tag = TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG;
    tecmo_gameplay_assets_init(&scene->assets);
    tecmo_gameplay_court_init(&scene->court);
    tecmo_gameplay_close_shots_init(&scene->close_shots);
    scene_set_status(scene, "gameplay scene initialized; assets not loaded");
}

bool tecmo_gameplay_scene_load(TecmoGameplayScene *scene,
                               const char *project_root,
                               const char *asset_pack_path,
                               TecmoMusicPlayer *music_player)
{
    char selected[1024];
    char failure[192];

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG) {
        return false;
    }
    scene_release_owned(scene);
    if (!scene_select_asset_pack(selected, sizeof(selected), project_root,
                                 asset_pack_path)) {
        scene_set_status(scene, "gameplay asset pack unavailable");
        return false;
    }
    if (!tecmo_gameplay_assets_load(&scene->assets, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s", scene->assets.status);
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
    if (!tecmo_gameplay_close_shots_load(&scene->close_shots, selected)) {
        (void)snprintf(failure, sizeof(failure), "%s",
                       scene->close_shots.status);
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
                     "native gameplay ready: TGPL-1/TGCT-1/TGCS-1/TSFX-1/TDMC-1");
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

static void scene_initialize_actors(TecmoGameplayScene *scene)
{
    static const int16_t x[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
        96, 124, 151, 178, 207, 111, 139, 166, 194, 222
    };
    static const int16_t y[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT] = {
        198, 167, 207, 151, 183, 214, 190, 169, 205, 145
    };
    size_t actor;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        TecmoGameplaySceneActor *item = &scene->actors[actor];
        item->x = x[actor];
        item->y = y[actor];
        item->anchor_x = x[actor];
        item->anchor_y = y[actor];
        item->pose_index = TECMO_GAMEPLAY_POSE_NEUTRAL;
        item->team = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                         ? TECMO_GAMEPLAY_TEAM_AWAY
                         : TECMO_GAMEPLAY_TEAM_HOME;
        item->roster_index = (uint8_t)(actor %
            TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT);
        item->facing_right = item->team == TECMO_GAMEPLAY_TEAM_AWAY;
        item->active = true;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++actor) {
        if (scene->launch.controller_team[actor] == TECMO_GAMEPLAY_TEAM_AWAY) {
            scene->controlled_actor[actor] = 0U;
        } else if (scene->launch.controller_team[actor] ==
                   TECMO_GAMEPLAY_TEAM_HOME) {
            scene->controlled_actor[actor] =
                TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        } else {
            scene->controlled_actor[actor] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        }
    }
    scene->ball_holder = 0U;
    scene->ball_x_q8 = (int32_t)(scene->actors[0].x + 7) * 256;
    scene->ball_y_q8 = (int32_t)(scene->actors[0].y - 18) * 256;
}

bool tecmo_gameplay_scene_launch(TecmoGameplayScene *scene,
                                 const TecmoGameplaySceneLaunch *launch)
{
    TecmoGameplayConfig config;
    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene_launch_valid(launch) ||
        !tecmo_gameplay_config_init(&config, launch->regulation_minutes)) {
        return false;
    }
    scene->launch = *launch;
    memset(&scene->result, 0, sizeof(scene->result));
    tecmo_gameplay_events_clear(&scene->events);
    if (!tecmo_gameplay_state_init(&scene->state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        scene_set_status(scene, "gameplay state initialization rejected");
        return false;
    }
    scene_initialize_actors(scene);
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->close_shot_profile = TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0;
    scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    scene->frame = 0U;
    scene->action_serial = 0U;
    scene->free_throw_frame = 0U;
    scene->previous_phase = scene->state.phase;
    scene->result_ready = false;
    scene->active = true;
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    tecmo_gameplay_audio_set_game_music_enabled(
        &scene->audio_player, launch->game_music_enabled);
    if (launch->game_music_enabled) {
        (void)tecmo_gameplay_audio_queue_game_music(&scene->audio_player);
    }
    scene_set_status(scene, "native gameplay active");
    return true;
}

static TecmoGameplayTeam scene_other_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? TECMO_GAMEPLAY_TEAM_HOME
               : TECMO_GAMEPLAY_TEAM_AWAY;
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

static void scene_clamp_actor(TecmoGameplaySceneActor *actor)
{
    if (actor->x < TECMO_GAMEPLAY_MIN_X) actor->x = TECMO_GAMEPLAY_MIN_X;
    if (actor->x > TECMO_GAMEPLAY_MAX_X) actor->x = TECMO_GAMEPLAY_MAX_X;
    if (actor->y < TECMO_GAMEPLAY_MIN_Y) actor->y = TECMO_GAMEPLAY_MIN_Y;
    if (actor->y > TECMO_GAMEPLAY_MAX_Y) actor->y = TECMO_GAMEPLAY_MAX_Y;
}

static void scene_move_controlled_actor(TecmoGameplayScene *scene,
                                        size_t controller,
                                        const TecmoControlFrame *controls)
{
    uint8_t actor_index;
    TecmoGameplaySceneActor *actor;
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT || controls == NULL ||
        scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM) {
        return;
    }
    actor_index = scene->controlled_actor[controller];
    if (actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return;
    actor = &scene->actors[actor_index];
    if (actor->team != scene->launch.controller_team[controller]) return;
    if (controls->held.left) {
        --actor->x;
        actor->facing_right = false;
    }
    if (controls->held.right) {
        ++actor->x;
        actor->facing_right = true;
    }
    if (controls->held.up) --actor->y;
    if (controls->held.down) ++actor->y;
    scene_clamp_actor(actor);
}

static uint8_t scene_first_actor_for_team(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY
               ? 0U
               : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
}

static void scene_attach_ball(TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *holder;
    if (scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return;
    holder = &scene->actors[scene->ball_holder];
    scene->ball_x_q8 = (int32_t)(holder->x +
        (holder->facing_right ? 7 : -7)) * 256;
    scene->ball_y_q8 = (int32_t)(holder->y - 17) * 256;
}

static uint32_t scene_distance_squared(const TecmoGameplaySceneActor *a,
                                       const TecmoGameplaySceneActor *b)
{
    int32_t dx = (int32_t)a->x - b->x;
    int32_t dy = (int32_t)a->y - b->y;
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

static void scene_pass_or_switch(TecmoGameplayScene *scene,
                                 size_t controller)
{
    TecmoGameplayTeam team;
    if (controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[controller] ==
            TECMO_GAMEPLAY_SCENE_NO_TEAM) {
        return;
    }
    team = (TecmoGameplayTeam)scene->launch.controller_team[controller];
    if (team == scene->state.possession &&
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        uint8_t next = scene_next_teammate(scene, scene->ball_holder);
        if (next < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            scene->ball_holder = next;
            scene->controlled_actor[controller] = next;
            scene_attach_ball(scene);
        }
    } else {
        scene->controlled_actor[controller] =
            scene_nearest_actor_for_team(scene, team, scene->ball_holder);
    }
}

static uint16_t scene_jump_pose(uint16_t frame)
{
    /* Provisional native timing over ROM pose candidates seen in one bounded
       rightward subtype-01 trace. This is not claimed as a universal ROM
       jump-shot schedule; exact normal-shot timing remains unresolved. */
    static const uint16_t poses[7] = {
        445U, 254U, 255U, 257U, 258U, 259U,
        TECMO_GAMEPLAY_POSE_NEUTRAL
    };
    size_t index = frame / 5U;
    if (index >= sizeof(poses) / sizeof(poses[0])) {
        index = sizeof(poses) / sizeof(poses[0]) - 1U;
    }
    return poses[index];
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
    int distance_x;
    int distance_y;
    bool close;
    TecmoGameplayCloseShotVariantInfo close_info;
    uint16_t initial_pose = TECMO_GAMEPLAY_POSE_NEUTRAL;
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
    distance_x = TECMO_GAMEPLAY_HOOP_X - actor->x;
    distance_y = TECMO_GAMEPLAY_HOOP_Y - actor->y;
    close = distance_x >= -8 && distance_x <= TECMO_GAMEPLAY_CLOSE_DISTANCE_X &&
            distance_y >= -64 && distance_y <= 80;
    if (close) {
        /* The numeric ROM families and pose timing are exact. The distance
           threshold selecting between them remains a native scene policy. */
        scene->shot_kind = distance_x <= 24
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
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
        memset(&close_info, 0, sizeof(close_info));
    }
    scene->shot_actor = actor_index;
    scene->shot_frame = 0U;
    scene->shot_duration = close ? close_info.step_count : 40U;
    scene->shot_points = close || distance_x < 104 ? 2U : 3U;
    scene->shot_start_x_q8 = (int32_t)(actor->x +
        (actor->facing_right ? 7 : -7)) * 256;
    scene->shot_start_y_q8 = (int32_t)(actor->y - 18) * 256;
    scene->shot_end_x_q8 = TECMO_GAMEPLAY_HOOP_X * 256;
    scene->shot_end_y_q8 = (TECMO_GAMEPLAY_HOOP_Y - 5) * 256;
    scene->ball_x_q8 = scene->shot_start_x_q8;
    scene->ball_y_q8 = scene->shot_start_y_q8;
    scene->ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    ++scene->action_serial;
    actor->pose_index = close ? initial_pose : scene_jump_pose(0U);
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
    distance = (uint32_t)(abs(TECMO_GAMEPLAY_HOOP_X - actor->x) +
                          abs(TECMO_GAMEPLAY_HOOP_Y - actor->y));
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
    if (scene->state.possession != possession &&
        !tecmo_gameplay_reset_possession(&scene->state, possession)) {
        return false;
    }
    if (scene->state.possession != possession) return false;
    scene->ball_holder = holder;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == possession) {
            scene->controlled_actor[controller] = holder;
        }
    }
    scene_attach_ball(scene);
    return true;
}

static bool scene_update_shot(TecmoGameplayScene *scene)
{
    int64_t duration;
    int64_t frame;
    int64_t arc;
    TecmoGameplaySceneActor *actor;
    TecmoGameplayTeam shooting_team;
    TecmoGameplayTeam next_team;
    bool made;
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration == 0U) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    shooting_team = (TecmoGameplayTeam)actor->team;
    ++scene->shot_frame;
    duration = scene->shot_duration;
    frame = scene->shot_frame < scene->shot_duration
                ? scene->shot_frame
                : scene->shot_duration;
    scene->ball_x_q8 = scene->shot_start_x_q8 +
        (int32_t)(((int64_t)(scene->shot_end_x_q8 -
                             scene->shot_start_x_q8) * frame) / duration);
    scene->ball_y_q8 = scene->shot_start_y_q8 +
        (int32_t)(((int64_t)(scene->shot_end_y_q8 -
                             scene->shot_start_y_q8) * frame) / duration);
    arc = (4LL * frame * (duration - frame) *
           (scene_shot_is_close(scene->shot_kind)
                ? 18LL
                : 34LL) * 256LL) /
          (duration * duration);
    scene->ball_y_q8 -= (int32_t)arc;

    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        actor->pose_index = scene_jump_pose(scene->shot_frame);
    } else {
        uint16_t pose_index;
        uint16_t bounded_step = scene->shot_frame < scene->shot_duration
                                    ? scene->shot_frame
                                    : scene->shot_duration - 1U;
        scene->close_shot_step = (uint8_t)bounded_step;
        if (!scene_close_pose_for_step(scene, scene->close_shot_step,
                                       &pose_index)) {
            return false;
        }
        actor->pose_index = pose_index;
        /* TGCS supplies one exact numeric pose phase per native scene step.
           Do not advance the unrelated bounded rightward trace on an invented
           cadence; the pure-state semantic chain remains untouched. */
    }

    if (scene->shot_frame < scene->shot_duration) return true;
    made = scene_shot_will_score(scene);
    if (made) {
        if (!tecmo_gameplay_award_points(&scene->state, shooting_team,
                                         scene->shot_points)) {
            return false;
        }
        (void)tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE);
    }
    actor->pose_index = TECMO_GAMEPLAY_POSE_NEUTRAL;
    next_team = scene_other_team(shooting_team);
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    if (scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        if (!scene_handoff_possession(
                scene, scene->state.possession,
                scene_first_actor_for_team(scene->state.possession))) {
            return false;
        }
    } else if (!scene_handoff_possession(
                   scene, next_team, scene_first_actor_for_team(next_team))) {
        return false;
    }
    return true;
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

static bool scene_team_has_controller(const TecmoGameplayScene *scene,
                                      TecmoGameplayTeam team)
{
    size_t controller;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == team) return true;
    }
    return false;
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

static bool scene_update_ai(TecmoGameplayScene *scene)
{
    size_t actor;
    if ((scene->frame & 3U) == 0U) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            TecmoGameplaySceneActor *item = &scene->actors[actor];
            if (scene_actor_is_controlled(scene, actor) ||
                actor == scene->ball_holder || actor == scene->shot_actor) {
                continue;
            }
            if (item->x < item->anchor_x) ++item->x;
            if (item->x > item->anchor_x) --item->x;
            if (item->y < item->anchor_y) ++item->y;
            if (item->y > item->anchor_y) --item->y;
        }
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
        !scene_team_has_controller(scene, scene->state.possession)) {
        TecmoGameplaySceneActor *holder = &scene->actors[scene->ball_holder];
        int target_x = 168 + (int)scene->launch.difficulty * 8;
        uint32_t shot_cadence = 60U -
            (uint32_t)scene->launch.difficulty * 15U;
        bool advance = scene->launch.difficulty != 0U ||
                       (scene->frame & 1U) == 0U;
        /* Native deterministic AI policy; positions/cadence are explicitly
           implementation-owned rather than claimed ROM behavior. */
        if (advance && holder->x < target_x) ++holder->x;
        if (advance && holder->y < 148) ++holder->y;
        if (advance && holder->y > 148) --holder->y;
        holder->facing_right = true;
        scene_clamp_actor(holder);
        scene_attach_ball(scene);
        if (holder->x >= target_x &&
            scene->frame % shot_cadence == 0U) {
            if (!scene_start_shot_actor(scene, 0U,
                                        scene->ball_holder)) {
                return false;
            }
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

static void scene_process_events(TecmoGameplayScene *scene)
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
            if (event->detail == 0U) {
                (void)tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE);
            }
            break;
        case TECMO_GAMEPLAY_EVENT_FREE_THROW_RESULT:
            if (event->value != 0U) {
                (void)tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE);
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
}

static void scene_process_phase_audio(TecmoGameplayScene *scene,
                                      TecmoGameplayPhase before)
{
    TecmoGameplayPhase after = scene->state.phase;
    if (before == after) return;
    if (after == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        (void)tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE);
    } else if (after == TECMO_GAMEPLAY_PHASE_LIVE &&
               scene->launch.game_music_enabled) {
        /* The neutral Bank05 $9FEC cue belongs only to the gated reset/restart
           boundary, never to foul-presentation entry. This function runs once
           per scene frame, so one boundary can queue it at most once. */
        if (before == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            before == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
            before == TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
            before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED ||
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
    /* Deterministic native free-throw timing/outcome policy. The 60-frame
       fallback and serial result are implementation-owned approximations; the
       strict state layer receives only the explicit made/missed result. */
    bool release = scene_controls_pressed_b(player_one) ||
                   scene_controls_pressed_b(player_two);
    ++scene->free_throw_frame;
    if (!release && scene->free_throw_frame <
                        TECMO_GAMEPLAY_FREE_THROW_FRAMES) {
        return true;
    }
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
    size_t controller;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        uint8_t team = scene->launch.controller_team[controller];
        uint8_t actor = scene->controlled_actor[controller];
        if (team == TECMO_GAMEPLAY_SCENE_NO_TEAM) {
            if (actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR) return false;
        } else if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                   scene->actors[actor].team != team) {
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

bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two)
{
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext live_context;
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayPhase phase_before;
    uint8_t moving_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    int16_t moving_holder_x = 0;
    int16_t moving_holder_y = 0;
    bool restart_applied;
    bool restart_frame;
    size_t controller;

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene->active || scene->result_ready ||
        !scene_ownership_valid(scene)) {
        return false;
    }
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
            if (!scene_update_shot(scene)) {
                scene_set_status(scene, "shot animation update rejected");
                return false;
            }
        } else if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE) {
            moving_holder = scene->ball_holder;
            if (moving_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
                moving_holder_x = scene->actors[moving_holder].x;
                moving_holder_y = scene->actors[moving_holder].y;
            }
            for (controller = 0U;
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
                 ++controller) {
                scene_move_controlled_actor(scene, controller,
                                            controls[controller]);
            }
            for (controller = 0U;
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
                 ++controller) {
                if (scene_controls_pressed_a(controls[controller])) {
                    scene_pass_or_switch(scene, controller);
                }
            }
            for (controller = 0U;
                 controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
                 ++controller) {
                if (scene_controls_pressed_b(controls[controller]) &&
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
            if (!scene_update_ai(scene)) {
                scene_set_status(scene, "native offense update rejected");
                return false;
            }
            if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
                scene_attach_ball(scene);
                if (scene->ball_holder == moving_holder &&
                    moving_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
                    (scene->actors[moving_holder].x != moving_holder_x ||
                     scene->actors[moving_holder].y != moving_holder_y) &&
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
        TecmoGameplayTeam free_throw_team =
            scene->state.free_throws.scoring_team;
        if (!tecmo_gameplay_settle_foul_presentation(
                &scene->state, free_throw_team,
                TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50)) {
            scene_set_status(scene, "foul settlement rejected");
            return false;
        }
        scene->free_throw_frame = 0U;
        if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
            !scene_handoff_possession(
                scene, scene->state.possession,
                scene_first_actor_for_team(scene->state.possession))) {
            scene_set_status(scene, "foul restart synchronization rejected");
            return false;
        }
    }
    if (phase_before == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE &&
        !scene_update_free_throw(scene, player_one, player_two)) {
        scene_set_status(scene, "free-throw settlement rejected");
        return false;
    }
    scene_process_events(scene);
    /* Restart-boundary audio is applied last so an event emitted during the
       settling action cannot overwrite the one-shot Bank05 $9FEC mailbox. */
    scene_process_phase_audio(scene, phase_before);
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
    uint8_t selector;
    if (scene->launch.home_team >= TECMO_GAMEPLAY_TEAM_LIMIT) return false;
    selector = (uint8_t)(0x40U + scene->launch.home_team);
    return tecmo_gameplay_assets_build_live_background_context(
        &scene->assets, selector, context);
}

static bool scene_background_tile(const TecmoGameplayScene *scene,
                                  const TecmoGameplayLiveBackgroundContext *context,
                                  unsigned row, unsigned column,
                                  uint32_t *chr_offset,
                                  uint8_t *palette_index)
{
    const uint8_t *nametable = scene->court.nametable;
    uint8_t tile_id;
    uint8_t attribute;
    unsigned shift;
    uint8_t band;
    uint8_t pre_asl;
    uint8_t mmc3_bank;
    uint32_t offset;
    if (nametable == NULL || context == NULL || chr_offset == NULL ||
        palette_index == NULL || row >= 30U || column >= 32U) {
        return false;
    }
    tile_id = nametable[row * 32U + column];
    attribute = nametable[0x3C0U + (row / 4U) * 8U + column / 4U];
    shift = ((row & 2U) != 0U ? 4U : 0U) +
            ((column & 2U) != 0U ? 2U : 0U);
    *palette_index = (uint8_t)((attribute >> shift) & 3U);
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

static bool scene_resolve_pose(const TecmoGameplayScene *scene,
                               uint16_t pointer_index,
                               uint8_t actor_slot_base,
                               uint8_t team,
                               TecmoGameplayResolvedPose *pose)
{
    TecmoGameplayPoseContext context;
    TecmoGameplayResolvedPose first;
    memset(&context, 0, sizeof(context));
    context.actor_slot_base = actor_slot_base;
    context.actor_attributes = 0U;
    context.palette_group = team;
    context.mmc3_r2_r5[0] = 0x40U;
    context.mmc3_r2_r5[1] = 0x41U;
    context.mmc3_r2_r5[2] = 0x42U;
    context.mmc3_r2_r5[3] = 0x43U;
    if (!tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                            &context, &first)) {
        return false;
    }
    context.mmc3_r2_r5[1] = first.record_tag;
    return tecmo_gameplay_assets_resolve_pose(&scene->assets, pointer_index,
                                              &context, pose);
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

bool tecmo_gameplay_scene_draw(const TecmoGameplayScene *scene,
                               TecmoFramebuffer *framebuffer,
                               int origin_x,
                               int origin_y,
                               int scale,
                               bool include_actors)
{
    TecmoGameplayLiveBackgroundContext background_context;
    TecmoGameplayResolvedPose actor_poses[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayResolvedPose ball_pose;
    uint8_t order[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    bool ball_visible;
    unsigned row;
    unsigned column;
    size_t actor;
    size_t left;

    if (scene == NULL ||
        scene->lifecycle_tag != TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG ||
        !scene->available || !scene_framebuffer_valid(framebuffer, origin_x,
                                                      origin_y, scale) ||
        !scene_build_background_context(scene, &background_context)) {
        return false;
    }
    for (row = 0U; row < 30U; ++row) {
        for (column = 0U; column < 32U; ++column) {
            uint32_t offset;
            uint8_t palette_index;
            if (!scene_background_tile(scene, &background_context, row,
                                       column, &offset, &palette_index) ||
                palette_index > 3U || offset + 16U >
                    scene->assets.chr_storage_size) {
                return false;
            }
        }
    }
    ball_visible = include_actors && scene->active;
    if (include_actors && scene->active) {
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            order[actor] = (uint8_t)actor;
            if (!scene_resolve_pose(scene, scene->actors[actor].pose_index,
                                    0x41U, scene->actors[actor].team,
                                    &actor_poses[actor])) {
                return false;
            }
        }
        if (!scene_resolve_pose(scene, TECMO_GAMEPLAY_BALL_POSE, 0xC1U,
                                0U, &ball_pose)) {
            return false;
        }
    }

    scene_fill_rect(framebuffer, origin_x, origin_y,
                    TECMO_GAMEPLAY_SCENE_NES_WIDTH * scale,
                    TECMO_GAMEPLAY_SCENE_NES_HEIGHT * scale,
                    tecmo_nes_2c02_rgba(scene->court.palette[0]));
    for (row = 0U; row < 30U; ++row) {
        for (column = 0U; column < 32U; ++column) {
            uint32_t offset;
            uint8_t palette_index;
            uint32_t palette[4];
            size_t color;
            (void)scene_background_tile(scene, &background_context, row,
                                        column, &offset, &palette_index);
            palette[0] = tecmo_nes_2c02_rgba(scene->court.palette[0]);
            for (color = 1U; color < 4U; ++color) {
                palette[color] = tecmo_nes_2c02_rgba(
                    scene->court.palette[(size_t)palette_index * 4U + color]);
            }
            tecmo_draw_chr_tile_at_offset_ex(
                framebuffer, scene->assets.chr_storage,
                scene->assets.chr_storage_size, offset,
                origin_x + (int)column * 8 * scale,
                origin_y + (int)row * 8 * scale,
                scale, palette, false, false);
        }
    }
    if (!include_actors || !scene->active) return true;

    for (left = 0U; left < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++left) {
        size_t right;
        for (right = left + 1U; right < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
             ++right) {
            if (scene->actors[order[right]].y <
                scene->actors[order[left]].y) {
                uint8_t swap = order[left];
                order[left] = order[right];
                order[right] = swap;
            }
        }
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        uint8_t index = order[actor];
        /* Actor-facing horizontal mirroring is an implementation-owned scene
           approximation. Live close shots still resolve only the explicitly
           supported TGCS profile-0/direction-0 slice. */
        scene_draw_pose(scene, framebuffer, &actor_poses[index],
                        scene->actors[index].x, scene->actors[index].y,
                        origin_x, origin_y, scale,
                        !scene->actors[index].facing_right);
    }
    if (ball_visible) {
        scene_draw_pose(scene, framebuffer, &ball_pose,
                        (int)(scene->ball_x_q8 / 256),
                        (int)(scene->ball_y_q8 / 256),
                        origin_x, origin_y, scale, false);
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

static bool scene_test_close_clock_collision(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneLaunch *launch)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t shot_actor;
    if (!tecmo_gameplay_scene_launch(scene, launch)) return false;
    scene->actors[scene->ball_holder].x = 210;
    scene->actors[scene->ball_holder].y = 148;
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
    scene->actors[0].x = scene->actors[5].x + 1;
    scene->actors[0].y = scene->actors[5].y;
    scene_attach_ball(scene);
    scene->action_serial = action_serial;
    holder_x = scene->actors[5].x;
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.released.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder != 5U || scene->controlled_actor[1] != 5U ||
        scene->actors[5].x != holder_x ||
        scene->action_serial != action_serial ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        return false;
    }
    tecmo_gameplay_scene_end(scene);
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
    TecmoGameplaySceneLaunch launch;
    TecmoGameplaySceneResult result;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoFramebuffer framebuffer;
    TecmoFramebuffer invalid_framebuffer;
    uint32_t *pixels;
    uint32_t render_hash;
    uint32_t close_transition_serial;
    uint16_t original_pose;
    uint16_t expected_pose;
    uint8_t holder;
    uint8_t shot_actor;
    int16_t x;
    bool policy_outcome;
    size_t frame;
    size_t pixel;
    const size_t pixel_count =
        (size_t)TECMO_GAMEPLAY_SCENE_NES_WIDTH *
        TECMO_GAMEPLAY_SCENE_NES_HEIGHT;

    tecmo_gameplay_scene_init(&missing_scene);
    if (tecmo_gameplay_scene_load(&missing_scene, project_root,
                                  "?:\\missing-gameplay.assetpack",
                                  music_player) || missing_scene.available) {
        scene_test_message(message, message_size,
                           "missing gameplay pack was accepted");
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
                           "gameplay scene canonical launch rejected");
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
    x = scene.actors[scene.controlled_actor[0]].x;
    p1.held.confirm = true;
    p1.pressed.confirm = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].x != x ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        scene_test_message(message, message_size,
                           "START changed live gameplay state");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].x != x + 1) {
        scene_test_message(message, message_size,
                           "directional movement contract failed");
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
    scene.actors[scene.ball_holder].x = 210;
    scene.actors[scene.ball_holder].y = 148;
    scene.actors[scene.ball_holder].facing_right = true;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
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
    for (frame = 0U; frame < 40U &&
         scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p1, 0, sizeof(p1));
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
                               "dunk/TGCS variant-0 replay failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.actors[scene.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.controlled_actor[1] != scene.ball_holder ||
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "dunk/TGCS variant-0 settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    scene.actors[scene.ball_holder].x = 190;
    scene.actors[scene.ball_holder].y = 148;
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
        !scene_test_close_semantic_chain_untouched(&scene) ||
        scene_test_has_close_semantic_event(&scene.events) ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "layup/TGCS variant-2 settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }

    scene.actors[scene.ball_holder].x = 96;
    scene.actors[scene.ball_holder].y = 180;
    scene.actors[scene.ball_holder].facing_right = true;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        scene_test_message(message, message_size,
                           "NES B jump-shot contract failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    policy_outcome = scene_shot_will_score(&scene);
    if (scene_shot_will_score(&scene) != policy_outcome) {
        scene_test_message(message, message_size,
                           "native deterministic shot policy diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < 48U &&
         scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p1, 0, sizeof(p1));
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "jump-shot replay update failed");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "jump-shot settlement failed");
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
    if (!tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 6U) {
        scene_test_message(message, message_size,
                           "shot-clock violation entry failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "violation lead-in update failed");
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
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "violation restart holder synchronization failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 5U) {
        scene_test_message(message, message_size,
                           "violation restart cue consumption failed");
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
    for (frame = 0U; frame < 120U &&
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
    scene.actors[scene.ball_holder].x = 96;
    scene.actors[scene.ball_holder].y = 180;
    scene.actors[scene.ball_holder].facing_right = true;
    scene_attach_ball(&scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_state_valid(&scene.state) ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        scene_test_message(message, message_size,
                           "period-expiry live shot setup failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U; frame < 48U &&
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
        scene.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene.actors[scene.ball_holder].team != scene.state.possession ||
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
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "period-expiry transition failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES - 1U;
    scene.ball_holder = 5U;
    scene_attach_ball(&scene);
    scene.action_serial = 3U;
    x = scene.actors[0].x;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.right = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene.ball_holder != 0U || scene.controlled_actor[0] != 0U ||
        scene.actors[0].x != x || scene.action_serial != 3U ||
        scene.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 5U) {
        scene_test_message(message, message_size,
                           "period restart action suppression failed");
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
    x = scene.actors[scene.controlled_actor[0]].x;
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.actors[scene.controlled_actor[0]].x != x + 1) {
        scene_test_message(message, message_size,
                           "swapped controller movement failed");
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
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene.shot_actor != 1U) {
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
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "human holder displacement missed holder DMC");
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
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        !scene.audio_player.dmc.active) {
        scene_test_message(message, message_size,
                           "CPU holder displacement missed holder DMC");
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
    scene.actors[scene.controlled_actor[1]].x =
        scene.actors[scene.ball_holder].x + 1;
    scene.actors[scene.controlled_actor[1]].y =
        scene.actors[scene.ball_holder].y;
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

    if (!tecmo_gameplay_scene_launch(&scene, &launch)) {
        scene_test_message(message, message_size,
                           "foul/free-throw gameplay launch rejected");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    scene.actors[scene.controlled_actor[1]].x =
        scene.actors[scene.ball_holder].x + 1;
    scene.actors[scene.controlled_actor[1]].y =
        scene.actors[scene.ball_holder].y;
    scene.action_serial = 3U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene.state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 1U ||
        scene.state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME][0] != 1U ||
        scene.action_serial != 4U ||
        scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "native action-serial foul policy diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    for (frame = 0U; frame < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2)) {
            scene_test_message(message, message_size,
                               "foul presentation lead-in failed");
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
        scene.free_throw_frame != 0U || scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "foul dismissal/free-throw handoff failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 0U; frame + 1U < TECMO_GAMEPLAY_FREE_THROW_FRAMES;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
            scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            scene.state.free_throws.attempts_remaining != 2U ||
            scene.free_throw_frame != frame + 1U ||
            scene.action_serial != 4U) {
            scene_test_message(message, message_size,
                               "native free-throw fallback timing diverged");
            tecmo_gameplay_scene_destroy(&scene);
            return false;
        }
    }
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        scene.state.free_throws.attempts_remaining != 1U ||
        scene.free_throw_frame != 0U || scene.action_serial != 5U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 1U) {
        scene_test_message(message, message_size,
                           "native free-throw serial outcome diverged");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene.state.free_throws.attempts_remaining != 0U ||
        scene.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene.action_serial != 6U ||
        scene.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene.ball_holder < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        scene.controlled_actor[1] != scene.ball_holder ||
        !scene.audio_player.sfx_pending ||
        scene.audio_player.pending_sfx_id != 5U ||
        !tecmo_gameplay_state_valid(&scene.state)) {
        scene_test_message(message, message_size,
                           "free-throw settlement failed");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&scene.audio_player, NULL, 1024U);
    memset(&p1, 0, sizeof(p1));
    if (scene.audio_player.sfx_pending ||
        scene.audio_player.current_sfx_id != 5U ||
        !tecmo_gameplay_scene_update(&scene, &p1, &p2) ||
        scene.audio_player.sfx_pending) {
        scene_test_message(message, message_size,
                           "foul restart cue repeated or missing");
        tecmo_gameplay_scene_destroy(&scene);
        return false;
    }
    tecmo_gameplay_scene_end(&scene);

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
    scene_test_message(message, message_size,
                       "GAMEPLAY SCENE SELF TEST PASS");
    return true;
}
