#include "tecmo_gameplay_cpu_possession_proof.h"

#include "png_writer.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_scene_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POSSESSION_PROOF_WIDTH 640
#define POSSESSION_PROOF_HEIGHT 480
#define POSSESSION_PROOF_MEMORY_SIZE (16U * 1024U * 1024U)
#define POSSESSION_PROOF_PRETIP_LIMIT 2048U
#define POSSESSION_PROOF_INBOUND_LIMIT 128U
#define POSSESSION_PROOF_OUTER_LIMIT 20000U
#define POSSESSION_PROOF_NO_EFFECT_LIMIT 1U

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
    bool world_anchor_within;
    bool scene_anchor_valid;
    bool ordinary_gate_band;
    bool projected_visible;
    uint8_t screen_x;
    uint8_t screen_y;
    bool mirror;
} PossessionProofActorExtent;

typedef struct PossessionProofInboundPromotionResult {
    uint16_t stale_cursor;
    uint8_t actor;
    bool adversarial_state_valid;
    bool inbound_started;
    bool stale_suppressed;
    bool promoted;
} PossessionProofInboundPromotionResult;

typedef struct PossessionProofFirstOutcome {
    uint16_t score_before[TECMO_GAMEPLAY_TEAM_COUNT];
    uint16_t score_after[TECMO_GAMEPLAY_TEAM_COUNT];
    uint32_t claimant_serial_before;
    uint32_t claimant_serial_after;
    uint8_t shot_kind;
    uint8_t shot_outcome;
    uint8_t shooting_team;
    uint8_t possession_after;
    uint8_t holder_after;
    uint16_t cursor_after;
    uint8_t state_after;
    uint8_t action_after;
    uint8_t wait_after;
    bool launch_captured;
    bool settlement_captured;
    bool claimant_valid_before;
    bool claimant_valid_after;
    bool automatic_new_holder;
    bool route_cleared;
    bool target_cleared;
    bool defer_cleared;
    bool normalized;
} PossessionProofFirstOutcome;

typedef struct PossessionProofOwnershipFixtures {
    bool controllerless_automatic;
    bool p1_direct_holder;
    bool p2_direct_holder;
    bool invalid_same_team_other_actor_unowned;
} PossessionProofOwnershipFixtures;

typedef struct PossessionProofSourceProgression {
    bool start_059b_opcode3;
    bool installed_state6_wait30_cursor05a0;
    unsigned countdown_ticks;
    bool returned_state4_cursor05a0;
    bool fetched_05a0_opcode2_to_05a5;
} PossessionProofSourceProgression;

static bool proof_route_motion_cleared(
    const TecmoGameplayCpuSteeringRouteMotionState *route)
{
    return route != NULL &&
        route->contract_tag ==
            TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG &&
        route->horizontal_accumulator_q6 == 0U &&
        route->depth_accumulator_q6 == 0U &&
        route->horizontal_velocity_q6 == 0 &&
        route->depth_velocity_q6 == 0 &&
        route->remaining_timer == 0U && !route->active;
}

static const char *proof_shot_outcome_name(uint8_t outcome)
{
    switch ((TecmoGameplayShotOutcome)outcome) {
    case TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN: return "unknown";
    case TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE: return "make";
    case TECMO_GAMEPLAY_SHOT_OUTCOME_MISS: return "miss";
    default: return "invalid";
    }
}

static bool proof_team_is_automatic(const TecmoGameplayScene *scene,
                                    uint8_t team)
{
    size_t controller;
    if (scene == NULL || team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->live_foundation.control_mode[team] == 0U) {
        return false;
    }
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == team) return false;
    }
    return true;
}

