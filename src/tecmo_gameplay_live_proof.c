#include "tecmo_gameplay_live_proof.h"

#include "png_writer.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_scene_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIVE_PROOF_WIDTH 640
#define LIVE_PROOF_HEIGHT 480
#define LIVE_PROOF_MEMORY_SIZE (16U * 1024U * 1024U)
#define LIVE_PROOF_MAX_PRETIP_UPDATES 2048U

static void live_proof_error(char *message, size_t message_size,
                             const char *text)
{
    if (message == NULL || message_size == 0U) return;
    (void)snprintf(message, message_size, "%s", text != NULL ? text :
                   "LIVE proof rejected");
}

static bool live_proof_reject(char *message, size_t message_size,
                              const char *text)
{
    live_proof_error(message, message_size, text);
    return false;
}

static bool live_proof_append(char *buffer, size_t capacity, size_t *length,
                              const char *format, ...)
{
    va_list args;
    int written;
    if (buffer == NULL || length == NULL || format == NULL ||
        *length >= capacity) {
        return false;
    }
    va_start(args, format);
    written = vsnprintf(buffer + *length, capacity - *length, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

static uint32_t live_proof_fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    if (bytes == NULL) return 0U;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static bool live_proof_event_valid(const char *event)
{
    static const char *const names[] = {
        "pretip-start",
        "live-handoff",
        "human-movement",
        "offensive-pass",
        "defensive-switch",
        "cpu-target-deferred",
        "shot-path"
    };
    size_t index;
    if (event == NULL || event[0] == '\0') return false;
    for (index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (strcmp(event, names[index]) == 0) return true;
    }
    return false;
}

static void live_proof_launch_init(TecmoGameplaySceneLaunch *launch)
{
    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->game_index = 0U;
    launch->away_team = 0U;
    launch->home_team = 1U;
    launch->regulation_minutes = 4U;
    launch->difficulty = 0U;
    launch->control_mode = 0U;
    launch->speed_value = 0U;
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->game_music_enabled = false;
    launch->starter_binding_bound = true;
    /* Deliberately exercises 5/6/10/11 and a non-identity home permutation. */
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 5U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][1U] = 6U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][2U] = 10U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][3U] = 11U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][4U] = 0U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][0U] = 11U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][1U] = 10U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][2U] = 6U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][3U] = 5U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][4U] = 1U;
}

static void live_proof_controls_neutral(TecmoControlFrame *frame)
{
    if (frame == NULL) return;
    memset(frame, 0, sizeof(*frame));
}

static bool live_proof_advance_pretip(TecmoGameplayScene *scene)
{
    TecmoControlFrame away;
    TecmoControlFrame neutral;
    uint8_t saved_home_controller_team;
    bool home_automatic_fixture;
    size_t update;
    live_proof_controls_neutral(&away);
    live_proof_controls_neutral(&neutral);
    saved_home_controller_team = scene->launch.controller_team[1U];
    home_automatic_fixture = saved_home_controller_team ==
        TECMO_GAMEPLAY_TEAM_HOME;
    if (home_automatic_fixture) {
        scene->launch.controller_team[1U] =
            TECMO_GAMEPLAY_SCENE_NO_TEAM;
    }
    for (update = 0U; update < LIVE_PROOF_MAX_PRETIP_UPDATES; ++update) {
        if (!tecmo_gameplay_scene_in_pretip(scene)) {
            if (home_automatic_fixture) {
                scene->launch.controller_team[1U] =
                    saved_home_controller_team;
            }
            return true;
        }
        live_proof_controls_neutral(&away);
        if (scene->pretip_state.phase ==
            TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP) {
            away.held.cancel = true;
        }
        if (!tecmo_gameplay_scene_update(scene, &away, &neutral)) {
            if (home_automatic_fixture) {
                scene->launch.controller_team[1U] =
                    saved_home_controller_team;
            }
            return false;
        }
        if (home_automatic_fixture &&
            scene->pretip_state.claim_resolved) {
            scene->launch.controller_team[1U] =
                saved_home_controller_team;
            home_automatic_fixture = false;
        }
    }
    if (home_automatic_fixture) {
        scene->launch.controller_team[1U] = saved_home_controller_team;
    }
    return !tecmo_gameplay_scene_in_pretip(scene);
}

