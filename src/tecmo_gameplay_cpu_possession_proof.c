#include "tecmo_gameplay_cpu_possession_proof.h"

#include "png_writer.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_scene_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POSSESSION_PROOF_WIDTH 640
#define POSSESSION_PROOF_HEIGHT 480
#define POSSESSION_PROOF_MEMORY_SIZE (16U * 1024U * 1024U)
#define POSSESSION_PROOF_PRETIP_LIMIT 2048U
#define POSSESSION_PROOF_INBOUND_LIMIT 128U
#define POSSESSION_PROOF_OUTER_LIMIT 1085U

typedef struct PossessionProofActorExtent {
    int pose_min_dx;
    int pose_max_dx;
    int pose_min_dy;
    int pose_max_dy;
    int world_min_x;
    int world_max_x;
    int world_min_y;
    int world_max_y;
    int left_edge;
    int right_edge;
    int left_overhang;
    int right_overhang;
    bool anchor_within;
    bool projected_visible;
    uint8_t screen_x;
    uint8_t screen_y;
    bool mirror;
} PossessionProofActorExtent;

static uint32_t proof_hash(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static bool proof_render(const TecmoRuntime *runtime, const char *path,
                         uint32_t *hash_out)
{
    TecmoFramebuffer framebuffer;
    uint32_t *pixels;
    uint8_t *rgba;
    size_t count = (size_t)POSSESSION_PROOF_WIDTH * POSSESSION_PROOF_HEIGHT;
    size_t index;
    bool ok = false;
    if (runtime == NULL || path == NULL || hash_out == NULL) return false;
    pixels = (uint32_t *)calloc(count, sizeof(*pixels));
    rgba = (uint8_t *)malloc(count * 4U);
    if (pixels == NULL || rgba == NULL) goto done;
    framebuffer.pixels = pixels;
    framebuffer.width = POSSESSION_PROOF_WIDTH;
    framebuffer.height = POSSESSION_PROOF_HEIGHT;
    framebuffer.pitch_pixels = POSSESSION_PROOF_WIDTH;
    tecmo_runtime_render(runtime, &framebuffer);
    for (index = 0U; index < count; ++index) {
        rgba[index * 4U] = (uint8_t)(pixels[index] >> 16U);
        rgba[index * 4U + 1U] = (uint8_t)(pixels[index] >> 8U);
        rgba[index * 4U + 2U] = (uint8_t)pixels[index];
        rgba[index * 4U + 3U] = (uint8_t)(pixels[index] >> 24U);
    }
    *hash_out = proof_hash(rgba, count * 4U);
    ok = png_write_rgba8(path, rgba, POSSESSION_PROOF_WIDTH,
                         POSSESSION_PROOF_HEIGHT) == 0;
done:
    free(pixels);
    free(rgba);
    return ok;
}

static void proof_launch_init(TecmoGameplaySceneLaunch *launch)
{
    static const uint8_t away[5] = {5U, 6U, 10U, 11U, 0U};
    static const uint8_t home[5] = {11U, 10U, 6U, 5U, 1U};
    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->away_team = 0U;
    launch->home_team = 1U;
    launch->regulation_minutes = 4U;
    launch->difficulty = 0U;
    launch->speed_value = 0U;
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch->game_music_enabled = false;
    launch->starter_binding_bound = true;
    memcpy(launch->starter_roster_index[0U], away, sizeof(away));
    memcpy(launch->starter_roster_index[1U], home, sizeof(home));
}

static bool proof_advance_pretip(TecmoGameplayScene *scene)
{
    TecmoControlFrame away;
    TecmoControlFrame neutral;
    size_t update;
    memset(&away, 0, sizeof(away));
    memset(&neutral, 0, sizeof(neutral));
    for (update = 0U; update < POSSESSION_PROOF_PRETIP_LIMIT &&
         tecmo_gameplay_scene_in_pretip(scene); ++update) {
        memset(&away, 0, sizeof(away));
        if (scene->pretip_state.phase ==
            TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP) {
            away.held.cancel = true;
        }
        if (!tecmo_gameplay_scene_update(scene, &away, &neutral)) {
            return false;
        }
    }
    return !tecmo_gameplay_scene_in_pretip(scene);
}

static bool proof_actor_extent(const TecmoGameplayScene *scene,
                               const TecmoGameplaySceneCourtFrame *court,
                               size_t actor,
                               PossessionProofActorExtent *extent)
{
    TecmoGameplayResolvedPose pose;
    const TecmoGameplaySceneActor *item;
    size_t piece;
    if (scene == NULL || court == NULL || extent == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_scene_render_resolve_actor_pose(scene, actor, &pose) ||
        !tecmo_gameplay_scene_render_actor_mirror(
            scene, actor, &extent->mirror) || pose.piece_count == 0U) {
        return false;
    }
    item = &scene->actors[actor];
    extent->pose_min_dx = 32767;
    extent->pose_max_dx = -32768;
    extent->pose_min_dy = 32767;
    extent->pose_max_dy = -32768;
    for (piece = 0U; piece < pose.piece_count; ++piece) {
        int dx = extent->mirror ? -pose.pieces[piece].dx - 8
                                : pose.pieces[piece].dx;
        int dy = pose.pieces[piece].dy;
        if (dx < extent->pose_min_dx) extent->pose_min_dx = dx;
        if (dx + 7 > extent->pose_max_dx) extent->pose_max_dx = dx + 7;
        if (dy < extent->pose_min_dy) extent->pose_min_dy = dy;
        if (dy + 15 > extent->pose_max_dy) extent->pose_max_dy = dy + 15;
    }
    extent->world_min_x = item->position.x + extent->pose_min_dx;
    extent->world_max_x = item->position.x + extent->pose_max_dx;
    extent->world_min_y = item->position.y + extent->pose_min_dy;
    extent->world_max_y = item->position.y + extent->pose_max_dy;
    extent->left_edge = TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE -
        item->position.y / 2;
    extent->right_edge = TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE +
        item->position.y / 2;
    extent->anchor_within = item->position.y >= TECMO_GAMEPLAY_MIN_Y &&
        item->position.y <= TECMO_GAMEPLAY_MAX_Y &&
        item->position.x >= extent->left_edge &&
        item->position.x <= extent->right_edge;
    extent->left_overhang = extent->world_min_x < extent->left_edge
        ? extent->left_edge - extent->world_min_x : 0;
    extent->right_overhang = extent->world_max_x > extent->right_edge
        ? extent->world_max_x - extent->right_edge : 0;
    extent->projected_visible = court->projection.players[actor].visible;
    extent->screen_x = court->projection.players[actor].screen_x;
    extent->screen_y = court->projection.players[actor].screen_y;
    return true;
}

static bool proof_write_frame(FILE *trace, const TecmoGameplayScene *scene,
                              unsigned update, int delta_x, int delta_y,
                              unsigned idle_frames, int *max_overhang_out,
                              bool *anchor_oob_out)
{
    TecmoGameplaySceneCourtFrame court;
    size_t actor;
    int frame_max = 0;
    uint8_t holder = scene->ball_holder;
    if (trace == NULL || scene == NULL || max_overhang_out == NULL ||
        anchor_oob_out == NULL ||
        !tecmo_gameplay_scene_court_frame(scene, &court)) return false;
    fprintf(trace,
            "{\"schema\":\"tecmo.cpu-possession-frame/TGPH-1\","
            "\"update\":%u,\"scene_frame\":%u,\"phase\":%u,"
            "\"violation\":%u,\"possession\":%u,\"holder\":%u,"
            "\"primary\":%u,\"defender\":%u,\"clock\":[%u,%u,%u],"
            "\"shot_clock\":%u,\"action_serial\":%u,"
            "\"shot\":[%u,%u,%u],\"pass\":[%u,%u,%u,%u],"
            "\"holder_delta\":[%d,%d],\"idle_frames\":%u",
            update, (unsigned)scene->frame, (unsigned)scene->state.phase,
            (unsigned)scene->state.violation,
            (unsigned)scene->state.possession, (unsigned)holder,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->state.clock_minutes,
            (unsigned)scene->state.clock_seconds,
            (unsigned)scene->state.clock_divider,
            (unsigned)scene->state.shot_clock,
            (unsigned)scene->action_serial, (unsigned)scene->shot_kind,
            (unsigned)scene->shot_actor, (unsigned)scene->shot_frame,
            (unsigned)scene->pass_state.phase,
            (unsigned)scene->pass_state.passer,
            (unsigned)scene->pass_state.receiver,
            (unsigned)scene->pass_state.flight_frame,
            delta_x, delta_y, idle_frames);
    if (holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        const TecmoGameplayCpuSteeringRouteMotionState *route =
            &scene->live_foundation.play_state.route_motion[holder];
        const TecmoGameplaySceneCpuActor *cpu = &scene->cpu_actors[holder];
        TecmoGameplayCpuSteeringCommand command;
        bool decoded = tecmo_gameplay_cpu_steering_decode_command(
            &scene->cpu_steering_assets,
            scene->live_foundation.play_state.stream_offset[holder],
            &command);
        fprintf(trace,
                ",\"holder_lifecycle\":{\"state\":%u,\"action\":%u,"
                "\"wait\":%u,\"cursor\":%u,\"next_opcode\":%d,"
                "\"last_accepted_next\":%u,\"route_active\":%s,"
                "\"route_remaining\":%u,\"step_serial\":%u,"
                "\"decision_serial\":%u,\"position\":[%d,%d],"
                "\"cpu_target\":[%s,%d,%d,%u],"
                "\"source_target\":[%s,%d,%d,%u],"
                "\"deferred\":%s,\"defer_reason\":%u}",
                (unsigned)scene->live_foundation.play_state.actor_state[holder],
                (unsigned)scene->live_foundation.play_state
                    .action_state_046e[holder],
                (unsigned)scene->live_foundation.play_state.wait_counter[holder],
                (unsigned)scene->live_foundation.play_state
                    .stream_offset[holder], decoded ? (int)command.opcode : -1,
                (unsigned)scene->live_foundation.last_step_offset[holder],
                route->active ? "true" : "false",
                (unsigned)route->remaining_timer,
                (unsigned)scene->live_foundation.play_state.step_serial,
                (unsigned)cpu->decision_serial,
                (int)scene->actors[holder].position.x,
                (int)scene->actors[holder].position.y,
                cpu->target_valid ? "true" : "false",
                (int)cpu->target_position.x, (int)cpu->target_position.y,
                (unsigned)cpu->target_kind,
                scene->live_foundation.source_target_valid[holder]
                    ? "true" : "false",
                (int)scene->live_foundation.play_state.target_x[holder],
                (int)scene->live_foundation.play_state.target_depth[holder],
                (unsigned)scene->live_foundation.play_state
                    .target_object[holder],
                scene->live_foundation.deferred[holder] ? "true" : "false",
                (unsigned)scene->live_foundation.deferred_reason[holder]);
    } else {
        fputs(",\"holder_lifecycle\":null", trace);
    }
    fputs(",\"actors\":[", trace);
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        PossessionProofActorExtent extent;
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        if (!proof_actor_extent(scene, &court, actor, &extent)) return false;
        if (!extent.anchor_within) *anchor_oob_out = true;
        if (extent.left_overhang > frame_max) frame_max = extent.left_overhang;
        if (extent.right_overhang > frame_max) frame_max = extent.right_overhang;
        fprintf(trace,
                "%s{\"slot\":%u,\"anchor\":[%d,%d],"
                "\"bounds\":[%d,%d],\"anchor_within\":%s,"
                "\"projected_foot\":[%s,%u,%u],\"mirror\":%s,"
                "\"pose_bbox_relative\":[%d,%d,%d,%d],"
                "\"pose_bbox_world\":[%d,%d,%d,%d],"
                "\"court_edge_overhang\":[%d,%d]}",
                actor == 0U ? "" : ",", (unsigned)actor,
                (int)item->position.x, (int)item->position.y,
                extent.left_edge, extent.right_edge,
                extent.anchor_within ? "true" : "false",
                extent.projected_visible ? "true" : "false",
                (unsigned)extent.screen_x, (unsigned)extent.screen_y,
                extent.mirror ? "true" : "false",
                extent.pose_min_dx, extent.pose_max_dx,
                extent.pose_min_dy, extent.pose_max_dy,
                extent.world_min_x, extent.world_max_x,
                extent.world_min_y, extent.world_max_y,
                extent.left_overhang, extent.right_overhang);
    }
    fputs("]}\n", trace);
    if (frame_max > *max_overhang_out) *max_overhang_out = frame_max;
    return !ferror(trace);
}