static void proof_capture_first_settlement(
    const TecmoGameplayScene *scene, PossessionProofFirstOutcome *first)
{
    const TecmoGameplayLiveFoundation *live;
    uint8_t holder;
    if (scene == NULL || first == NULL || first->settlement_captured) return;
    live = &scene->live_foundation;
    holder = scene->ball_holder;
    first->settlement_captured = true;
    first->score_after[TECMO_GAMEPLAY_TEAM_AWAY] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    first->score_after[TECMO_GAMEPLAY_TEAM_HOME] =
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
    first->claimant_valid_after = scene->claimant_settlement_trace.valid;
    first->claimant_serial_after =
        scene->claimant_settlement_trace.event_serial;
    first->possession_after = (uint8_t)scene->state.possession;
    first->holder_after = holder;
    if (holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return;
    first->automatic_new_holder =
        proof_team_is_automatic(scene, first->possession_after) &&
        scene->actors[holder].team == first->possession_after &&
        live->primary_actor == holder &&
        live->play_state.primary_actor == holder &&
        live->last_ball_holder == holder;
    first->cursor_after = live->play_state.stream_offset[holder];
    first->state_after = live->play_state.actor_state[holder];
    first->action_after = live->play_state.action_state_046e[holder];
    first->wait_after = live->play_state.wait_counter[holder];
    first->route_cleared =
        proof_route_motion_cleared(&live->play_state.route_motion[holder]);
    first->target_cleared =
        !live->source_target_valid[holder] &&
        !live->source_direction_valid[holder] &&
        live->source_direction[holder] ==
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION &&
        live->play_state.direction[holder] ==
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION &&
        live->play_state.target_object[holder] ==
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
        live->play_state.target_x[holder] == 0 &&
        live->play_state.target_depth[holder] == 0;
    first->defer_cleared = !live->deferred[holder] &&
        live->deferred_reason[holder] ==
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    first->normalized = first->automatic_new_holder &&
        first->cursor_after == 0x007DU &&
        live->last_step_offset[holder] == 0x007DU &&
        first->state_after == 0x04U && first->action_after == 0x18U &&
        first->wait_after == 0U && first->route_cleared &&
        first->target_cleared && first->defer_cleared;
}

static const char *proof_first_outcome_classification(
    const PossessionProofFirstOutcome *first)
{
    if (first == NULL || !first->launch_captured ||
        !first->settlement_captured) return "not-observed";
    if (first->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        return "non-jump-shot";
    }
    if (first->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) {
        return "jump-make";
    }
    if (first->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return "jump-outcome-unknown";
    }
    if (first->score_before[TECMO_GAMEPLAY_TEAM_AWAY] !=
            first->score_after[TECMO_GAMEPLAY_TEAM_AWAY] ||
        first->score_before[TECMO_GAMEPLAY_TEAM_HOME] !=
            first->score_after[TECMO_GAMEPLAY_TEAM_HOME]) {
        return "jump-miss-score-changed";
    }
    if (first->claimant_valid_before || first->claimant_valid_after ||
        first->claimant_serial_before != first->claimant_serial_after) {
        return "jump-miss-claimant-settlement";
    }
    if (first->shooting_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        first->possession_after != (uint8_t)(first->shooting_team ^ 1U)) {
        return "jump-miss-possession-mismatch";
    }
    if (!first->automatic_new_holder) {
        return "jump-miss-new-holder-not-automatic";
    }
    if (!first->normalized) return "jump-miss-handoff-not-normalized";
    return "jump-miss-generic-compatibility-handoff";
}

static bool proof_inbound_promotion(
    const TecmoGameplayScene *scene, uint16_t stale_cursor,
    PossessionProofInboundPromotionResult *result_out)
{
    /* Deliberately adversarial proof fixture: these stale ordinary-actor
       values are not claimed as a valid inbound receiver lifecycle. They
       model the two observed pre-promotion cursors at the Bank07 restart
       boundary and prove production inbound setup suppresses them without an
       AI dispatch; the B24F-shaped catch must then install the automatic
       receiver's bounded long route/state/action endpoint. */
    TecmoGameplayScene candidate;
    TecmoControlFrame neutral;
    TecmoGameplayCpuSteeringRouteMotionState *route;
    uint8_t actor;
    uint8_t side;
    uint16_t step_before;
    unsigned update;
    if (scene == NULL || result_out == NULL ||
        scene->live_foundation.primary_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    memset(result_out, 0, sizeof(*result_out));
    result_out->stale_cursor = stale_cursor;
    candidate = *scene;
    memset(&neutral, 0, sizeof(neutral));
    side = candidate.live_foundation.offense_side;
    actor = candidate.live_foundation.candidate_actor_by_side[side];
    if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        actor == candidate.live_foundation.primary_actor ||
        candidate.actors[actor].team != side) return false;
    result_out->actor = actor;
    candidate.live_foundation.play_state.stream_offset[actor] = stale_cursor;
    candidate.live_foundation.last_step_offset[actor] = stale_cursor;
    candidate.live_foundation.play_state.actor_state[actor] = 0x0BU;
    candidate.live_foundation.play_state.action_state_046e[actor] = 0U;
    candidate.live_foundation.play_state.wait_counter[actor] = 9U;
    candidate.live_foundation.play_state.target_object[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.live_foundation.play_state.target_x[actor] =
        candidate.actors[actor].position.x;
    candidate.live_foundation.play_state.target_depth[actor] =
        candidate.actors[actor].position.y;
    candidate.live_foundation.source_target_valid[actor] = true;
    candidate.live_foundation.source_direction_valid[actor] = false;
    candidate.live_foundation.source_direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate.live_foundation.play_state.direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    route = &candidate.live_foundation.play_state.route_motion[actor];
    memset(route, 0, sizeof(*route));
    route->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    result_out->adversarial_state_valid =
        tecmo_gameplay_live_foundation_valid(
            &candidate.cpu_steering_assets, &candidate.live_foundation);
    if (!result_out->adversarial_state_valid) return false;
    step_before = candidate.live_foundation.play_state.step_serial;
    if (!scene_begin_inbound(&candidate, (TecmoGameplayTeam)side)) return true;
    result_out->inbound_started = true;
    if (candidate.inbound_state.receiver != actor) return true;
    result_out->stale_suppressed =
        candidate.live_foundation.play_state.stream_offset[actor] !=
            stale_cursor &&
        candidate.live_foundation.play_state.actor_state[actor] != 0x0BU &&
        candidate.live_foundation.play_state.step_serial == step_before;
    if (!result_out->stale_suppressed) return true;
    for (update = 0U; update < POSSESSION_PROOF_INBOUND_LIMIT &&
         scene_inbound_active(&candidate); ++update) {
        if (!tecmo_gameplay_scene_update(&candidate, &neutral, &neutral)) {
            return true;
        }
        if (scene_inbound_active(&candidate) &&
            candidate.live_foundation.play_state.step_serial != step_before)
            return true;
    }
    if (scene_inbound_active(&candidate)) return true;
    result_out->promoted =
        candidate.ball_holder == actor &&
        candidate.live_foundation.primary_actor == actor &&
        candidate.live_foundation.play_state.primary_actor == actor &&
        candidate.live_foundation.last_ball_holder == actor &&
        candidate.live_foundation.play_state.stream_offset[actor] == 0x00D7U &&
        candidate.live_foundation.last_step_offset[actor] == 0x00D7U &&
        candidate.live_foundation.play_state.actor_state[actor] == 0x04U &&
        candidate.live_foundation.play_state.action_state_046e[actor] == 0x18U &&
        candidate.live_foundation.play_state.wait_counter[actor] == 0U &&
        !candidate.live_foundation.source_target_valid[actor] &&
        candidate.live_foundation.play_state.target_object[actor] ==
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
        candidate.live_foundation.play_state.target_x[actor] == 0 &&
        candidate.live_foundation.play_state.target_depth[actor] == 0 &&
        proof_route_motion_cleared(
            &candidate.live_foundation.play_state.route_motion[actor]) &&
        !candidate.live_foundation.deferred[actor] &&
        candidate.live_foundation.deferred_reason[actor] ==
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE &&
        tecmo_gameplay_live_foundation_valid(
            &candidate.cpu_steering_assets, &candidate.live_foundation);
    return true;
}

static bool proof_ownership_fixtures(
    const TecmoGameplayScene *scene,
    PossessionProofOwnershipFixtures *result)
{
    TecmoGameplayScene candidate;
    TecmoDebugCpuOwnershipSnapshot snapshot;
    uint8_t holder;
    if (scene == NULL || result == NULL ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    memset(result, 0, sizeof(*result));
    holder = scene->ball_holder;
    candidate = *scene;
    candidate.launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    candidate.launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    candidate.controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    candidate.controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    result->controllerless_automatic =
        tecmo_debug_cpu_ownership_snapshot(&candidate, &snapshot) &&
        snapshot.holder_owner ==
            TECMO_DEBUG_CPU_HOLDER_OWNER_AUTOMATIC_PRIMARY &&
        snapshot.automatic_selected_eligible &&
        snapshot.automatic_selected_admitted;

    candidate.launch.controller_team[0U] =
        (uint8_t)candidate.state.possession;
    candidate.controlled_actor[0U] = holder;
    result->p1_direct_holder =
        tecmo_debug_cpu_ownership_snapshot(&candidate, &snapshot) &&
        snapshot.holder_owner == TECMO_DEBUG_CPU_HOLDER_OWNER_HUMAN_P1 &&
        !snapshot.automatic_selected_eligible &&
        !snapshot.automatic_selected_admitted;

    candidate.launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    candidate.controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    candidate.launch.controller_team[1U] =
        (uint8_t)candidate.state.possession;
    candidate.controlled_actor[1U] = holder;
    result->p2_direct_holder =
        tecmo_debug_cpu_ownership_snapshot(&candidate, &snapshot) &&
        snapshot.holder_owner == TECMO_DEBUG_CPU_HOLDER_OWNER_HUMAN_P2 &&
        !snapshot.automatic_selected_eligible &&
        !snapshot.automatic_selected_admitted;

    candidate.launch.controller_team[1U] =
        (uint8_t)candidate.state.possession;
    candidate.controlled_actor[1U] = holder < 5U
        ? (uint8_t)((holder + 1U) % 5U)
        : (uint8_t)(5U + ((holder - 5U + 1U) % 5U));
    result->invalid_same_team_other_actor_unowned =
        tecmo_debug_cpu_ownership_snapshot(&candidate, &snapshot) &&
        snapshot.holder_owner == TECMO_DEBUG_CPU_HOLDER_OWNER_UNOWNED &&
        !snapshot.automatic_selected_eligible &&
        !snapshot.automatic_selected_admitted;
    return result->controllerless_automatic && result->p1_direct_holder &&
        result->p2_direct_holder &&
        result->invalid_same_team_other_actor_unowned;
}

static bool proof_source_progression_059b(
    const TecmoGameplayScene *scene,
    PossessionProofSourceProgression *result)
{
    TecmoGameplayScene candidate;
    TecmoGameplaySceneCpuShotRequest shot_request;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCpuSteeringRouteMotionState *route;
    uint8_t actor;
    unsigned tick;
    if (scene == NULL || result == NULL ||
        scene->live_foundation.primary_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    memset(result, 0, sizeof(*result));
    candidate = *scene;
    actor = candidate.live_foundation.primary_actor;
    if (actor != candidate.ball_holder) return false;
    candidate.launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    candidate.launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    candidate.controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    candidate.controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    candidate.live_foundation.play_state.stream_offset[actor] = 0x059BU;
    candidate.live_foundation.last_step_offset[actor] = 0x059BU;
    candidate.live_foundation.play_state.actor_state[actor] = 0x04U;
    candidate.live_foundation.play_state.action_state_046e[actor] = 0U;
    candidate.live_foundation.play_state.wait_counter[actor] = 0U;
    candidate.live_foundation.play_state.target_object[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.live_foundation.play_state.target_x[actor] = 0;
    candidate.live_foundation.play_state.target_depth[actor] = 0;
    candidate.live_foundation.play_state.direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate.live_foundation.source_target_valid[actor] = false;
    candidate.live_foundation.source_direction_valid[actor] = false;
    candidate.live_foundation.source_direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate.live_foundation.deferred[actor] = false;
    candidate.live_foundation.deferred_reason[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    route = &candidate.live_foundation.play_state.route_motion[actor];
    memset(route, 0, sizeof(*route));
    route->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    if (!tecmo_gameplay_live_foundation_valid(
            &candidate.cpu_steering_assets, &candidate.live_foundation) ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &candidate.cpu_steering_assets, 0x059BU, &command)) return false;
    result->start_059b_opcode3 = command.opcode == 3U;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!result->start_059b_opcode3 ||
        !scene_update_ai(&candidate, &shot_request) ||
        shot_request.requested) return false;
    result->installed_state6_wait30_cursor05a0 =
        candidate.live_foundation.play_state.actor_state[actor] == 0x06U &&
        candidate.live_foundation.play_state.wait_counter[actor] == 30U &&
        candidate.live_foundation.play_state.stream_offset[actor] == 0x05A0U;
    if (!result->installed_state6_wait30_cursor05a0) return false;
    for (tick = 0U; tick < 30U; ++tick) {
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(&candidate, &shot_request) ||
            shot_request.requested ||
            candidate.live_foundation.play_state.stream_offset[actor] !=
                0x05A0U) return false;
        ++result->countdown_ticks;
    }
    result->returned_state4_cursor05a0 =
        candidate.live_foundation.play_state.actor_state[actor] == 0x04U &&
        candidate.live_foundation.play_state.wait_counter[actor] == 0U;
    if (!result->returned_state4_cursor05a0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &candidate.cpu_steering_assets, 0x05A0U, &command) ||
        command.opcode != 2U) return false;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(&candidate, &shot_request) ||
        shot_request.requested) return false;
    result->fetched_05a0_opcode2_to_05a5 =
        candidate.live_foundation.play_state.stream_offset[actor] == 0x05A5U &&
        candidate.live_foundation.last_step_offset[actor] == 0x05A5U &&
        candidate.live_foundation.source_target_valid[actor];
    return result->fetched_05a0_opcode2_to_05a5;
}

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
    launch->controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch->controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch->game_music_enabled = false;
    launch->starter_binding_bound = true;
    memcpy(launch->starter_roster_index[0U], away, sizeof(away));
    memcpy(launch->starter_roster_index[1U], home, sizeof(home));
}