static bool live_proof_force_possession(TecmoGameplayScene *scene,
                                        TecmoGameplayTeam team,
                                        uint8_t holder)
{
    if (!scene_handoff_possession(scene, team, holder) ||
        !scene_sync_live_foundation(scene)) {
        return false;
    }
    return scene_ownership_valid(scene);
}

static bool live_proof_live_ownership(const TecmoGameplayScene *scene,
                                      char *message, size_t message_size)
{
    if (scene == NULL || tecmo_gameplay_scene_in_pretip(scene) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.first_sync_pending ||
        scene->live_foundation.primary_actor != scene->ball_holder ||
        scene->live_foundation.last_possession !=
            (uint8_t)scene->state.possession ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation) ||
        !scene_ownership_valid(scene)) {
        return live_proof_reject(
            message, message_size,
            "LIVE handoff did not produce synchronized LIVE ownership");
    }
    return true;
}

static bool live_proof_find_offset(
    const TecmoGameplayCpuSteeringAssets *assets,
    bool target_command,
    uint16_t *offset_out)
{
    uint16_t offset;
    if (assets == NULL || offset_out == NULL || !assets->available) return false;
    for (offset = 0U;
         offset < (uint16_t)(assets->command_record_count *
                             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE);
         offset = (uint16_t)(offset +
                             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        TecmoGameplayCpuSteeringCommand command;
        if (!tecmo_gameplay_cpu_steering_decode_command(
                assets, offset, &command)) {
            continue;
        }
        if (target_command && command.opcode == 4U) {
            *offset_out = offset;
            return true;
        }
        if (!target_command &&
            (command.opcode == 5U || command.opcode == 6U ||
             command.opcode == 8U || command.opcode == 10U ||
             command.opcode == 12U || command.opcode == 13U ||
             command.opcode == 15U || command.opcode == 16U ||
             command.opcode == 20U || command.opcode == 23U)) {
            *offset_out = offset;
            return true;
        }
    }
    return false;
}

static bool live_proof_prepare_cpu_fixture(TecmoGameplayScene *scene)
{
    TecmoGameplayLiveFoundation candidate;
    uint16_t target_offset;
    uint16_t deferred_offset;
    if (scene == NULL ||
        !live_proof_find_offset(&scene->cpu_steering_assets, true,
                                &target_offset) ||
        !live_proof_find_offset(&scene->cpu_steering_assets, false,
                                &deferred_offset) ||
        !live_proof_force_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return false;
    }
    /* Deterministic test fixture: CPU-only routing is injected after the
       bound production-style launch. It is not original or normal-policy
       evidence. */
    scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (!live_proof_force_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return false;
    }
    candidate = scene->live_foundation;
    candidate.play_state.stream_offset[1U] = target_offset;
    candidate.last_step_offset[1U] = target_offset;
    candidate.play_state.target_actor[1U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.play_state.target_x[1U] = 0;
    candidate.play_state.target_depth[1U] = 0;
    candidate.source_target_valid[1U] = false;
    candidate.play_state.stream_offset[2U] = deferred_offset;
    candidate.last_step_offset[2U] = deferred_offset;
    candidate.play_state.target_actor[2U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.play_state.target_x[2U] = 0;
    candidate.play_state.target_depth[2U] = 0;
    candidate.source_target_valid[2U] = false;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate)) {
        return false;
    }
    scene->live_foundation = candidate;
    return true;
}

static bool live_proof_apply_event(TecmoGameplayScene *scene,
                                   const char *event,
                                   char *message,
                                   size_t message_size)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    if (scene == NULL || event == NULL) {
        return live_proof_reject(message, message_size,
                                 "LIVE proof event context missing");
    }
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    if (strcmp(event, "pretip-start") == 0) {
        if (!tecmo_gameplay_scene_in_pretip(scene) ||
            !tecmo_gameplay_pretip_is_presentation(&scene->pretip_state) ||
            scene->pretip_state.live_handoff ||
            !scene->live_foundation.first_sync_pending ||
            scene->live_foundation.sync_serial != 0U ||
            scene->live_foundation.last_ball_holder !=
                TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->live_foundation.last_possession !=
                (uint8_t)TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->live_foundation.primary_actor !=
                scene->live_foundation.static_primary_seed ||
            scene->live_foundation.defender_actor !=
                scene->live_foundation.static_defender_seed ||
            scene->live_foundation.static_primary_seed != 4U ||
            scene->live_foundation.static_defender_seed != 9U) {
            return live_proof_reject(
                message, message_size,
                "pretip-start was not an unsynchronized presentation state");
        }
        return true;
    }
    if (!live_proof_advance_pretip(scene)) {
        return live_proof_reject(message, message_size,
                                 "real PRETIP-to-LIVE advancement failed");
    }
    if (strcmp(event, "live-handoff") == 0) {
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "human-movement") == 0) {
        int16_t start_x;
        uint8_t roster_index;
        uint8_t condition;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "human movement fixture handoff failed");
        }
        start_x = scene->actors[scene->controlled_actor[0U]].position.x;
        roster_index = scene->actors[0U].roster_index;
        condition = scene->actors[0U].condition;
        p1.held.right = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[0U].position.x != start_x ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[0U].position.x != (int16_t)(start_x + 1) ||
            scene->actors[0U].roster_index != roster_index ||
            scene->actors[0U].condition != condition) {
            return live_proof_reject(
                message, message_size,
                "human movement did not prove one-update TGMO latency");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "offensive-pass") == 0) {
        uint8_t expected_roster;
        uint8_t expected_condition;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "offensive pass fixture handoff failed");
        }
        expected_roster = scene->launch.starter_roster_index
            [TECMO_GAMEPLAY_TEAM_AWAY][1U];
        expected_condition = scene->fatigue_state.condition
            [TECMO_GAMEPLAY_TEAM_AWAY][expected_roster];
        p1.held.shoot = true;
        p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->ball_holder != 1U || scene->controlled_actor[0U] != 1U ||
            scene->actors[1U].team != TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->actors[1U].roster_index != expected_roster ||
            scene->actors[1U].condition != expected_condition) {
            return live_proof_reject(
                message, message_size,
                "offensive pass did not preserve bound receiver identity");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "defensive-switch") == 0) {
        uint8_t expected_actor;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
            return live_proof_reject(message, message_size,
                                     "defensive switch fixture handoff failed");
        }
        expected_actor = scene_nearest_actor_for_team(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, scene->ball_holder);
        p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != expected_actor ||
            scene->controlled_actor[1U] != 5U ||
            expected_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[expected_actor].team != TECMO_GAMEPLAY_TEAM_AWAY ||
            expected_actor == scene->ball_holder) {
            return live_proof_reject(
                message, message_size,
                "defensive A did not select the nearest same-team nonholder");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "cpu-target-deferred") == 0) {
        size_t target_count = 0U;
        size_t deferred_count = 0U;
        size_t actor;
        if (!live_proof_prepare_cpu_fixture(scene) ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            return live_proof_reject(message, message_size,
                                     "CPU target/deferred fixture update failed");
        }
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            if (scene->live_foundation.source_target_valid[actor]) {
                ++target_count;
            }
            if (scene->live_foundation.deferred[actor]) ++deferred_count;
        }
        if (target_count == 0U || deferred_count == 0U) {
            return live_proof_reject(
                message, message_size,
                "CPU event did not expose both source target and deferred actors");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "shot-path") == 0) {
        TecmoGameplayCourtCoordinate close_position;
        TecmoGameplaySceneCpuShotRequest shot_request;
        TecmoControlFrame neutral;
        uint16_t action_before;
        scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        /* Deterministic close-position fixture for the supported shots.c
           profile; this is not original source evidence or normal policy. */
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "shot fixture handoff failed");
        }
        close_position.x = (int16_t)(
            scene->orientation_state.offensive_hoop.x + 8);
        close_position.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
        if (!scene_actor_coordinate_valid(&close_position)) {
            return live_proof_reject(
                message, message_size,
                "shot close-position fixture was outside TGCT bounds");
        }
        scene->actors[0U].position = close_position;
        scene->actors[0U].anchor = close_position;
        scene->state.shot_clock = 12U;
        scene->state.clock_divider = 1U;
        if (!scene_attach_ball(scene)) return false;
        action_before = scene->action_serial;
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) ||
            !shot_request.requested || !shot_request.playback_supported ||
            shot_request.deferred ||
            scene->action_serial != (uint16_t)(action_before + 1U) ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != 0U) {
            return live_proof_reject(message, message_size,
                                     "shot fixture did not launch exactly once");
        }
        /* Advance the already-started close shot through the production
           outer update. This is playback only: AI is not re-entered, the
           excluded shot path is not called a second time, and action_serial
           must remain at the single launch increment. */
        live_proof_controls_neutral(&neutral);
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral) ||
            scene->action_serial != (uint16_t)(action_before + 1U) ||
            scene->shot_frame == 0U ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != 0U) {
            return live_proof_reject(
                message, message_size,
                "shot fixture visible playback advance rejected");
        }
        return true;
    }
    return live_proof_reject(message, message_size,
                             "LIVE proof event was not implemented");
}