bool tecmo_gameplay_cpu_possession_proof(
    const char *project_root, const char *asset_pack_path,
    const char *trace_path, const char *mid_png_path,
    const char *terminal_png_path, char *message, size_t message_size)
{
    TecmoRuntime runtime;
    TecmoGameMemory memory;
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame neutral;
    FILE *trace = NULL;
    void *permanent_block = NULL;
    void *transient_block = NULL;
    TecmoGameplayCourtCoordinate holder_before = {0, 0};
    uint8_t possession_before;
    uint8_t holder_before_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint16_t action_before = 0U;
    uint32_t decision_before = 0U;
    uint16_t step_before = 0U;
    uint16_t cursor_before = 0U;
    uint8_t state_before = 0U;
    uint8_t wait_before = 0U;
    unsigned update;
    unsigned idle_frames = 0U;
    unsigned max_idle_frames = 0U;
    unsigned pass_events = 0U;
    int max_overhang = -1;
    uint32_t mid_hash = 0U;
    uint32_t terminal_hash = 0U;
    bool anchor_oob = false;
    bool legitimate_outcome = false;
    const char *outcome = "horizon-exhausted";
    bool initialized = false;
    bool result = false;
    if (message != NULL && message_size != 0U) message[0] = '\0';
    if (project_root == NULL || asset_pack_path == NULL ||
        trace_path == NULL || mid_png_path == NULL ||
        terminal_png_path == NULL || message == NULL || message_size == 0U) {
        return false;
    }
    memset(&runtime, 0, sizeof(runtime));
    memset(&memory, 0, sizeof(memory));
    memset(&neutral, 0, sizeof(neutral));
    permanent_block = malloc(POSSESSION_PROOF_MEMORY_SIZE);
    transient_block = malloc(POSSESSION_PROOF_MEMORY_SIZE);
    if (permanent_block == NULL || transient_block == NULL) goto done;
    tecmo_arena_init(&memory.permanent, permanent_block,
                     POSSESSION_PROOF_MEMORY_SIZE);
    tecmo_arena_init(&memory.transient, transient_block,
                     POSSESSION_PROOF_MEMORY_SIZE);
    runtime.memory = &memory;
    tecmo_gameplay_scene_init(&runtime.gameplay_scene);
    initialized = true;
    if (!tecmo_music_asset_load_from_pack(&runtime.music_asset,
                                          asset_pack_path)) goto done;
    tecmo_music_player_init(&runtime.music_player, &runtime.music_asset);
    if (!tecmo_gameplay_scene_load(&runtime.gameplay_scene, project_root,
                                   asset_pack_path, &runtime.music_player)) {
        goto done;
    }
    proof_launch_init(&launch);
    if (!tecmo_gameplay_scene_launch(&runtime.gameplay_scene, &launch) ||
        !proof_advance_pretip(&runtime.gameplay_scene)) goto done;
    runtime.gameplay_scene.launch.controller_team[0U] =
        TECMO_GAMEPLAY_SCENE_NO_TEAM;
    runtime.gameplay_scene.controlled_actor[0U] =
        TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    runtime.gameplay_scene.live_foundation.control_mode[
        TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    if (
        !scene_handoff_possession(&runtime.gameplay_scene,
                                  TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        !scene_sync_live_foundation(&runtime.gameplay_scene) ||
        !scene_begin_inbound(&runtime.gameplay_scene,
                             TECMO_GAMEPLAY_TEAM_AWAY)) goto done;
    for (update = 0U; update < POSSESSION_PROOF_INBOUND_LIMIT &&
         scene_inbound_active(&runtime.gameplay_scene); ++update) {
        if (!tecmo_gameplay_scene_update(&runtime.gameplay_scene,
                                         &neutral, &neutral)) goto done;
    }
    if (scene_inbound_active(&runtime.gameplay_scene) ||
        runtime.gameplay_scene.state.shot_clock != 24U ||
        runtime.gameplay_scene.state.clock_divider != 50U ||
        runtime.gameplay_scene.ball_holder >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) goto done;
    runtime.mode = TECMO_MODE_COURT;
    runtime.normal_play_active = true;
    runtime.debug_overlay = false;
    runtime.frame_seconds = 1.0f / 60.0f;
    if (fopen_s(&trace, trace_path, "wb") != 0 || trace == NULL) goto done;
    possession_before = (uint8_t)runtime.gameplay_scene.state.possession;
    for (update = 0U; update <= POSSESSION_PROOF_OUTER_LIMIT; ++update) {
        TecmoGameplayScene *scene = &runtime.gameplay_scene;
        uint8_t holder = scene->ball_holder;
        int dx = 0;
        int dy = 0;
        int previous_max = max_overhang;
        bool same_lifecycle = false;
        if (holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
            holder == holder_before_actor) {
            dx = scene->actors[holder].position.x - holder_before.x;
            dy = scene->actors[holder].position.y - holder_before.y;
            same_lifecycle = dx == 0 && dy == 0 &&
                scene->action_serial == action_before &&
                scene->cpu_actors[holder].decision_serial == decision_before &&
                scene->live_foundation.play_state.step_serial == step_before &&
                scene->live_foundation.play_state.stream_offset[holder] ==
                    cursor_before &&
                scene->live_foundation.play_state.actor_state[holder] ==
                    state_before &&
                scene->live_foundation.play_state.wait_counter[holder] ==
                    wait_before && !scene_pass_active(scene) &&
                scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE;
        }
        idle_frames = same_lifecycle ? idle_frames + 1U : 0U;
        if (idle_frames > max_idle_frames) max_idle_frames = idle_frames;
        if (!proof_write_frame(trace, scene, update, dx, dy, idle_frames,
                               &max_overhang, &anchor_oob)) goto done;
        runtime.frame_counter = scene->frame;
        runtime.mode_frame_counter = scene->frame;
        if (max_overhang > previous_max &&
            !proof_render(&runtime, mid_png_path, &mid_hash)) goto done;
        if (anchor_oob) {
            outcome = "anchor-oob";
            break;
        }
        if (scene->state.violation == TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK) {
            outcome = "shot-clock-violation";
            break;
        }
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            legitimate_outcome = true;
            outcome = "shot-launched";
            break;
        }
        if ((uint8_t)scene->state.possession != possession_before) {
            legitimate_outcome = true;
            outcome = "possession-changed";
            break;
        }
        if (update == POSSESSION_PROOF_OUTER_LIMIT) break;
        if (scene_pass_active(scene)) ++pass_events;
        holder_before_actor = holder;
        if (holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            holder_before = scene->actors[holder].position;
            action_before = scene->action_serial;
            decision_before = scene->cpu_actors[holder].decision_serial;
            step_before = scene->live_foundation.play_state.step_serial;
            cursor_before = scene->live_foundation.play_state
                .stream_offset[holder];
            state_before = scene->live_foundation.play_state
                .actor_state[holder];
            wait_before = scene->live_foundation.play_state
                .wait_counter[holder];
        }
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral)) goto done;
    }
    if (!proof_render(&runtime, terminal_png_path, &terminal_hash)) goto done;
    fclose(trace);
    trace = NULL;
    (void)snprintf(
        message, message_size,
        "{\"schema\":\"tecmo.cpu-possession-proof/TGPH-1\","
        "\"passed\":%s,\"structured_state_authority\":true,"
        "\"screenshot_scope\":\"presentation-only\","
        "\"fixture\":\"production automatic inbound; native clocks 24/50\","
        "\"outer_update_limit\":1085,\"updates_observed\":%u,"
        "\"outcome\":\"%s\",\"legitimate_outcome\":%s,"
        "\"anchor_oob\":%s,\"max_idle_frames\":%u,"
        "\"pass_active_frame_count\":%u,\"max_pose_overhang\":%d,"
        "\"mid_frame_fnv1a32\":\"%08X\","
        "\"terminal_frame_fnv1a32\":\"%08X\"}",
        legitimate_outcome && !anchor_oob ? "true" : "false",
        update, outcome, legitimate_outcome ? "true" : "false",
        anchor_oob ? "true" : "false", max_idle_frames, pass_events,
        max_overhang, (unsigned)mid_hash, (unsigned)terminal_hash);
    result = legitimate_outcome && !anchor_oob;
done:
    if (trace != NULL) fclose(trace);
    if (message != NULL && message_size != 0U && message[0] == '\0') {
        (void)snprintf(message, message_size,
                       "CPU possession proof setup or telemetry failed");
    }
    if (initialized) {
        tecmo_gameplay_scene_destroy(&runtime.gameplay_scene);
        tecmo_music_asset_shutdown(&runtime.music_asset);
    }
    free(permanent_block);
    free(transient_block);
    return result;
}