static bool proof_advance_pretip(TecmoGameplayScene *scene)
{
    TecmoControlFrame neutral;
    size_t update;
    memset(&neutral, 0, sizeof(neutral));
    for (update = 0U; update < POSSESSION_PROOF_PRETIP_LIMIT &&
         tecmo_gameplay_scene_in_pretip(scene); ++update) {
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral)) {
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
    extent->world_anchor_within =
        tecmo_gameplay_court_coordinate_valid(&item->position);
    extent->ordinary_gate_band = item->position.y >= TECMO_GAMEPLAY_MIN_Y &&
        item->position.y <= TECMO_GAMEPLAY_MAX_Y &&
        item->position.x >= extent->left_edge &&
        item->position.x <= extent->right_edge;
    extent->scene_anchor_valid = scene_actor_world_position_valid(item);
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
                              bool *anchor_oob_out,
                              unsigned *gate_excursion_count_out)
{
    TecmoGameplaySceneCourtFrame court;
    TecmoDebugCpuOwnershipSnapshot ownership;
    size_t actor;
    int frame_max = 0;
    uint8_t holder = scene->ball_holder;
    if (trace == NULL || scene == NULL || max_overhang_out == NULL ||
        anchor_oob_out == NULL || gate_excursion_count_out == NULL ||
        !tecmo_gameplay_scene_court_frame(scene, &court) ||
        !tecmo_debug_cpu_ownership_snapshot(scene, &ownership)) return false;
    fprintf(trace,
            "{\"schema\":\"tecmo.cpu-possession-frame/TGPH-4\","
            "\"update\":%u,\"scene_frame\":%u,\"phase\":%u,"
            "\"violation\":%u,\"possession\":%u,\"holder\":%u,"
            "\"controller_team\":[%u,%u],\"controlled_actor\":[%u,%u],"
            "\"holder_owned_by\":\"%s\",\"automatic_selected_eligible\":%s,"
            "\"automatic_selected_admitted\":%s,"
            "\"primary\":%u,\"defender\":%u,\"clock\":[%u,%u,%u],"
            "\"shot_clock\":%u,\"action_serial\":%u,"
            "\"aggregation\":[%u,%u,%u],"
            "\"claimant_trace\":[%s,%u],"
            "\"shot\":[%u,%u,%u],\"pass\":[%u,%u,%u,%u],"
            "\"holder_delta\":[%d,%d],\"idle_frames\":%u",
            update, (unsigned)scene->frame, (unsigned)scene->state.phase,
            (unsigned)scene->state.violation,
            (unsigned)scene->state.possession, (unsigned)holder,
            (unsigned)ownership.controller_team[0U],
            (unsigned)ownership.controller_team[1U],
            (unsigned)ownership.controlled_actor[0U],
            (unsigned)ownership.controlled_actor[1U],
            tecmo_debug_cpu_holder_owner_name(ownership.holder_owner),
            ownership.automatic_selected_eligible ? "true" : "false",
            ownership.automatic_selected_admitted ? "true" : "false",
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->state.clock_minutes,
            (unsigned)scene->state.clock_seconds,
            (unsigned)scene->state.clock_divider,
            (unsigned)scene->state.shot_clock,
            (unsigned)scene->action_serial,
            (unsigned)scene->live_foundation.play_state.aggregation_06df,
            (unsigned)scene->live_foundation.play_state.aggregation_06e0,
            (unsigned)scene->live_foundation.play_state.aggregation_06e1,
            scene->claimant_settlement_trace.valid ? "true" : "false",
            (unsigned)scene->claimant_settlement_trace.event_serial,
            (unsigned)scene->shot_kind,
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
        if (!extent.scene_anchor_valid) *anchor_oob_out = true;
        if (!extent.ordinary_gate_band &&
            (scene->live_foundation.play_state.actor_state[actor] == 0x05U ||
             scene->live_foundation.play_state.route_motion[actor].active)) {
            ++*gate_excursion_count_out;
        }
        if (extent.left_overhang > frame_max) frame_max = extent.left_overhang;
        if (extent.right_overhang > frame_max) frame_max = extent.right_overhang;
        fprintf(trace,
                "%s{\"slot\":%u,\"anchor\":[%d,%d],"
                "\"ordinary_gate_band\":[%d,%d,%s],"
                "\"world_anchor_within\":%s,\"scene_anchor_valid\":%s,"
                "\"projected_anchor\":[%s,%u,%u],\"mirror\":%s,"
                "\"pose_bbox_relative\":[%d,%d,%d,%d],"
                "\"pose_bbox_world\":[%d,%d,%d,%d],"
                "\"court_edge_overhang\":[%d,%d]}",
                actor == 0U ? "" : ",", (unsigned)actor,
                (int)item->position.x, (int)item->position.y,
                extent.left_edge, extent.right_edge,
                extent.ordinary_gate_band ? "true" : "false",
                extent.world_anchor_within ? "true" : "false",
                extent.scene_anchor_valid ? "true" : "false",
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
    const char *trace_path, const char *mid_horizon_png_path,
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
    uint8_t holder_before_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint16_t action_before = 0U;
    uint32_t decision_before = 0U;
    uint16_t step_before = 0U;
    uint16_t cursor_before = 0U;
    uint8_t state_before = 0U;
    uint8_t actor_action_before = 0U;
    uint8_t wait_before = 0U;
    bool route_active_before = false;
    uint16_t route_remaining_before = 0U;
    bool source_target_before = false;
    bool deferred_before = false;
    bool automatic_admitted_before = false;
    uint8_t movement_action_before = 0U;
    uint8_t movement_fraction_before = 0U;
    unsigned update;
    unsigned idle_frames = 0U;
    unsigned max_idle_frames = 0U;
    unsigned no_effect_streak = 0U;
    unsigned max_no_effect_streak = 0U;
    unsigned gate_excursion_count = 0U;
    unsigned pass_events = 0U;
    unsigned possession_outcomes = 0U;
    unsigned legitimate_possession_outcomes = 0U;
    unsigned shot_launches = 0U;
    unsigned selected_state0b_update = UINT_MAX;
    unsigned mid_horizon_update = UINT_MAX;
    uint8_t possession_observed;
    uint8_t period_observed;
    int max_overhang = -1;
    uint32_t mid_horizon_hash = 0U;
    uint32_t terminal_hash = 0U;
    bool anchor_oob = false;
    bool selected_state0b_observed = false;
    bool mid_horizon_captured = false;
    bool reached_beyond_one_minute = false;
    bool no_effect_failure = false;
    bool ownership_failure = false;
    bool shot_active_before = false;
    bool possession_had_shot = false;
    bool update_failed = false;
    bool first_outcome_captured = false;
    PossessionProofFirstOutcome first_outcome;
    PossessionProofInboundPromotionResult promotion_0627;
    PossessionProofInboundPromotionResult promotion_0488;
    PossessionProofOwnershipFixtures ownership_fixtures;
    PossessionProofSourceProgression progression_059b;
    const char *outcome = "horizon-exhausted";
    const char *setup_stage = "arguments";
    bool initialized = false;
    bool result = false;
    if (message != NULL && message_size != 0U) message[0] = '\0';
    if (project_root == NULL || asset_pack_path == NULL ||
        trace_path == NULL || mid_horizon_png_path == NULL ||
        terminal_png_path == NULL ||
        message == NULL || message_size == 0U) {
        return false;
    }
    memset(&runtime, 0, sizeof(runtime));
    memset(&memory, 0, sizeof(memory));
    memset(&neutral, 0, sizeof(neutral));
    memset(&first_outcome, 0, sizeof(first_outcome));
    memset(&ownership_fixtures, 0, sizeof(ownership_fixtures));
    memset(&progression_059b, 0, sizeof(progression_059b));
    permanent_block = malloc(POSSESSION_PROOF_MEMORY_SIZE);
    transient_block = malloc(POSSESSION_PROOF_MEMORY_SIZE);
    if (permanent_block == NULL || transient_block == NULL) goto done;
    tecmo_arena_init(&memory.permanent, permanent_block,
                     POSSESSION_PROOF_MEMORY_SIZE);
    tecmo_arena_init(&memory.transient, transient_block,
                     POSSESSION_PROOF_MEMORY_SIZE);
    runtime.memory = &memory;
    setup_stage = "scene-load";
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
    setup_stage = "controllerless-pretip";
    if (!tecmo_gameplay_scene_launch(&runtime.gameplay_scene, &launch) ||
        !proof_advance_pretip(&runtime.gameplay_scene)) goto done;
    setup_stage = "production-inbound";
    /* Deterministic 24/50 clock fixture feeding the real inbound transport. */
    runtime.gameplay_scene.state.shot_clock = 24U;
    runtime.gameplay_scene.state.clock_divider = 50U;
    if (runtime.gameplay_scene.state.shot_clock != 24U ||
        runtime.gameplay_scene.state.clock_divider != 50U) {
        (void)snprintf(message, message_size,
                       "pre-inbound native clocks rejected %u/%u",
                       (unsigned)runtime.gameplay_scene.state.shot_clock,
                       (unsigned)runtime.gameplay_scene.state.clock_divider);
        goto done;
    }
    if (!scene_begin_inbound(
            &runtime.gameplay_scene,
            runtime.gameplay_scene.state.possession)) {
        (void)snprintf(message, message_size,
                       "production inbound rejected phase=%u possession=%u holder=%u inbound=%u",
                       (unsigned)runtime.gameplay_scene.state.phase,
                       (unsigned)runtime.gameplay_scene.state.possession,
                       (unsigned)runtime.gameplay_scene.ball_holder,
                       (unsigned)runtime.gameplay_scene.inbound_state.phase);
        goto done;
    }
    setup_stage = "production-inbound-flight";
    for (update = 0U; update < POSSESSION_PROOF_INBOUND_LIMIT &&
         scene_inbound_active(&runtime.gameplay_scene); ++update) {
        if (!tecmo_gameplay_scene_update(&runtime.gameplay_scene,
                                         &neutral, &neutral)) {
            (void)snprintf(message, message_size,
                           "production inbound update rejected at %u: %s",
                           update, runtime.gameplay_scene.status);
            goto done;
        }
    }
    if (scene_inbound_active(&runtime.gameplay_scene) ||
        runtime.gameplay_scene.ball_holder >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        (void)snprintf(message, message_size,
                       "post-inbound contract rejected active=%u clocks=%u/%u holder=%u",
                       scene_inbound_active(&runtime.gameplay_scene) ? 1U : 0U,
                       (unsigned)runtime.gameplay_scene.state.shot_clock,
                       (unsigned)runtime.gameplay_scene.state.clock_divider,
                       (unsigned)runtime.gameplay_scene.ball_holder);
        goto done;
    }
    setup_stage = "post-inbound-clocks";
    setup_stage = "promotion-fixtures";
    if (!proof_inbound_promotion(
            &runtime.gameplay_scene, 0x0627U, &promotion_0627) ||
        !proof_inbound_promotion(
            &runtime.gameplay_scene, 0x0488U, &promotion_0488)) {
        goto done;
    }
    setup_stage = "ownership-fixtures";
    if (!proof_ownership_fixtures(&runtime.gameplay_scene,
                                  &ownership_fixtures)) goto done;
    setup_stage = "source-progression-059b";
    if (!proof_source_progression_059b(&runtime.gameplay_scene,
                                       &progression_059b)) goto done;
    runtime.mode = TECMO_MODE_COURT;
    runtime.normal_play_active = true;
    runtime.debug_overlay = false;
    runtime.frame_seconds = 1.0f / 60.0f;
    if (fopen_s(&trace, trace_path, "wb") != 0 || trace == NULL) goto done;
    setup_stage = "natural-horizon";
    possession_observed = (uint8_t)runtime.gameplay_scene.state.possession;
    period_observed = runtime.gameplay_scene.state.period;
    for (update = 0U; update <= POSSESSION_PROOF_OUTER_LIMIT; ++update) {
        TecmoGameplayScene *scene = &runtime.gameplay_scene;
        uint8_t holder = scene->ball_holder;
        int dx = 0;
        int dy = 0;
        bool same_lifecycle = false;
        bool selected_no_effect = false;
        TecmoDebugCpuOwnershipSnapshot ownership;
        if (!tecmo_debug_cpu_ownership_snapshot(scene, &ownership)) goto done;
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
            selected_no_effect = automatic_admitted_before &&
                ownership.automatic_selected_admitted &&
                state_before == 0x04U &&
                scene->live_foundation.play_state.actor_state[holder] ==
                    0x04U &&
                dx == 0 && dy == 0 &&
                scene->action_serial == action_before &&
                scene->cpu_actors[holder].decision_serial == decision_before &&
                scene->live_foundation.play_state.stream_offset[holder] ==
                    cursor_before &&
                scene->live_foundation.play_state.action_state_046e[holder] ==
                    actor_action_before &&
                scene->live_foundation.play_state.wait_counter[holder] ==
                    wait_before &&
                scene->live_foundation.play_state.route_motion[holder].active ==
                    route_active_before &&
                scene->live_foundation.play_state.route_motion[holder]
                        .remaining_timer == route_remaining_before &&
                scene->live_foundation.source_target_valid[holder] ==
                    source_target_before &&
                scene->live_foundation.deferred[holder] == deferred_before &&
                scene->actors[holder].movement_action_state ==
                    movement_action_before &&
                scene->actors[holder].movement_fractional_accumulator ==
                    movement_fraction_before &&
                !scene_pass_active(scene) && !scene_inbound_active(scene) &&
                scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE;
        }
        idle_frames = same_lifecycle ? idle_frames + 1U : 0U;
        no_effect_streak = selected_no_effect ? no_effect_streak + 1U : 0U;
        if (idle_frames > max_idle_frames) max_idle_frames = idle_frames;
        if (no_effect_streak > max_no_effect_streak) {
            max_no_effect_streak = no_effect_streak;
        }
        if (!proof_write_frame(trace, scene, update, dx, dy, idle_frames,
                               &max_overhang, &anchor_oob,
                               &gate_excursion_count)) goto done;
        runtime.frame_counter = scene->frame;
        runtime.mode_frame_counter = scene->frame;
        if (anchor_oob) {
            outcome = "anchor-oob";
            break;
        }
        if (holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
            scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !scene_pass_active(scene) && !scene_inbound_active(scene) &&
            (!ownership.automatic_selected_eligible ||
             !ownership.automatic_selected_admitted ||
             ownership.holder_owner !=
                 TECMO_DEBUG_CPU_HOLDER_OWNER_AUTOMATIC_PRIMARY)) {
            ownership_failure = true;
            outcome = "automatic-holder-ownership-invalid";
            break;
        }
        if (scene->state.violation != TECMO_GAMEPLAY_VIOLATION_NONE) {
            outcome = scene->state.violation ==
                    TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK
                ? "shot-clock-violation" : "other-violation";
            break;
        }
        if (no_effect_streak > POSSESSION_PROOF_NO_EFFECT_LIMIT) {
            no_effect_failure = true;
            outcome = "automatic-state4-no-effect";
            break;
        }
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
            !shot_active_before) {
            ++shot_launches;
            possession_had_shot = true;
            if (!first_outcome.launch_captured) {
                first_outcome.launch_captured = true;
                first_outcome.shot_kind = (uint8_t)scene->shot_kind;
                first_outcome.shot_outcome = (uint8_t)scene->shot_outcome;
                first_outcome.shooting_team = scene->shot_actor_team;
                first_outcome.score_before[TECMO_GAMEPLAY_TEAM_AWAY] =
                    scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
                first_outcome.score_before[TECMO_GAMEPLAY_TEAM_HOME] =
                    scene->state.score[TECMO_GAMEPLAY_TEAM_HOME];
                first_outcome.claimant_valid_before =
                    scene->claimant_settlement_trace.valid;
                first_outcome.claimant_serial_before =
                    scene->claimant_settlement_trace.event_serial;
            }
        }
        shot_active_before = scene->shot_kind !=
            TECMO_GAMEPLAY_SCENE_SHOT_NONE;
        if ((uint8_t)scene->state.possession != possession_observed) {
            ++possession_outcomes;
            if (!possession_had_shot) {
                outcome = "possession-change-without-shot-outcome";
                break;
            }
            ++legitimate_possession_outcomes;
            possession_had_shot = false;
            if (!first_outcome_captured) {
                proof_capture_first_settlement(scene, &first_outcome);
                first_outcome_captured = true;
            }
            possession_observed = (uint8_t)scene->state.possession;
        }
        if (scene->live_foundation.primary_actor <
                TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            uint8_t primary = scene->live_foundation.primary_actor;
            if (scene->live_foundation.play_state.actor_state[primary] ==
                    0x0BU) {
                selected_state0b_observed = true;
                selected_state0b_update = update;
            }
        }
        if (!mid_horizon_captured && scene->state.clock_minutes <= 1U) {
            mid_horizon_captured = true;
            mid_horizon_update = update;
            if (!proof_render(&runtime, mid_horizon_png_path,
                              &mid_horizon_hash)) goto done;
        }
        if (scene->state.clock_minutes == 0U &&
            scene->state.clock_seconds <= 59U) {
            reached_beyond_one_minute = true;
        }
        if (reached_beyond_one_minute && possession_outcomes >= 2U &&
            shot_launches >= 2U) {
            outcome = "long-horizon-legitimate-possession-outcomes";
            break;
        }
        if (scene->state.period != period_observed || scene->result_ready ||
            scene->state.phase == TECMO_GAMEPLAY_PHASE_COMPLETE) {
            outcome = "period-ended-before-two-outcomes";
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
            actor_action_before = scene->live_foundation.play_state
                .action_state_046e[holder];
            wait_before = scene->live_foundation.play_state
                .wait_counter[holder];
            route_active_before = scene->live_foundation.play_state
                .route_motion[holder].active;
            route_remaining_before = scene->live_foundation.play_state
                .route_motion[holder].remaining_timer;
            source_target_before =
                scene->live_foundation.source_target_valid[holder];
            deferred_before = scene->live_foundation.deferred[holder];
            automatic_admitted_before =
                ownership.automatic_selected_admitted;
            movement_action_before =
                scene->actors[holder].movement_action_state;
            movement_fraction_before =
                scene->actors[holder].movement_fractional_accumulator;
        } else {
            automatic_admitted_before = false;
        }
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral)) {
            update_failed = true;
            outcome = "scene-update-failed";
            break;
        }
    }
    if (!proof_render(&runtime, terminal_png_path, &terminal_hash)) goto done;
    fclose(trace);
    trace = NULL;
    result = reached_beyond_one_minute && possession_outcomes >= 2U &&
        legitimate_possession_outcomes == possession_outcomes &&
        shot_launches >= 2U && !no_effect_failure && mid_horizon_captured &&
        first_outcome_captured && first_outcome.normalized &&
        strcmp(proof_first_outcome_classification(&first_outcome),
               "jump-miss-generic-compatibility-handoff") == 0 &&
        promotion_0627.promoted && promotion_0488.promoted &&
        ownership_fixtures.controllerless_automatic &&
        ownership_fixtures.p1_direct_holder &&
        ownership_fixtures.p2_direct_holder &&
        ownership_fixtures.invalid_same_team_other_actor_unowned &&
        progression_059b.fetched_05a0_opcode2_to_05a5 &&
        !update_failed && !ownership_failure && !anchor_oob &&
        runtime.gameplay_scene.state.violation ==
            TECMO_GAMEPLAY_VIOLATION_NONE;
    (void)snprintf(
        message, message_size,
        "{\"schema\":\"tecmo.cpu-possession-proof/TGPH-4\","
        "\"passed\":%s,\"structured_state_authority\":true,"
        "\"screenshot_scope\":\"presentation-only\","
        "\"fixture\":\"COM VS COM controllerless setup feeding production inbound; deterministic clocks 24/50\","
        "\"outer_update_limit\":20000,\"updates_observed\":%u,"
        "\"outcome\":\"%s\",\"possession_outcomes\":%u,"
        "\"legitimate_possession_outcomes\":%u,"
        "\"shot_launches\":%u,\"selected_state0b_observed\":%s,"
        "\"reached_beyond_one_minute\":%s,"
        "\"no_effect_failure\":%s,\"max_no_effect_streak\":%u,"
        "\"scene_update_failed\":%s,\"terminal_clock\":[%u,%u,%u],"
        "\"ownership_failure\":%s,"
        "\"first_outcome_classification\":\"%s\","
        "\"first_shot\":{\"captured\":%s,\"kind\":%u,\"kind_name\":\"%s\","
        "\"outcome\":%u,\"outcome_name\":\"%s\",\"shooting_team\":%u,"
        "\"score_before\":[%u,%u],\"claimant_valid_before\":%s,"
        "\"claimant_serial_before\":%u},"
        "\"first_settlement\":{\"captured\":%s,\"possession_after\":%u,"
        "\"holder_after\":%u,\"score_after\":[%u,%u],"
        "\"scores_unchanged\":%s,\"claimant_valid_after\":%s,"
        "\"claimant_serial_after\":%u,\"claimant_unchanged_invalid\":%s,"
        "\"automatic_new_holder\":%s,\"cursor\":%u,\"state\":%u,"
        "\"action\":%u,\"wait\":%u,\"route_cleared\":%s,"
        "\"target_cleared\":%s,\"defer_cleared\":%s,\"normalized\":%s},"
        "\"selected_state0b_update\":%u,"
        "\"mid_horizon_update\":%u,"
        "\"ownership_fixtures\":{\"controllerless_automatic\":%s,"
        "\"p1_direct_holder\":%s,\"p2_direct_holder\":%s,"
        "\"invalid_same_team_other_actor_unowned\":%s},"
        "\"source_progression_059b\":{\"opcode3\":%s,"
        "\"state6_wait30_cursor05a0\":%s,\"countdown_ticks\":%u,"
        "\"state4_cursor05a0\":%s,\"opcode2_to05a5\":%s},"
        "\"inbound_promotion_0627\":{\"adversarial_fixture_valid\":%s,"
        "\"inbound_started\":%s,\"stale_suppressed_before_ai\":%s,"
        "\"catch_promoted_d7_state4_action18\":%s},"
        "\"inbound_promotion_0488\":{\"adversarial_fixture_valid\":%s,"
        "\"inbound_started\":%s,\"stale_suppressed_before_ai\":%s,"
        "\"catch_promoted_d7_state4_action18\":%s},"
        "\"violation_code\":%u,\"violation_name\":\"%s\","
        "\"anchor_oob\":%s,\"max_idle_frames\":%u,"
        "\"gate_excursion_count\":%u,"
        "\"pass_active_frame_count\":%u,\"max_pose_overhang\":%d,"
        "\"mid_horizon_frame_fnv1a32\":\"%08X\","
        "\"terminal_frame_fnv1a32\":\"%08X\"}",
        result ? "true" : "false", update, outcome, possession_outcomes,
        legitimate_possession_outcomes,
        shot_launches, selected_state0b_observed ? "true" : "false",
        reached_beyond_one_minute ? "true" : "false",
        no_effect_failure ? "true" : "false", max_no_effect_streak,
        update_failed ? "true" : "false",
        (unsigned)runtime.gameplay_scene.state.clock_minutes,
        (unsigned)runtime.gameplay_scene.state.clock_seconds,
        (unsigned)runtime.gameplay_scene.state.clock_divider,
        ownership_failure ? "true" : "false",
        proof_first_outcome_classification(&first_outcome),
        first_outcome.launch_captured ? "true" : "false",
        (unsigned)first_outcome.shot_kind,
        tecmo_gameplay_scene_shot_name(
            (TecmoGameplaySceneShotKind)first_outcome.shot_kind),
        (unsigned)first_outcome.shot_outcome,
        proof_shot_outcome_name(first_outcome.shot_outcome),
        (unsigned)first_outcome.shooting_team,
        (unsigned)first_outcome.score_before[TECMO_GAMEPLAY_TEAM_AWAY],
        (unsigned)first_outcome.score_before[TECMO_GAMEPLAY_TEAM_HOME],
        first_outcome.claimant_valid_before ? "true" : "false",
        (unsigned)first_outcome.claimant_serial_before,
        first_outcome.settlement_captured ? "true" : "false",
        (unsigned)first_outcome.possession_after,
        (unsigned)first_outcome.holder_after,
        (unsigned)first_outcome.score_after[TECMO_GAMEPLAY_TEAM_AWAY],
        (unsigned)first_outcome.score_after[TECMO_GAMEPLAY_TEAM_HOME],
        first_outcome.score_before[TECMO_GAMEPLAY_TEAM_AWAY] ==
                first_outcome.score_after[TECMO_GAMEPLAY_TEAM_AWAY] &&
            first_outcome.score_before[TECMO_GAMEPLAY_TEAM_HOME] ==
                first_outcome.score_after[TECMO_GAMEPLAY_TEAM_HOME]
            ? "true" : "false",
        first_outcome.claimant_valid_after ? "true" : "false",
        (unsigned)first_outcome.claimant_serial_after,
        !first_outcome.claimant_valid_before &&
                !first_outcome.claimant_valid_after &&
                first_outcome.claimant_serial_before ==
                    first_outcome.claimant_serial_after
            ? "true" : "false",
        first_outcome.automatic_new_holder ? "true" : "false",
        (unsigned)first_outcome.cursor_after,
        (unsigned)first_outcome.state_after,
        (unsigned)first_outcome.action_after,
        (unsigned)first_outcome.wait_after,
        first_outcome.route_cleared ? "true" : "false",
        first_outcome.target_cleared ? "true" : "false",
        first_outcome.defer_cleared ? "true" : "false",
        first_outcome.normalized ? "true" : "false",
        selected_state0b_update, mid_horizon_update,
        ownership_fixtures.controllerless_automatic ? "true" : "false",
        ownership_fixtures.p1_direct_holder ? "true" : "false",
        ownership_fixtures.p2_direct_holder ? "true" : "false",
        ownership_fixtures.invalid_same_team_other_actor_unowned
            ? "true" : "false",
        progression_059b.start_059b_opcode3 ? "true" : "false",
        progression_059b.installed_state6_wait30_cursor05a0
            ? "true" : "false",
        progression_059b.countdown_ticks,
        progression_059b.returned_state4_cursor05a0 ? "true" : "false",
        progression_059b.fetched_05a0_opcode2_to_05a5 ? "true" : "false",
        promotion_0627.adversarial_state_valid ? "true" : "false",
        promotion_0627.inbound_started ? "true" : "false",
        promotion_0627.stale_suppressed ? "true" : "false",
        promotion_0627.promoted ? "true" : "false",
        promotion_0488.adversarial_state_valid ? "true" : "false",
        promotion_0488.inbound_started ? "true" : "false",
        promotion_0488.stale_suppressed ? "true" : "false",
        promotion_0488.promoted ? "true" : "false",
        (unsigned)runtime.gameplay_scene.state.violation,
        tecmo_gameplay_violation_name(runtime.gameplay_scene.state.violation),
        anchor_oob ? "true" : "false", max_idle_frames,
        gate_excursion_count, pass_events,
        max_overhang, (unsigned)mid_horizon_hash,
        (unsigned)terminal_hash);
done:
    if (trace != NULL) fclose(trace);
    if (message != NULL && message_size != 0U && message[0] == '\0') {
        (void)snprintf(message, message_size,
                       "CPU possession proof setup or telemetry failed at %s",
                       setup_stage);
    }
    if (initialized) {
        tecmo_gameplay_scene_destroy(&runtime.gameplay_scene);
        tecmo_music_asset_shutdown(&runtime.music_asset);
    }
    free(permanent_block);
    free(transient_block);
    return result;
}