static bool live_proof_render(const TecmoRuntime *runtime,
                              const char *output_png_path,
                              uint32_t *frame_hash_out)
{
    TecmoFramebuffer framebuffer;
    uint32_t *pixels;
    uint8_t *rgba;
    size_t pixel_count;
    size_t index;
    if (runtime == NULL || output_png_path == NULL ||
        output_png_path[0] == '\0' || frame_hash_out == NULL) return false;
    pixel_count = (size_t)LIVE_PROOF_WIDTH * (size_t)LIVE_PROOF_HEIGHT;
    pixels = (uint32_t *)calloc(pixel_count, sizeof(*pixels));
    rgba = (uint8_t *)malloc(pixel_count * 4U);
    if (pixels == NULL || rgba == NULL) {
        free(pixels);
        free(rgba);
        return false;
    }
    framebuffer.pixels = pixels;
    framebuffer.width = LIVE_PROOF_WIDTH;
    framebuffer.height = LIVE_PROOF_HEIGHT;
    framebuffer.pitch_pixels = LIVE_PROOF_WIDTH;
    tecmo_runtime_render(runtime, &framebuffer);
    for (index = 0U; index < pixel_count; ++index) {
        rgba[index * 4U + 0U] = (uint8_t)((pixels[index] >> 16U) & 0xFFU);
        rgba[index * 4U + 1U] = (uint8_t)((pixels[index] >> 8U) & 0xFFU);
        rgba[index * 4U + 2U] = (uint8_t)(pixels[index] & 0xFFU);
        rgba[index * 4U + 3U] = (uint8_t)((pixels[index] >> 24U) & 0xFFU);
    }
    *frame_hash_out = live_proof_fnv1a32(rgba, pixel_count * 4U);
    if (png_write_rgba8(output_png_path, rgba,
                        LIVE_PROOF_WIDTH, LIVE_PROOF_HEIGHT) != 0) {
        free(pixels);
        free(rgba);
        return false;
    }
    free(pixels);
    free(rgba);
    return true;
}

static bool live_proof_json(const TecmoGameplayScene *scene,
                            const char *event,
                            const char *output_png_path,
                            uint32_t frame_hash,
                            char *message, size_t message_size)
{
    size_t length = 0U;
    size_t actor;
    size_t target_count = 0U;
    size_t deferred_count = 0U;
    if (scene == NULL || event == NULL || output_png_path == NULL ||
        message == NULL || message_size == 0U) return false;
    (void)output_png_path;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (scene->live_foundation.source_target_valid[actor]) ++target_count;
        if (scene->live_foundation.deferred[actor]) ++deferred_count;
    }
    message[0] = '\0';
    if (!live_proof_append(
            message, message_size, &length,
            "{\"schema\":\"tecmo.live-proof/TGLP-1\","
            "\"event\":\"%s\","
            "\"launch\":\"direct-bound-nonidentity-scene-launch\","
             "\"render_path\":\"TecmoRuntime/TECMO_MODE_COURT->tecmo_runtime_render\","
             "\"resolution\":[640,480],\"pretip_skip_hook\":false,"
             "\"pretip\":{\"in_presentation\":%s,"
             "\"is_presentation\":%s,\"phase\":%u,"
             "\"live_handoff\":%s,\"first_sync_pending\":%s,"
             "\"synchronized\":%s},"
             "\"frame\":%u,\"phase\":%u,\"possession\":%u,"
            "\"ball_holder\":%u,\"orientation\":%u,"
            "\"controlled\":[%u,%u],\"action_serial\":%u,"
            "\"shot_kind\":%u,\"shot_actor\":%u,\"shot_frame\":%u,"
            "\"frame_fingerprint_fnv1a32\":\"%08X\","
            "\"starter_roster_index\":{"
            "\"away\":[%u,%u,%u,%u,%u],"
            "\"home\":[%u,%u,%u,%u,%u]},\"live\":{"
            "\"valid\":%s,\"formation_index\":%u,"
            "\"primary_actor\":%u,\"defender_actor\":%u,"
            "\"orientation\":%u,\"sync_serial\":%u,"
            "\"tick_serial\":%u,\"first_sync_pending\":%s,"
            "\"last_shot_request\":%s,\"last_shot_deferred\":%s,"
            "\"last_shot_playback_supported\":%s,"
            "\"last_shot_actor\":%u,\"source_target_count\":%u,"
            "\"deferred_count\":%u},\"actors\":[",
              event,
             tecmo_gameplay_scene_in_pretip(scene) ? "true" : "false",
             tecmo_gameplay_pretip_is_presentation(
                 &scene->pretip_state) ? "true" : "false",
             (unsigned)scene->pretip_state.phase,
             scene->pretip_state.live_handoff ? "true" : "false",
             scene->live_foundation.first_sync_pending ? "true" : "false",
             (!scene->live_foundation.first_sync_pending &&
              scene->live_foundation.sync_serial != 0U) ? "true" : "false",
             (unsigned)scene->frame, (unsigned)scene->state.phase,
            (unsigned)scene->state.possession, (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.current_direction,
            (unsigned)scene->controlled_actor[0U],
            (unsigned)scene->controlled_actor[1U],
            (unsigned)scene->action_serial, (unsigned)scene->shot_kind,
            (unsigned)scene->shot_actor, (unsigned)scene->shot_frame,
            (unsigned)frame_hash,
            (unsigned)scene->launch.starter_roster_index[0U][0U],
            (unsigned)scene->launch.starter_roster_index[0U][1U],
            (unsigned)scene->launch.starter_roster_index[0U][2U],
            (unsigned)scene->launch.starter_roster_index[0U][3U],
            (unsigned)scene->launch.starter_roster_index[0U][4U],
            (unsigned)scene->launch.starter_roster_index[1U][0U],
            (unsigned)scene->launch.starter_roster_index[1U][1U],
            (unsigned)scene->launch.starter_roster_index[1U][2U],
            (unsigned)scene->launch.starter_roster_index[1U][3U],
            (unsigned)scene->launch.starter_roster_index[1U][4U],
            tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)
                ? "true" : "false",
            (unsigned)scene->live_foundation.formation_index,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation.orientation,
            (unsigned)scene->live_foundation.sync_serial,
            (unsigned)scene->live_foundation.tick_serial,
            scene->live_foundation.first_sync_pending ? "true" : "false",
            scene->live_foundation.last_shot_request ? "true" : "false",
            scene->live_foundation.last_shot_deferred ? "true" : "false",
            scene->live_foundation.last_shot_playback_supported ? "true" :
                                                                  "false",
            (unsigned)scene->live_foundation.last_shot_actor,
            (unsigned)target_count, (unsigned)deferred_count)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        const TecmoGameplaySceneCpuActor *cpu = &scene->cpu_actors[actor];
        if (!live_proof_append(
                message, message_size, &length,
                "%s{\"slot\":%u,\"team\":%u,\"roster\":%u,"
                "\"x\":%d,\"y\":%d,\"link\":%u,\"cpu\":{"
                "\"target_valid\":%s,\"kind\":%u,\"x\":%d,"
                "\"y\":%d,\"direction\":%u},"
                "\"source_target_valid\":%s,\"source_target_actor\":%u,"
                "\"source_target_x\":%d,\"source_target_y\":%d,"
                "\"source_direction_valid\":%s,\"source_direction\":%u,"
                "\"deferred\":%s}",
                actor == 0U ? "" : ",", (unsigned)actor,
                (unsigned)item->team, (unsigned)item->roster_index,
                (int)item->position.x, (int)item->position.y,
                (unsigned)cpu->linked_actor,
                cpu->target_valid ? "true" : "false",
                (unsigned)cpu->target_kind,
                (int)cpu->target_position.x, (int)cpu->target_position.y,
                (unsigned)cpu->direction,
                scene->live_foundation.source_target_valid[actor]
                    ? "true" : "false",
                (unsigned)scene->live_foundation.play_state
                    .target_actor[actor],
                (int)scene->live_foundation.play_state.target_x[actor],
                (int)scene->live_foundation.play_state.target_depth[actor],
                scene->live_foundation.source_direction_valid[actor]
                    ? "true" : "false",
                (unsigned)scene->live_foundation.source_direction[actor],
                scene->live_foundation.deferred[actor] ? "true" : "false")) {
            return false;
        }
    }
    return live_proof_append(message, message_size, &length, "]}");
}

bool tecmo_gameplay_live_foundation_proof(
    const char *project_root,
    const char *asset_pack_path,
    const char *event,
    const char *output_png_path,
    char *message,
    size_t message_size)
{
    TecmoRuntime runtime;
    TecmoGameMemory memory;
    TecmoGameplaySceneLaunch launch;
    void *permanent_block;
    void *transient_block;
    uint32_t frame_hash;
    bool ok = false;
    if (message != NULL && message_size != 0U) message[0] = '\0';
    if (project_root == NULL || asset_pack_path == NULL || event == NULL ||
        output_png_path == NULL || !live_proof_event_valid(event)) {
        live_proof_error(message, message_size,
                         "missing or unsupported LIVE proof input");
        return false;
    }
    memset(&runtime, 0, sizeof(runtime));
    memset(&memory, 0, sizeof(memory));
    permanent_block = malloc(LIVE_PROOF_MEMORY_SIZE);
    transient_block = malloc(LIVE_PROOF_MEMORY_SIZE);
    if (permanent_block == NULL || transient_block == NULL) {
        live_proof_error(message, message_size,
                         "LIVE proof memory allocation failed");
        free(permanent_block);
        free(transient_block);
        return false;
    }
    tecmo_arena_init(&memory.permanent, permanent_block,
                     LIVE_PROOF_MEMORY_SIZE);
    tecmo_arena_init(&memory.transient, transient_block,
                     LIVE_PROOF_MEMORY_SIZE);
    runtime.memory = &memory;
    tecmo_gameplay_scene_init(&runtime.gameplay_scene);
    if (!tecmo_music_asset_load_from_pack(&runtime.music_asset,
                                          asset_pack_path)) {
        live_proof_error(message, message_size,
                         runtime.music_asset.status);
        goto cleanup;
    }
    tecmo_music_player_init(&runtime.music_player, &runtime.music_asset);
    if (!tecmo_gameplay_scene_load(&runtime.gameplay_scene, project_root,
                                   asset_pack_path, &runtime.music_player)) {
        live_proof_error(message, message_size,
                         runtime.gameplay_scene.status);
        goto cleanup;
    }
    live_proof_launch_init(&launch);
    if (!tecmo_gameplay_scene_launch(&runtime.gameplay_scene, &launch) ||
        !runtime.gameplay_scene.active) {
        live_proof_error(message, message_size,
                         runtime.gameplay_scene.status[0] != '\0'
                             ? runtime.gameplay_scene.status
                             : "bound LIVE proof launch rejected");
        goto cleanup;
    }
    if (!live_proof_apply_event(&runtime.gameplay_scene, event,
                                message, message_size)) {
        if (message == NULL || message[0] == '\0') {
            live_proof_error(message, message_size,
                             runtime.gameplay_scene.status[0] != '\0'
                                 ? runtime.gameplay_scene.status
                                 : "LIVE proof event rejected");
        }
        goto cleanup;
    }
    runtime.mode = TECMO_MODE_COURT;
    runtime.normal_play_active = true;
    runtime.debug_overlay = false;
    runtime.frame_counter = runtime.gameplay_scene.frame;
    runtime.mode_frame_counter = runtime.gameplay_scene.frame;
    runtime.frame_seconds = 1.0f / 60.0f;
    if (!live_proof_render(&runtime, output_png_path, &frame_hash) ||
        !live_proof_json(&runtime.gameplay_scene, event, output_png_path,
                         frame_hash, message, message_size)) {
        live_proof_error(message, message_size,
                         "LIVE proof render or JSON emission failed");
        goto cleanup;
    }
    ok = true;

cleanup:
    tecmo_gameplay_scene_destroy(&runtime.gameplay_scene);
    tecmo_music_asset_shutdown(&runtime.music_asset);
    free(permanent_block);
    free(transient_block);
    return ok;
}
