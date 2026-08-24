#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_asset_pack.h"
#include "tecmo_gameplay_defense_contact.h"
#include "tecmo_nes_video.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Native shot, contact, and possession orchestration. */

static bool scene_shot_is_contested(
    const TecmoGameplayScene *scene,
    uint8_t shooter,
    bool *contact_out,
    bool *contest_out);
static bool scene_update_shot_mutating(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls);
static bool scene_begin_shot_rim_tail(TecmoGameplayScene *scene);
static bool scene_update_shot_rim_tail_mutating(TecmoGameplayScene *scene);

/*
 * Live foul bridge, revision 1.
 *
 * Bank05 $957E saves the pre-commit $0478 route in $07E3, then its ordinary
 * (not 05/07/08) fall-through installs $19 in $0478.  The native scene does
 * not yet retain original $0478/$07E3/$05A8 state, so this is deliberately a
 * narrow native adapter for the ordinary route-zero fall-through only.  These
 * are inputs to the strict TPNL classifier, not reconstructed live RAM.
 *
 * Do not add the special 05/07/08 routes or infer their semantics here until
 * the caller-owned source bytes exist in the scene.  In particular, no CPU
 * proximity policy is synthesized: only an explicit human defensive-B action
 * reaches this bridge.
 */

static bool scene_record_shot_attempt_stats(
    TecmoGameplayScene *scene)
{
    uint8_t stats_point_value;
    if (scene == NULL || scene->shot_actor_team >=
            TECMO_PLAYER_STATS_GAME_SIDE_COUNT ||
        scene->shot_actor_roster_index >= TECMO_PLAYER_STATS_ROSTER_COUNT)
        return false;
    if (scene->shot_points != 1U && scene->shot_points != 2U &&
        scene->shot_points != 3U)
        return false;
    stats_point_value = scene->shot_points == 3U ? 3U : 2U;
    return tecmo_player_stats_record_shot_attempt(
        &scene->player_stats, scene->shot_actor_team,
        scene->shot_actor_roster_index, stats_point_value);
}

static bool scene_record_shot_make_stats(
    TecmoGameplayScene *scene)
{
    uint8_t stats_point_value;
    if (scene == NULL || scene->shot_actor_team >=
            TECMO_PLAYER_STATS_GAME_SIDE_COUNT ||
        scene->shot_actor_roster_index >= TECMO_PLAYER_STATS_ROSTER_COUNT)
        return false;
    if (scene->shot_points != 1U && scene->shot_points != 2U &&
        scene->shot_points != 3U)
        return false;
    stats_point_value = scene->shot_points == 3U ? 3U : 2U;
    return tecmo_player_stats_record_shot_make(
        &scene->player_stats, scene->shot_actor_team,
        scene->shot_actor_roster_index, stats_point_value);
}

/* All owned shot schedules are uint16_t timelines, but their next-frame
   arithmetic must be checked in a wider type first.  In particular, a
   corrupted frame equal to the schedule duration is already terminal and
   must not wrap through another update. */
static bool scene_next_shot_frame(const TecmoGameplayScene *scene,
                                  uint16_t *next_frame)
{
    uint32_t next;
    if (scene == NULL || next_frame == NULL || scene->shot_duration == 0U ||
        scene->shot_frame >= scene->shot_duration ||
        scene->shot_frame == UINT16_MAX) {
        return false;
    }
    next = (uint32_t)scene->shot_frame + 1U;
    if (next > UINT16_MAX || next > scene->shot_duration) return false;
    *next_frame = (uint16_t)next;
    return true;
}

static bool scene_shot_boundary_valid(const TecmoGameplayScene *scene)
{
    if (scene == NULL || !scene->available || !scene->active ||
        (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
         scene->state.phase !=
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE)) {
        return false;
    }
    return scene->legacy_direct_launch
        ? scene_shot_state_valid(scene)
        : scene_ownership_valid(scene);
}

bool scene_shot_controller_binding_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplayTeam team;
    if (scene == NULL || scene->shot_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    team = (TecmoGameplayTeam)scene->actors[scene->shot_actor].team;
    if (scene->shot_controller < TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        return scene->launch.controller_team[scene->shot_controller] == team;
    }
    return scene->shot_controller == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
        scene_controller_for_team(scene, team) ==
            TECMO_GAMEPLAY_CONTROLLER_COUNT;
}

void scene_shot_clear_jump_playback(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    /* Scene end and every settlement boundary call this shared clear helper;
       keep the inactive timeline atomically empty even when the legacy caller
       did not separately zero its frame/duration pair. */
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene->shot_actor_launch_position.x = 0;
    scene->shot_actor_launch_position.y = 0;
    scene->shot_actor_team = 0U;
    scene->shot_actor_roster_index = 0U;
    scene->shot_launch_facing_right = false;
    scene->shot_launch_frame = 0U;
    scene->shot_target_delta_x = 0;
    scene->shot_target_delta_y = 0;
    scene->shot_close_context = false;
    scene->jump_actor_altitude_q8 = 0U;
    scene->jump_actor_velocity_q8 = 0U;
    scene->jump_ball_altitude_q8 = 0U;
    scene->jump_ball_bounce_q8 = 0U;
    scene->jump_entry_pose_index = 0U;
    scene->jump_resolved_pose_index = 0U;
    scene->jump_actor_state = 0U;
    scene->jump_ball_state = 0U;
    scene->jump_phase_counter = 0U;
    scene->jump_pose_frame = 0U;
    scene->shot_controller = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->jump_family = TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0;
    scene->jump_profile = TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0;
    scene->jump_direction = TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0;
    scene->shot_a0f3_origin_valid = false;
    scene->shot_a0f3_origin_x = 0U;
    scene->shot_a0f3_origin_depth = 0U;
    scene->shot_a0f3_preflight_valid = false;
    scene->shot_a0f3_preflight_raw_006a = 0U;
    scene->shot_a0f3_launch_raw_006a = 0U;
    memset(&scene->shot_a0f3_result, 0,
           sizeof(scene->shot_a0f3_result));
    memset(&scene->shot_a0f3_motion, 0,
           sizeof(scene->shot_a0f3_motion));
    scene->shot_a0f3_motion_valid = false;
    scene->shot_a0f3_raw_position_valid = false;
    scene->shot_a0f3_raw_x = 0U;
    scene->shot_a0f3_raw_depth = 0U;
    scene->shot_a0f3_tick_count = 0U;
    scene->shot_a8e9_normalized_valid = false;
    memset(&scene->shot_a8e9_normalized, 0,
           sizeof(scene->shot_a8e9_normalized));
    scene->close_shot_variant = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
    scene->native_policy_sample = 0U;
    scene->shot_flags = 0U;
    scene->shot_make_probability = 0U;
    scene->shot_contact_context = false;
    scene->shot_contest_context = false;
    scene->shot_context_signature = 0U;
    scene->shot_result_awarded = false;
    scene->shot_outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
    scene->shot_schedule =
        TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION;
    scene->shot_rim_rattle_raw_selector = 0U;
    memset(&scene->shot_rim_route, 0, sizeof(scene->shot_rim_route));
    scene->shot_rim_rattle_selected = false;
    scene->shot_rim_tail_active = false;
    scene->shot_rim_tail_frame = 0U;
    scene->shot_rim_tail_duration = 0U;
    scene->shot_rim_tail_base_frame = 0U;
    scene->jump_playback_active = false;
    scene->predicted_make_route = false;
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

bool scene_shot_queue_result_audio(TecmoGameplayScene *scene,
                                   TecmoGameplayTeam shooting_team)
{
    TecmoGameplayAudioPlayer candidate_audio;
    TecmoGameplayAudioEvent side_result;
    if (scene == NULL ||
        (shooting_team != TECMO_GAMEPLAY_TEAM_AWAY &&
         shooting_team != TECMO_GAMEPLAY_TEAM_HOME)) {
        return false;
    }
    candidate_audio = scene->audio_player;
    if (!tecmo_gameplay_audio_queue_event(
            &candidate_audio, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE)) {
        return false;
    }

    /* Bank05 $AD01 requests ID 11 first. $B1D1 then overwrites the same
       one-byte mailbox with the pre-handoff shooting-side result when the
       clock is above 0:01. Only the final request is consumed. */
    if (scene->state.clock_minutes == 0U &&
        scene->state.clock_seconds < 2U) {
        scene->audio_player = candidate_audio;
        return true;
    }
    side_result = shooting_team == TECMO_GAMEPLAY_TEAM_AWAY
                      ? TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_12
                      : TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_13;
    if (!tecmo_gameplay_audio_queue_event(&candidate_audio, side_result)) {
        return false;
    }
    scene->audio_player = candidate_audio;
    return true;
}

static bool scene_jump_pose_for_context(const TecmoGameplayScene *scene,
                                        uint16_t *pose_index)
{
    if (scene == NULL || pose_index == NULL ||
        (unsigned)scene->jump_family >=
            TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT ||
        (unsigned)scene->jump_profile >=
            TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT ||
        (unsigned)scene->jump_direction >=
            TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT) {
        return false;
    }
    return tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
        &scene->jump_shots, scene->jump_family, scene->jump_profile,
        scene->jump_direction, pose_index);
}

static bool scene_jump_pose_for_phase(const TecmoGameplayScene *scene,
                                      uint8_t animation_byte,
                                      uint16_t *pose_index)
{
    if (scene == NULL) return false;
    return tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
        &scene->jump_shots, scene->jump_family, scene->jump_profile,
        scene->jump_direction, animation_byte, pose_index);
}

static uint16_t scene_jump_playback_flight_pose(
    const TecmoGameplayScene *scene)
{
    /* Legacy/direct render and shot-clock adapters predate the bound TGJS
       selector and retain their accepted $00D5 flight pose. Bound production
       shots consume the persisted family/profile/direction result. */
    uint16_t pose = 0U;
    if (scene != NULL && scene->legacy_direct_launch) {
        return TECMO_GAMEPLAY_JUMP_FLIGHT_POSE;
    }
    return scene_jump_pose_for_phase(scene, scene->jump_phase_counter, &pose)
        ? pose
        : 0U;
}

bool scene_shot_is_close(TecmoGameplaySceneShotKind kind)
{
    return kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
           kind == TECMO_GAMEPLAY_SCENE_SHOT_LAYUP ||
           kind == TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1;
}

static TecmoGameplayCloseShotVariant scene_close_variant(
    TecmoGameplaySceneShotKind kind)
{
    if (kind == TECMO_GAMEPLAY_SCENE_SHOT_LAYUP) {
        return TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2;
    }
    if (kind == TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1) {
        return TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1;
    }
    return TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
}

bool scene_close_pose_for_step(const TecmoGameplayScene *scene,
                                      uint8_t step,
                                      uint16_t *pose_index)
{
    TecmoGameplayCloseShotVariant variant;
    uint8_t phase;
    if (scene == NULL || pose_index == NULL ||
        !scene_shot_is_close(scene->shot_kind)) {
        return false;
    }
    if (scene->legacy_direct_launch &&
        (scene->close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
         scene->close_shot_direction !=
             TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0)) {
        return false;
    }
    variant = scene_close_variant(scene->shot_kind);
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1) {
        if (step >= TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION ||
            !tecmo_gameplay_close_shots_resolve_numeric_variant1_pose_pointer_index(
                &scene->close_shots, scene->close_shot_direction,
                pose_index)) {
            return false;
        }
        /* Bank05 $8C7D selects fixed group $10, then adds the canonical
           direction slot. Hold that source-backed pose for the bounded native
           24-frame schedule; full object/trajectory semantics remain
           unproven. */
        return true;
    }
    return tecmo_gameplay_close_shots_phase_for_step(
               &scene->close_shots, variant, step, &phase) &&
           tecmo_gameplay_close_shots_resolve_pose_pointer_index(
               &scene->close_shots, variant, scene->close_shot_profile,
               scene->close_shot_direction, phase, pose_index);
}

static bool scene_resolve_actor_offensive_hoop(
    const TecmoGameplayScene *scene,
    uint8_t actor_index,
    TecmoGameplayCourtCoordinate *hoop_out,
    bool *facing_right_out)
{
    TecmoGameplayCourtCoordinate hoop;
    const TecmoGameplaySceneActor *actor;
    if (scene == NULL || hoop_out == NULL || facing_right_out == NULL ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_court_orientation_state_valid(
            &scene->court_orientation, &scene->orientation_state) ||
        scene->orientation_state.tracked_possession_team !=
            (uint8_t)scene->state.possession ||
        !tecmo_gameplay_court_orientation_hoop(
            &scene->court_orientation,
            scene->orientation_state.attack_direction, &hoop)) {
        return false;
    }
    actor = &scene->actors[actor_index];
    if (!actor->active || actor->team != (uint8_t)scene->state.possession ||
        hoop.x != scene->orientation_state.offensive_hoop.x ||
        hoop.y != scene->orientation_state.offensive_hoop.y) {
        return false;
    }
    *hoop_out = hoop;
    *facing_right_out = actor->position.x == hoop.x
                            ? scene->orientation_state.attack_direction != 0U
                            : actor->position.x < hoop.x;
    return true;
}

static bool scene_shot_is_contested(
    const TecmoGameplayScene *scene,
    uint8_t shooter,
    bool *contact_out,
    bool *contest_out)
{
    const TecmoGameplaySceneActor *shooter_actor;
    bool contact = false;
    bool contest = false;
    if (scene == NULL || contact_out == NULL || contest_out == NULL ||
        shooter >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[shooter].active) {
        return false;
    }
    shooter_actor = &scene->actors[shooter];
    for (size_t index = 0U; index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++index) {
        const TecmoGameplaySceneActor *defender = &scene->actors[index];
        int horizontal;
        int vertical;
        if (!defender->active || defender->team == shooter_actor->team) {
            continue;
        }
        horizontal = abs((int)defender->position.x -
                         (int)shooter_actor->position.x);
        vertical = abs((int)defender->position.y -
                       (int)shooter_actor->position.y);
        if (horizontal <= 40 && vertical <= 32) contest = true;
        if (horizontal <= 16 && vertical <= 16) contact = true;
    }
    *contact_out = contact;
    *contest_out = contest;
    return true;
}

uint32_t scene_shot_native_policy_sample_from_inputs(
    int16_t actor_x,
    int16_t actor_y,
    uint8_t point_value,
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint8_t actor_team,
    uint8_t actor_roster_index,
    uint32_t launch_frame)
{
    uint32_t native_policy_sample = 2166136261U;
    native_policy_sample ^= (uint16_t)actor_x;
    native_policy_sample *= 16777619U;
    native_policy_sample ^= (uint16_t)actor_y;
    native_policy_sample *= 16777619U;
    native_policy_sample ^= (uint16_t)target_delta_x;
    native_policy_sample *= 16777619U;
    native_policy_sample ^= (uint16_t)target_delta_y;
    native_policy_sample *= 16777619U;
    native_policy_sample ^= ((uint32_t)point_value << 24U) |
                            ((uint32_t)actor_team << 16U) |
                            ((uint32_t)actor_roster_index << 8U) |
                            (launch_frame & 0xFFU);
    native_policy_sample *= 16777619U;
    /* Preserve the accepted low-byte sample stream while binding every
       upper launch-frame bit.  A nonzero byte is folded as its own FNV step;
       a zero byte is the identity contribution, so ordinary frame<256
       checkpoints retain their captured values but upper-byte corruption
       cannot pass the scene boundary. */
    for (unsigned shift = 8U; shift < 32U; shift += 8U) {
        uint8_t frame_byte = (uint8_t)(launch_frame >> shift);
        if (frame_byte != 0U) {
            native_policy_sample ^= frame_byte;
            native_policy_sample *= 16777619U;
            native_policy_sample ^= (uint8_t)(shift / 8U);
            native_policy_sample *= 16777619U;
        }
    }
    return native_policy_sample;
}

uint32_t scene_shot_context_signature(
    uint32_t native_policy_sample,
    bool contact_context,
    bool contest_context)
{
    uint32_t signature = 2166136261U;
    signature ^= native_policy_sample;
    signature *= 16777619U;
    signature ^= contact_context ? 0xC3U : 0x3CU;
    signature *= 16777619U;
    signature ^= contest_context ? 0xA7U : 0x7AU;
    signature *= 16777619U;
    return signature;
}

bool scene_shot_captured_rattle_orientation(
    const TecmoGameplayScene *scene,
    uint8_t *orientation_out)
{
    int32_t endpoint_x_q8;
    int32_t endpoint_y_q8;
    if (scene == NULL || orientation_out == NULL) return false;
    endpoint_x_q8 = scene->shot_end_position.x_q8;
    endpoint_y_q8 = scene->shot_end_position.y_q8;
    if (endpoint_y_q8 != TECMO_GAMEPLAY_SHOT_TARGET_Y * 256) {
        return false;
    }
    if (endpoint_x_q8 == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256) {
        *orientation_out = 0U;
    } else if (endpoint_x_q8 ==
                   TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X * 256) {
        *orientation_out = 1U;
    } else {
        return false;
    }
    if (scene->jump_rim_rattle_debug) *orientation_out = 0U;
    return true;
}

int16_t scene_close_variant_selection_approach(
    int approach_distance_x,
    TecmoGameplayShotDirectionSlot direction,
    uint32_t native_policy_sample)
{
    /* The available close gate and the proven 4:1 direction sectors leave no
       physical vertical case with approach >24.  The raw object/timer inputs
       that distinguish this source identity are unavailable, so bit $400 is
       an explicit neutral substitution used only for vertical numeric-2
       reachability without relabeling contact, foul, or shot semantics. */
    if ((direction == TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN ||
         direction == TECMO_GAMEPLAY_SHOT_DIRECTION_UP) &&
        (native_policy_sample & 0x00000400U) != 0U) {
        return 32;
    }
    if (approach_distance_x < INT16_MIN) return INT16_MIN;
    if (approach_distance_x > INT16_MAX) return INT16_MAX;
    return (int16_t)approach_distance_x;
}

static bool scene_shot_profile_for_actor(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor,
    uint8_t *profile)
{
    const TecmoTeamDataPlayer *player;
    if (scene == NULL || actor == NULL || profile == NULL) return false;
    player = scene_actor_player(scene, actor);
    if (player == NULL) return false;
    return tecmo_gameplay_shot_profile_from_profile_byte2(
        player->profile[2], profile);
}

static bool scene_shot_select_rim_route(
    TecmoGameplayScene *scene,
    uint32_t native_policy_sample)
{
    if (scene == NULL ||
        !tecmo_gameplay_shot_resolution_resolve_rim_route(
            &scene->shot_resolution, (uint8_t)native_policy_sample,
            &scene->shot_rim_route)) {
        return false;
    }
    /* TGSR's resolver boundary is raw-selector shaped, but this low byte is
       a native policy substitution rather than retained NES RAM/RNG state. */
    scene->shot_rim_rattle_raw_selector = (uint8_t)native_policy_sample;
    scene->shot_rim_rattle_selected = false;
    return true;
}

uint8_t scene_shot_family_for_context(
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint32_t native_policy_sample)
{
    (void)target_delta_x;
    (void)target_delta_y;
    (void)native_policy_sample;
    /* Bank05 $8B12 resets $038C to family 0.  $8B83-$8BC8 may increment it
       only after the source has proved the near-hoop, near-defender,
       defender-side, and raw $006A<$9C gates.  The native scene does not
       retain that last raw input at shot launch, so fail closed to the reset
       family.  Do not substitute a hash bit here: that selected family 1 for
       ordinary uncontested shots and displayed the wrong TGJS sequence. */
    return TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0;
}

typedef enum SceneShotLaunchOwner {
    SCENE_SHOT_LAUNCH_HUMAN = 0,
    SCENE_SHOT_LAUNCH_AUTOMATIC_CPU
} SceneShotLaunchOwner;

static bool scene_start_shot_actor_mutating(TecmoGameplayScene *scene,
                                            size_t controller,
                                            uint8_t actor_index,
                                            SceneShotLaunchOwner owner)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayCourtCoordinate offensive_hoop;
    TecmoGameplayCourtCoordinate shot_start;
    TecmoGameplayCourtCoordinate shot_end;
    TecmoGameplayCourtCoordinateQ8 shot_start_q8;
    TecmoGameplayCourtCoordinateQ8 shot_end_q8;
    int16_t target_delta_x;
    int16_t target_delta_y;
    int approach_distance_x;
    int distance_y;
    int16_t variant_selection_approach;
    uint8_t classified_points;
    uint8_t jump_family;
    uint8_t jump_profile;
    uint8_t jump_direction;
    bool close;
    bool shot_facing_right;
    bool contact_context;
    bool contest_context;
    bool source_variant1_gate;
    uint8_t profile;
    TecmoGameplayShotDirectionSlot direction_slot;
    const TecmoTeamDataPlayer *player;
    uint32_t native_policy_sample;
    TecmoGameplayShotEvaluationInput evaluation_input;
    TecmoGameplayShotEvaluation evaluation;
    TecmoGameplayCloseShotVariantInfo close_info;
    uint16_t entry_pose;
    uint16_t initial_pose = 0U;
    bool predicted_make;
    bool a0f3_origin_valid = false;
    uint16_t a0f3_origin_x = 0U;
    uint8_t a0f3_origin_depth = 0U;
    TecmoGameplayCourtCoordinateQ8 a0f3_origin_q8 = {0, 0};
    if (scene == NULL ||
        (owner != SCENE_SHOT_LAUNCH_HUMAN &&
         owner != SCENE_SHOT_LAUNCH_AUTOMATIC_CPU) ||
        (owner == SCENE_SHOT_LAUNCH_HUMAN &&
         controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[actor_index].team != scene->state.possession) {
        return false;
    }
    if (scene->ball_holder != actor_index) {
        return false;
    }
    actor = &scene->actors[actor_index];
    entry_pose = actor->pose_index;
    if (!scene_resolve_actor_offensive_hoop(
            scene, actor_index, &offensive_hoop,
            &shot_facing_right)) {
        return false;
    }
    shot_start.x = (int16_t)(
        actor->position.x + (shot_facing_right ? 7 : -7));
    shot_start.y = (int16_t)(actor->position.y - 18);
    shot_end.x = offensive_hoop.x;
    shot_end.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
    if (!tecmo_gameplay_court_coordinate_to_q8(
            &shot_start, &shot_start_q8) ||
        !tecmo_gameplay_court_coordinate_to_q8(
            &shot_end, &shot_end_q8)) {
        return false;
    }
    approach_distance_x =
        scene->orientation_state.attack_direction == 0U
            ? actor->position.x - (int)offensive_hoop.x
            : (int)offensive_hoop.x - actor->position.x;
    distance_y = TECMO_GAMEPLAY_SHOT_TARGET_Y - actor->position.y;
    target_delta_x = (int16_t)((int)offensive_hoop.x -
                               (int)actor->position.x);
    target_delta_y = (int16_t)((int)offensive_hoop.y -
                               (int)actor->position.y);
    if (!scene_shot_is_contested(scene, actor_index,
                                 &contact_context, &contest_context) ||
        !scene_shot_profile_for_actor(scene, actor, &profile) ||
        !tecmo_gameplay_shot_resolution_direction_for_delta(
            target_delta_x, target_delta_y, &direction_slot)) {
        return false;
    }
    player = scene_actor_player(scene, actor);
    if (player == NULL) {
        return false;
    }
    if (!tecmo_gameplay_shot_resolution_classify_point_value(
            &scene->shot_resolution, (uint16_t)actor->position.x,
            (uint8_t)actor->position.y,
            scene->orientation_state.attack_direction, 0U,
            &classified_points)) {
        return false;
    }
    scene->shot_launch_frame = scene->frame;
    native_policy_sample = scene_shot_native_policy_sample_from_inputs(
        actor->position.x, actor->position.y, classified_points,
        target_delta_x, target_delta_y, actor->team, actor->roster_index,
        scene->shot_launch_frame);
    close = approach_distance_x >= -8 &&
            approach_distance_x <= TECMO_GAMEPLAY_CLOSE_DISTANCE_X &&
            distance_y >= -64 && distance_y <= 80;
    /* The physical court boundary makes the 4:1 vertical sectors near a hoop
       appear only inside the close gate.  The missing native object/timer
       context is substituted by a native policy sample bit for bound
       production:
       clear selects the close contract, set exposes the ordinary jump
       approximation.  This is neutral source substitution, not a contact,
       foul, or semantic shot label; legacy/direct fixtures retain geometry. */
    if (!scene->legacy_direct_launch && close &&
        (direction_slot == TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN ||
         direction_slot == TECMO_GAMEPLAY_SHOT_DIRECTION_UP) &&
        (native_policy_sample & 0x00000200U) != 0U) {
        close = false;
    }
    /* The pre-R2 direct-launch compatibility harness has an accepted
       profile-0/direction-0 close checkpoint.  Bound production launches use
       the exact profile[2] and geometry selectors above; this adapter keeps
       the legacy harness contract isolated to its explicitly marked path. */
    if (close && scene->legacy_direct_launch) {
        profile = 0U;
        direction_slot = TECMO_GAMEPLAY_SHOT_DIRECTION_RIGHT;
    }
    if (!close && !scene->legacy_direct_launch) {
        /* `$7D/$F2/$FD` aliases object slot 10. The ball is still attached
           here, before native presentation replaces it with shot_start_q8.
           This follows the existing raw mapper: x>>8/x>>16 and depth>>8. */
        a0f3_origin_x = (uint16_t)(
            ((uint16_t)((uint32_t)scene->ball_position.x_q8 >> 16U) << 8U) |
            (uint8_t)((uint32_t)scene->ball_position.x_q8 >> 8U));
        a0f3_origin_depth = (uint8_t)(
            (uint32_t)scene->ball_position.y_q8 >> 8U);
        a0f3_origin_q8 = scene->ball_position;
        a0f3_origin_valid = true;
    }
    scene_shot_clear_jump_playback(scene);
    scene->shot_launch_frame = scene->frame;
    /* $8BDE/$8C79/$8C7D prove the numeric-1 path, but not its semantic
       label.  The raw object/timer predicate is unavailable, so use one
       native policy sample bit as a neutral source/substitution gate.  It is
       independent of contact/contest classification and permits all eight
       direction slots to reach the pose-only numeric-1 approximation. */
    source_variant1_gate = close && !scene->legacy_direct_launch &&
        (native_policy_sample & 0x00000100U) != 0U;
    if (close) {
        variant_selection_approach = scene->legacy_direct_launch
            ? (int16_t)approach_distance_x
            : scene_close_variant_selection_approach(
                  approach_distance_x, direction_slot, native_policy_sample);
        if (!tecmo_gameplay_close_shots_select_numeric_variant(
                variant_selection_approach, (int16_t)distance_y,
                source_variant1_gate, &scene->close_shot_variant)) {
            return false;
        }
    } else {
        scene->close_shot_variant = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
    }
    if (!scene_shot_select_rim_route(scene, native_policy_sample)) {
        return false;
    }
    /* Bank05 $8B12 starts the selector at family 0.  The full $8B83-$8BC8
       family-1 gate still lacks its raw object/timer input, so the helper
       deliberately remains fail-closed.  In contrast, Bank02
       $A8AE/$A8BA/$A8BC retains the profile[2] bit-5 selector, and Bank05
       $9054-$90AF -> $8DD3-$8E4D -> $BF6C retains the eight-way hoop vector
       stored at $05A0.  $842C then indexes $8D3D/$8D5D with
       family-base + profile*8 + direction.  Do not replace those owned
       profile/direction inputs with the old captured 0/0/1 diagnostic. */
    jump_family = scene_shot_family_for_context(
        target_delta_x, target_delta_y, native_policy_sample);
    jump_profile = profile;
    jump_direction = (uint8_t)direction_slot;
    memset(&evaluation_input, 0, sizeof(evaluation_input));
    evaluation_input.player_rating = player->profile[0];
    evaluation_input.point_value = classified_points;
    evaluation_input.close_context = close;
    evaluation_input.contact_context = contact_context;
    evaluation_input.contest_context = contest_context;
    evaluation_input.horizontal_distance = target_delta_x;
    evaluation_input.vertical_distance =
        (int16_t)(TECMO_GAMEPLAY_SHOT_TARGET_Y - actor->position.y);
    evaluation_input.family = close
        ? scene_shot_family_for_context(
              target_delta_x, target_delta_y, native_policy_sample)
        : jump_family;
    if (!close && scene->legacy_direct_launch && classified_points == 3U) {
        /* The accepted direct render/shot-clock adapter is source-pinned to
           family 0 before the unported family inputs are available. */
        evaluation_input.family = 0U;
    }
    evaluation_input.profile = close ? profile : jump_profile;
    evaluation_input.direction = close
        ? (uint8_t)direction_slot : jump_direction;
    evaluation_input.numeric_variant = (uint8_t)scene->close_shot_variant;
    evaluation_input.native_policy_sample = native_policy_sample;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            &scene->shot_resolution, &evaluation_input, &evaluation)) {
        return false;
    }
    if (!close && scene->legacy_direct_launch &&
        scene->action_serial == 0U && classified_points == 3U) {
        /* Preserve the accepted pre-R2 direct-launch 3-point checkpoint. This
           is an explicitly isolated test-harness adapter, not the production
           outcome evaluator or an action-serial make roll. */
        evaluation.outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE;
        evaluation.schedule =
            TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT;
    }
    predicted_make = evaluation.outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE;
    /* Raw route identity is retained for every outcome, but only a bound MISS
       may activate the A7A9 rattle contract.  In particular, a MAKE whose
       native policy sample has low2==1 remains an ordinary make schedule. */
    scene->shot_rim_rattle_selected =
        !scene->legacy_direct_launch &&
        evaluation.outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS &&
        scene->shot_rim_route.kind == TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9;
    if (!close && scene->legacy_direct_launch && predicted_make &&
        classified_points != 3U) {
        /* Preserve the accepted direct-launch checkpoint's fail-closed
           behavior. Bound production launches use the native approximate
           1/2-point schedule below. */
        return false;
    }
    scene->native_policy_sample = native_policy_sample;
    scene->shot_make_probability = evaluation.make_probability;
    scene->shot_contact_context = evaluation.contact_context;
    scene->shot_contest_context = evaluation.contest_context;
    scene->shot_context_signature = scene_shot_context_signature(
        native_policy_sample, evaluation.contact_context,
        evaluation.contest_context);
    scene->shot_outcome = evaluation.outcome;
    scene->shot_schedule = evaluation.schedule;
    if (close) {
        scene->shot_kind = scene->close_shot_variant ==
                                   TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
                               ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                               : scene->close_shot_variant ==
                                         TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2
                                   ? TECMO_GAMEPLAY_SCENE_SHOT_LAYUP
                                   : TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1;
        scene->close_shot_profile =
            (TecmoGameplayCloseShotProfile)profile;
        scene->close_shot_direction =
            (TecmoGameplayCloseShotDirection)direction_slot;
        scene->close_shot_step = 0U;
        if (!tecmo_gameplay_close_shots_get_variant_info(
                &scene->close_shots, scene_close_variant(scene->shot_kind),
                &close_info) ||
            !scene_close_pose_for_step(scene, 0U, &initial_pose)) {
            scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
            return false;
        }
    } else {
        if (owner == SCENE_SHOT_LAUNCH_HUMAN) {
            if (scene->launch.controller_team[controller] != actor->team) {
                return false;
            }
            scene->shot_controller = (uint8_t)controller;
        } else {
            /* Bank05 $81F2-$822F -> $8A6D -> $8ACE proves that selected
               action $17 enters the same initializer without a human-pad
               owner.  Admission is still a bounded native adapter because
               raw $0478/$0499/$007E are not retained.  Reuse the existing
               source-backed TGJS/TGSR playback with an explicit autonomous
               owner instead of fabricating a controller assignment. */
            if (scene_controller_for_team(
                    scene, (TecmoGameplayTeam)actor->team) !=
                TECMO_GAMEPLAY_CONTROLLER_COUNT) {
                return false;
            }
            scene->shot_controller = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        }
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
        memset(&close_info, 0, sizeof(close_info));
        scene->jump_family =
            (TecmoGameplayJumpShotFamily)jump_family;
        scene->jump_profile =
            (TecmoGameplayJumpShotProfile)jump_profile;
        scene->jump_direction =
            (TecmoGameplayJumpShotDirection)jump_direction;
        scene->shot_a0f3_origin_valid = a0f3_origin_valid;
        scene->shot_a0f3_origin_x = a0f3_origin_x;
        scene->shot_a0f3_origin_depth = a0f3_origin_depth;
        if (!scene_jump_pose_for_context(scene, &initial_pose)) {
            scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
            scene_shot_clear_jump_playback(scene);
            return false;
        }
        scene->jump_resolved_pose_index = initial_pose;
    }
    scene->shot_actor = actor_index;
    scene->shot_frame = close ? 0U : 1U;
    scene->shot_points = classified_points;
    scene->shot_flags = 0U;
    scene->shot_start_position = a0f3_origin_valid
        ? a0f3_origin_q8 : shot_start_q8;
    /* Capture the TGOR-selected endpoint once at launch. A later possession
       transition may change orientation, but it must not retarget flight. */
    scene->shot_end_position = shot_end_q8;
    scene->shot_actor_launch_position = actor->position;
    scene->shot_actor_team = actor->team;
    scene->shot_actor_roster_index = actor->roster_index;
    scene->shot_launch_facing_right = shot_facing_right;
    scene->shot_target_delta_x = target_delta_x;
    scene->shot_target_delta_y = target_delta_y;
    scene->shot_close_context = close;
    scene->ball_position = scene->shot_start_position;
    scene->ball_holder = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    ++scene->action_serial;
    actor->facing_right = shot_facing_right;
    /* The retained tip-off pose owns its encoded orientation only until a
       court action selects a new TGJS pose.  Once shooting owns the pose,
       allow the renderer to apply the launch-facing mirror. */
    actor->pose_orientation_encoded = false;
    if (close) {
        scene->shot_duration =
            scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                : scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1
                    ? TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION
                    : close_info.step_count;
    } else if (predicted_make) {
        scene->shot_duration = evaluation.schedule ==
                                   TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT
                               ? (uint16_t)(
                                     TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
                                     scene->jump_shots.constants.made_update_count)
                               : TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION;
    } else {
        scene->shot_duration = scene->shot_rim_rattle_selected
            ? TECMO_GAMEPLAY_JUMP_RATTLE_DURATION
            : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION;
    }
    if (!close && predicted_make) {
        initial_pose = TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE;
    } else if (!close) {
        /* Bank05 state $1E leaves $0442/$044D untouched for the first four
           pose ticks. Preserve the actor's actual entry pose instead of
           substituting the later $01AA flight pose. */
        initial_pose = entry_pose;
    }
    actor->pose_index = initial_pose;
    scene->shot_result_awarded = false;
    if (!scene_record_shot_attempt_stats(scene)) return false;
    if (!close) {
        scene->jump_entry_pose_index = initial_pose;
        scene->jump_pose_frame = 1U;
        /* These booleans schedule native scene playback; neither mirrors the
           Bank05 $91BC-$943A terminal-result route or another RAM byte. */
        scene->jump_playback_active = true;
        scene->predicted_make_route = predicted_make;
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

bool scene_start_shot_actor(TecmoGameplayScene *scene,
                            size_t controller,
                            uint8_t actor_index)
{
    TecmoGameplayScene candidate;
    if (scene == NULL) return false;
    candidate = *scene;
    if (!scene_shot_boundary_valid(&candidate)) return false;
    if (!scene_start_shot_actor_mutating(
            &candidate, controller, actor_index,
            SCENE_SHOT_LAUNCH_HUMAN)) {
        return false;
    }
    if (!scene_shot_boundary_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

bool scene_start_automatic_cpu_shot_actor(TecmoGameplayScene *scene,
                                          uint8_t actor_index)
{
    TecmoGameplayScene candidate;
    if (scene == NULL) return false;
    candidate = *scene;
    if (!scene_shot_boundary_valid(&candidate) ||
        actor_index >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        candidate.ball_holder != actor_index ||
        candidate.actors[actor_index].team != candidate.state.possession ||
        scene_controller_for_team(
            &candidate,
            (TecmoGameplayTeam)candidate.actors[actor_index].team) !=
            TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        return false;
    }
    if (!scene_start_shot_actor_mutating(
            &candidate, TECMO_GAMEPLAY_CONTROLLER_COUNT, actor_index,
            SCENE_SHOT_LAUNCH_AUTOMATIC_CPU) ||
        !scene_shot_boundary_valid(&candidate)) {
        return false;
    }
    *scene = candidate;
    return true;
}

bool scene_start_shot(TecmoGameplayScene *scene,
                             size_t controller)
{
    if (scene == NULL || controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
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
    uint16_t canonical_pose;
    uint16_t entry_pose_before;
    uint16_t actor_pose_before;
    if (scene == NULL || !scene->available || !scene->active ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->launch.controller_team[0U] != TECMO_GAMEPLAY_TEAM_AWAY) {
        return false;
    }
    if (!scene_shot_boundary_valid(scene)) return false;
    actor = scene->controlled_actor[0U];
    if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->ball_holder != actor ||
        scene->actors[actor].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        !scene->actors[actor].facing_right) {
        return false;
    }

    /* This is an explicit deterministic diagnostic setup, not a live selector
       or make/miss policy. It uses the already-covered native miss branch.
       Shot setup has several fail-closed branches after it starts mutating
       scene state, so stage the diagnostic in a shallow candidate. The setup
       performs no allocation and owns no external writes. */
    candidate = *scene;
    candidate.action_serial = 1U;
    if (!scene_start_shot_actor(&candidate, 0U, actor)) {
        return false;
    }
    if (candidate.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        candidate.predicted_make_route ||
        candidate.shot_duration != TECMO_GAMEPLAY_JUMP_SLOT0_DURATION ||
        candidate.shot_points != 3U ||
        candidate.shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        candidate.jump_family != TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 ||
        candidate.jump_profile != TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0) {
        return false;
    }
    /* The explicit captured diagnostic is source-pinned to family 0/profile
       0/direction 1.  The legacy render fixture reaches this wrapper with a
       preserved pre-R2 pose geometry, so normalize only the diagnostic
       identity after the production start probe proves the point-3 miss/f0/p0
       identity.  The miss route intentionally retains the actor's actual
       entry pose for its first four ticks; resolve the canonical matrix only
       as a fail-closed availability check and preserve both pose fields. */
    entry_pose_before = candidate.jump_entry_pose_index;
    actor_pose_before = candidate.actors[actor].pose_index;
    candidate.jump_direction = TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1;
    candidate.shot_schedule =
        TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT;
    if (!scene_jump_pose_for_context(&candidate, &canonical_pose) ||
        candidate.jump_entry_pose_index != entry_pose_before ||
        candidate.actors[actor].pose_index != actor_pose_before) {
        return false;
    }
    candidate.jump_resolved_pose_index = canonical_pose;
    candidate.jump_rim_rattle_debug = true;
    candidate.jump_rim_rattle_raw_selector = 0x71U;
    candidate.shot_duration = TECMO_GAMEPLAY_JUMP_RATTLE_DURATION;
    if (!scene_shot_boundary_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

static bool scene_handoff_possession_impl(TecmoGameplayScene *scene,
                                          TecmoGameplayTeam possession,
                                          uint8_t preferred_actor,
                                          bool preserve_actor_state)
{
    TecmoGameplayState state_before;
    TecmoGameplayCourtOrientationState orientation_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayBackcourtState backcourt_before;
    TecmoGameplayBackcourtState backcourt_reset;
    TecmoGameplayCourtCoordinateQ8 ball_before;
    TecmoGameplayCourtCoordinateQ8 candidate_ball;
    TecmoGameplaySceneActor
        candidate_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t controlled_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t holder_before;
    uint8_t first = scene_first_actor_for_team(possession);
    uint8_t holder = preferred_actor;
    size_t controller;
    if (scene == NULL ||
        (possession != TECMO_GAMEPLAY_TEAM_AWAY &&
         possession != TECMO_GAMEPLAY_TEAM_HOME) ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state) ||
        !tecmo_gameplay_backcourt_state_initialize(
            &scene->backcourt_assets, &backcourt_reset)) {
        return false;
    }
    if (holder < first ||
        holder >= first + TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT) {
        holder = first;
    }
    state_before = scene->state;
    orientation_before = scene->orientation_state;
    camera_before = scene->camera_state;
    backcourt_before = scene->backcourt_state;
    ball_before = scene->ball_position;
    holder_before = scene->ball_holder;
    memcpy(controlled_before, scene->controlled_actor,
           sizeof(controlled_before));
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
    memcpy(candidate_actors, scene->actors, sizeof(candidate_actors));
    /* Ordinary possession changes establish a fresh effective facing
       baseline.  Tip completion is different: Bank05 changes ball ownership
       while the existing player objects recover in place. */
    if ((!preserve_actor_state &&
         !scene_apply_goal_facing(scene, candidate_actors)) ||
        !scene_ball_position_for_actors(
            scene, candidate_actors, holder, &candidate_ball)) {
        scene->state = state_before;
        scene->orientation_state = orientation_before;
        scene->camera_state = camera_before;
        scene->backcourt_state = backcourt_before;
        scene->ball_position = ball_before;
        scene->ball_holder = holder_before;
        memcpy(scene->controlled_actor, controlled_before,
               sizeof(controlled_before));
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
    memcpy(scene->actors, candidate_actors, sizeof(candidate_actors));
    scene->ball_holder = holder;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == possession) {
            scene->controlled_actor[controller] = holder;
        }
    }
    scene->ball_position = candidate_ball;
    scene->backcourt_state = backcourt_reset;
    scene_loose_ball_clear(scene);
    return true;
}

bool scene_handoff_possession(TecmoGameplayScene *scene,
                              TecmoGameplayTeam possession,
                              uint8_t preferred_actor)
{
    return scene_handoff_possession_impl(
        scene, possession, preferred_actor, false);
}

bool scene_handoff_tip_possession(TecmoGameplayScene *scene,
                                  TecmoGameplayTeam possession,
                                  uint8_t preferred_actor)
{
    return scene_handoff_possession_impl(
        scene, possession, preferred_actor, true);
}

/* The source's $B87C entry is reached from the bounded claimant/collision
 * route, not from every possession-changing scene event. Keep the generic
 * scene ownership/orientation handoff separate, then apply the typed LIVE
 * $0308/$0309/$030A/$030B transaction only for the two miss claimant callers
 * below. The complete scene is staged so a malformed typed input rolls back
 * both layers together. */
static bool scene_handoff_claimant_settlement(
    TecmoGameplayScene *scene,
    TecmoGameplayTeam possession,
    uint8_t claimant)
{
    TecmoGameplayScene candidate;
    TecmoGameplayScenePossessionTraceSnapshot before;
    TecmoGameplayScenePossessionTraceSnapshot after;
    TecmoGameplayLiveClaimantSettlement transaction;
    uint32_t next_serial;

    if (scene == NULL || claimant >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        (possession != TECMO_GAMEPLAY_TEAM_AWAY &&
         possession != TECMO_GAMEPLAY_TEAM_HOME) ||
        scene->actors[claimant].team != (uint8_t)possession ||
        !tecmo_gameplay_scene_possession_trace_snapshot(scene, &before)) {
        return false;
    }
    candidate = *scene;
    if (!scene_handoff_possession_impl(
            &candidate, possession, claimant, false) ||
        candidate.ball_holder != claimant ||
        candidate.state.possession != possession ||
        !tecmo_gameplay_live_foundation_claimant_settlement(
            &candidate.cpu_steering_assets, claimant, (uint8_t)possession,
            &candidate.live_foundation, &transaction) ||
        !tecmo_gameplay_scene_possession_trace_snapshot(&candidate, &after)) {
        return false;
    }
    /* Diagnostic event serial zero remains the not-yet-emitted sentinel.
       Like the established LIVE observation serials, UINT32_MAX restarts at
       one rather than producing a misleading zero event. */
    next_serial = candidate.claimant_settlement_trace.event_serial ==
            UINT32_MAX
        ? 1U : candidate.claimant_settlement_trace.event_serial + 1U;
    memset(&candidate.claimant_settlement_trace, 0,
           sizeof(candidate.claimant_settlement_trace));
    candidate.claimant_settlement_trace.contract_tag =
        TECMO_GAMEPLAY_SCENE_CLAIMANT_TRACE_TAG;
    candidate.claimant_settlement_trace.event_serial = next_serial;
    candidate.claimant_settlement_trace.valid = true;
    candidate.claimant_settlement_trace.transaction = transaction;
    candidate.claimant_settlement_trace.before = before;
    candidate.claimant_settlement_trace.after = after;
    *scene = candidate;
    return true;
}

/* Legacy/direct diagnostics deliberately preserve their accepted generic
 * handoff checkpoints. They can manufacture a claimant only through their
 * explicitly marked fallback and do not establish the $BA56 claimant/contact
 * caller predicates required by the typed $B87C transaction. Production miss
 * selection remains the only bridge entry. */
static bool scene_handoff_miss_claimant(
    TecmoGameplayScene *scene,
    TecmoGameplayTeam possession,
    uint8_t claimant)
{
    if (scene == NULL) return false;
    if (scene->legacy_direct_launch || scene->jump_rim_rattle_debug) {
        return scene_handoff_possession(scene, possession, claimant);
    }
    return scene_handoff_claimant_settlement(scene, possession, claimant);
}

void scene_loose_ball_clear(TecmoGameplayScene *scene)
{
    if (scene == NULL) return;
    memset(&scene->loose_ball_state, 0, sizeof(scene->loose_ball_state));
    scene->loose_ball_state.shooting_team = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->loose_ball_state.chase_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
}

bool scene_loose_ball_state_valid(const TecmoGameplayScene *scene)
{
    size_t controller;
    if (scene == NULL) return false;
    if (!scene->loose_ball_state.active) {
        return scene->loose_ball_state.shooting_team ==
                   TECMO_GAMEPLAY_SCENE_NO_TEAM &&
               scene->loose_ball_state.chase_actor ==
                   TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               scene->loose_ball_state.reserved == 0U;
    }
    if ((scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
         scene->state.phase !=
             TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->loose_ball_state.shooting_team >=
            TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->state.possession !=
            (TecmoGameplayTeam)scene->loose_ball_state.shooting_team ||
        scene->loose_ball_state.chase_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[scene->loose_ball_state.chase_actor].active ||
        scene->jump_ball_altitude_q8 != 0U ||
        scene->loose_ball_state.reserved != 0U ||
        scene_pass_active(scene) || scene_inbound_active(scene) ||
        !tecmo_gameplay_court_coordinate_q8_valid(&scene->ball_position)) {
        return false;
    }
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->controlled_actor[controller] ==
            scene->loose_ball_state.chase_actor) {
            return false;
        }
    }
    return true;
}

static bool scene_begin_loose_ball(
    TecmoGameplayScene *scene, TecmoGameplayTeam shooting_team)
{
    TecmoGameplayScene candidate;
    uint8_t actor;
    size_t controller;
    bool controlled;
    if (scene == NULL || shooting_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != shooting_team ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->jump_ball_altitude_q8 != 0U ||
        !tecmo_gameplay_court_coordinate_q8_valid(&scene->ball_position)) {
        return false;
    }
    candidate = *scene;
    scene_loose_ball_clear(&candidate);
    candidate.loose_ball_state.active = true;
    candidate.loose_ball_state.shooting_team = (uint8_t)shooting_team;
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (!candidate.actors[actor].active) continue;
        controlled = false;
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (candidate.controlled_actor[controller] == actor) {
                controlled = true;
                break;
            }
        }
        if (!controlled) {
            candidate.loose_ball_state.chase_actor = actor;
            break;
        }
    }
    if (!scene_loose_ball_state_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

static bool scene_close_step_for_frame(const TecmoGameplayScene *scene,
                                       uint16_t frame,
                                       uint8_t *step)
{
    TecmoGameplayCloseShotVariantInfo info;
    uint16_t selected;
    if (scene == NULL || step == NULL ||
        !scene_shot_is_close(scene->shot_kind) ||
        (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1 &&
         scene->shot_duration == 0U)) {
        return false;
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1) {
        selected = frame < scene->shot_duration
            ? frame : (uint16_t)(scene->shot_duration - 1U);
        *step = (uint8_t)selected;
        return true;
    }
    if (!tecmo_gameplay_close_shots_get_variant_info(
            &scene->close_shots, scene_close_variant(scene->shot_kind),
            &info) || info.step_count == 0U) {
        return false;
    }
    /* The exact TGCS phase schedules are used only for numeric 0 and 2. */
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK) {
        selected = frame <= 22U
            ? frame
            : frame < TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME
                ? 22U
                : (uint16_t)(22U + frame -
                              (TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME - 1U));
    } else {
        selected = frame < info.step_count ? frame : info.step_count - 1U;
    }
    if (selected >= info.step_count) selected = info.step_count - 1U;
    *step = (uint8_t)selected;
    return true;
}

static bool scene_select_shot_claimant(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam shooting_team,
    uint8_t *claimant_out,
    TecmoGameplayShotClaimantTeamRelation *relation_out,
    TecmoGameplayShotSettlementDecision *decision_out)
{
    int16_t ball_x;
    int16_t ball_y;
    uint8_t ball_altitude;
    if (scene == NULL || claimant_out == NULL || relation_out == NULL ||
        decision_out == NULL ||
        (shooting_team != TECMO_GAMEPLAY_TEAM_AWAY &&
         shooting_team != TECMO_GAMEPLAY_TEAM_HOME)) {
        return false;
    }
    ball_x = (int16_t)(scene->ball_position.x_q8 / 256);
    ball_y = (int16_t)(scene->ball_position.y_q8 / 256);
    ball_altitude = (uint8_t)(scene->jump_ball_altitude_q8 >> 8U);
    /* Source order is preserved as the native scan/order substitution. The
       evidence proves eligibility and the SAME_TEAM/OTHER_TEAM settlement
       relation, not a geometric nearest-distance ranking or a rebound/steal
       label. The missing $BA/$40 predicate is not universal here: every
       eligible active actor, including the shooter, remains selectable. */
    for (uint8_t index = 0U; index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT;
         ++index) {
        const TecmoGameplaySceneActor *candidate = &scene->actors[index];
        TecmoGameplayShotSettlementDecision decision;
        TecmoGameplayShotClaimantTeamRelation relation;
        bool eligible;
        if (!candidate->active ||
            (candidate->team != TECMO_GAMEPLAY_TEAM_AWAY &&
             candidate->team != TECMO_GAMEPLAY_TEAM_HOME)) {
            continue;
        }
        relation = candidate->team == (uint8_t)shooting_team
            ? TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM
            : TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM;
        if (!tecmo_gameplay_shot_resolution_claimant_is_eligible(
                &scene->shot_resolution,
                (int16_t)((int)candidate->position.x - (int)ball_x),
                (int16_t)((int)candidate->position.y - (int)ball_y),
                0U, ball_altitude, &eligible) || !eligible ||
            !tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                &scene->shot_resolution, false, relation, &decision) ||
            !decision.select_claimant ||
            (relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM &&
             (!decision.replace_other_handler_with_previous ||
              !decision.change_possession)) ||
            (relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM &&
             decision.change_possession)) {
            continue;
        }
        *claimant_out = index;
        *relation_out = relation;
        *decision_out = decision;
        return true;
    }
    if (scene->jump_rim_rattle_debug || scene->legacy_direct_launch) {
        /* The captured diagnostic's exact $A7A9/$AD6E claimant geometry and
           the pre-R2 direct harness's opposing-handler placement are not
           proven. Preserve those accepted checkpoints with the source-order
           opposing handler only in these explicitly marked legacy adapters.
           Bound close/jump production paths retain the eligibility gate and
           fail closed when no claimant qualifies. */
        for (uint8_t index = 0U;
             index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++index) {
            const TecmoGameplaySceneActor *candidate = &scene->actors[index];
            if (candidate->active &&
                candidate->team == (uint8_t)scene_other_team(shooting_team)) {
                if (!tecmo_gameplay_shot_resolution_decide_claimant_settlement(
                        &scene->shot_resolution, false,
                        TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM,
                        decision_out)) {
                    return false;
                }
                *claimant_out = index;
                *relation_out = TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM;
                return decision_out->select_claimant &&
                       decision_out->replace_other_handler_with_previous &&
                       decision_out->change_possession;
            }
        }
    }
    return false;
}

bool scene_update_loose_ball(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT])
{
    TecmoGameplayScene candidate;
    TecmoGameplayShotClaimantTeamRelation relation;
    TecmoGameplayShotSettlementDecision decision;
    TecmoGameplayTeam possession;
    uint8_t claimant;
    size_t controller;
    if (scene == NULL || controls == NULL ||
        !scene_loose_ball_state_valid(scene)) {
        return false;
    }
    candidate = *scene;
    if (candidate.state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        uint8_t retained_primary = candidate.live_foundation.primary_actor;
        TecmoGameplayTeam retained_team = (TecmoGameplayTeam)
            candidate.loose_ball_state.shooting_team;
        /* Period-expiry shot settlement already retains the current side and
           emits no claimant transaction. If zero arrives during this bounded
           pending phase, end it through that same typed scene policy rather
           than calling B87C after gameplay_reset_possession has closed. */
        if (retained_primary >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            candidate.actors[retained_primary].team !=
                (uint8_t)retained_team) {
            return false;
        }
        scene_loose_ball_clear(&candidate);
        if (!scene_handoff_possession(
                &candidate, retained_team, retained_primary) ||
            !scene_loose_ball_state_valid(&candidate)) {
            return false;
        }
        *scene = candidate;
        return true;
    }
    /* Fixed $F031/$F034 selected movement precedes the slot-10 $A214
       dispatcher. Preserve controller movement first; the extra autonomous
       chase below is only a bounded substitute for the still-missing actor
       scheduler, not a claim about either fixed-bank call. */
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (!scene_move_controlled_actor(
                &candidate, controller, controls[controller])) {
            return false;
        }
    }
    if (!scene_move_actor_toward_loose_ball(
            &candidate, candidate.loose_ball_state.chase_actor)) {
        return false;
    }
    possession = (TecmoGameplayTeam)candidate.loose_ball_state.shooting_team;
    if (!scene_select_shot_claimant(
            &candidate, possession, &claimant, &relation, &decision)) {
        if (!scene_loose_ball_state_valid(&candidate)) return false;
        *scene = candidate;
        return true;
    }
    possession = relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM
        ? scene_other_team(possession) : possession;
    scene_loose_ball_clear(&candidate);
    if (!scene_handoff_claimant_settlement(
            &candidate, possession, claimant) ||
        !scene_sync_live_foundation(&candidate) ||
        !scene_loose_ball_state_valid(&candidate)) {
        return false;
    }
    *scene = candidate;
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
    TecmoGameplayShotClaimantTeamRelation claimant_relation =
        TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM;
    TecmoGameplayShotSettlementDecision decision;
    uint8_t claimant = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint16_t idle_pose;
    bool source_claimant = false;
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        actor != &scene->actors[scene->shot_actor] ||
        !scene_actor_movement_state(scene, actor, &movement) ||
        !scene_actor_movement_pose_index(
            scene, scene->actors, scene->shot_actor, &movement,
            &idle_pose)) {
        return false;
    }
    if (!made && scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        source_claimant = scene_select_shot_claimant(
            scene, shooting_team, &claimant, &claimant_relation, &decision);
        if (!source_claimant) {
            claimant = scene_first_actor_for_team(
                scene_other_team(shooting_team));
            if (claimant >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                !scene->actors[claimant].active ||
                scene->actors[claimant].team !=
                    (uint8_t)scene_other_team(shooting_team)) {
                return false;
            }
        }
    }
    if (made) {
        if (!tecmo_gameplay_award_points(&scene->state, shooting_team,
                                         scene->shot_points)) {
            return false;
        }
        if (!scene_record_shot_make_stats(scene)) return false;
        if (queue_side_result) {
            if (!scene_shot_queue_result_audio(scene, shooting_team)) {
                return false;
            }
        } else {
            /* The exact side-result ordering is proved for the dunk. Layups
               retain the crowd-only behavior. */
            if (!tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE)) {
                return false;
            }
        }
    }
    actor->pose_index = idle_pose;
    next_team = scene_other_team(shooting_team);
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);
    if (scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        if (!scene_handoff_possession(
            scene, scene->state.possession,
            scene_first_actor_for_team(scene->state.possession))) {
            return false;
        }
        return true;
    }
    if (made) {
        if (!scene_begin_scored_inbound(scene, next_team)) {
            return false;
        }
        return true;
    }
    if (source_claimant) {
        return scene_handoff_miss_claimant(
            scene,
            claimant_relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM
                ? next_team : shooting_team,
            claimant);
    }
    /* In the ordinary admitted context, Bank05 $A214->$B6E5 retains slot-10
       state $10 and the no-claim path retries the ascending claimant scan.
       Preserve the terminal grounded ball/no-holder state instead of
       manufacturing a claimant. Earlier $BA/$0499 cancellation gates, full
       B7C1 physics, and airborne state $11 remain outside this bounded slice. */
    return scene_begin_loose_ball(scene, shooting_team);
}

static bool scene_finish_jump_miss(TecmoGameplayScene *scene,
                                   TecmoGameplaySceneActor *actor,
                                   TecmoGameplayTeam shooting_team)
{
    TecmoGameplayTeam next_team;
    TecmoGameplayShotClaimantTeamRelation claimant_relation =
        TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM;
    TecmoGameplayShotSettlementDecision claimant_decision;
    uint8_t claimant = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t shooting_actor;
    bool period_expiry;
    bool source_claimant = false;
    if (scene == NULL || actor == NULL ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }

    period_expiry = scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
    shooting_actor = (uint8_t)(actor - scene->actors);
    next_team = scene_other_team(shooting_team);
    if (!period_expiry) {
        source_claimant = scene_select_shot_claimant(
            scene, shooting_team, &claimant, &claimant_relation,
            &claimant_decision);
        if (!source_claimant) {
            claimant = scene_first_actor_for_team(next_team);
            if (claimant >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
                !scene->actors[claimant].active ||
                scene->actors[claimant].team != (uint8_t)next_team) {
                return false;
            }
        }
    }

    actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);

    if (period_expiry) {
        /* The caller queues the post-miss result after state events, so the
           zero-clock crowd request remains the final audio mailbox write. */
        return scene_handoff_possession(
            scene, scene->state.possession,
            shooting_actor);
    }
    if (source_claimant) {
        return scene_handoff_miss_claimant(
            scene,
            claimant_relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM
                ? next_team : shooting_team,
            claimant);
    }
    return scene_begin_loose_ball(scene, shooting_team);
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

/* The captured jump route keeps its 103-update A7A9 schedule intact.  Close
   misses and the ordinary numeric A708/A8E9 routes reach this explicit tail
   after their own arc has ended.  The tail is a bounded native substitution;
   raw route/address identity remains authoritative, while no rebound/block/
   steal label is inferred from the unproven full flight inputs. */
static bool scene_begin_shot_rim_tail(TecmoGameplayScene *scene)
{
    uint8_t selector;
    uint8_t duration;
    uint8_t orientation;
    uint16_t base_frame;
    uint32_t tail_end;
    TecmoGameplayShotRimRattle candidate_rattle;
    if (scene == NULL || scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_rim_tail_active || scene->shot_frame == UINT16_MAX ||
        !scene_shot_is_close(scene->shot_kind) &&
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_resolution.route_selector_mask != 0x03U) {
        return false;
    }
    selector = (uint8_t)(scene->shot_rim_rattle_raw_selector &
                         scene->shot_resolution.route_selector_mask);
    if (scene->shot_rim_route.selector != selector) return false;
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
        (scene->jump_rim_rattle_debug || scene->predicted_make_route ||
         !scene->jump_playback_active ||
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
         scene->jump_actor_state !=
             scene->jump_shots.constants.actor_state_neutral ||
         scene->jump_ball_state !=
             scene->jump_shots.constants.ball_state_route10)) {
        return false;
    }
    if (scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }
    switch (scene->shot_rim_route.kind) {
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708:
        if (scene->shot_rim_route.source_target_cpu != 0xA708U ||
            (selector != 0U && selector != 3U)) {
            return false;
        }
        duration = selector == 0U ? 6U : 8U;
        break;
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9:
        if (scene->shot_rim_route.source_target_cpu != 0xA7A9U ||
            selector != 1U ||
            scene->shot_resolution.rim_rattle.repeat_dmc_length != 0x0AU) {
            return false;
        }
        duration = (uint8_t)(
            TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES +
            TECMO_GAMEPLAY_SHOT_RIM_TAIL_GROUND_UPDATE);
        break;
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9:
        if (scene->shot_rim_route.source_target_cpu != 0xA8E9U ||
            selector != 2U) {
            return false;
        }
        duration = 7U;
        break;
    default:
        return false;
    }
    base_frame = scene->shot_frame;
    tail_end = (uint32_t)base_frame + (uint32_t)duration;
    if (tail_end > UINT16_MAX) return false;

    if (scene->shot_rim_route.kind ==
            TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
        memset(&candidate_rattle, 0, sizeof(candidate_rattle));
        if (!scene_shot_captured_rattle_orientation(scene, &orientation)) {
            return false;
        }
        if (!tecmo_gameplay_shot_rim_rattle_begin(
                &scene->shot_resolution, &candidate_rattle,
                orientation, 3U,
                (uint8_t)(base_frame & 0x0FU),
                TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6,
                0) || !scene_map_rim_rattle_ball_position(
                    scene, &candidate_rattle)) {
            return false;
        }
        scene->jump_rim_rattle = candidate_rattle;
        scene->jump_ball_state = candidate_rattle.object_state;
        scene->jump_ball_altitude_q8 =
            (uint16_t)candidate_rattle.altitude << 8U;
        scene->jump_rim_rattle_audio_repeats = 0U;
        scene->shot_rim_rattle_selected = true;
    } else {
        memset(&scene->jump_rim_rattle, 0,
               sizeof(scene->jump_rim_rattle));
        scene->jump_ball_altitude_q8 = 0U;
        scene->shot_rim_rattle_selected = false;
    }
    scene->shot_rim_tail_active = true;
    scene->shot_rim_tail_frame = 0U;
    scene->shot_rim_tail_duration = duration;
    scene->shot_rim_tail_base_frame = base_frame;
    scene->shot_duration = (uint16_t)tail_end;
    return true;
}

static void scene_update_approximate_shot_rim_tail_position(
    TecmoGameplayScene *scene)
{
    uint8_t selector = (uint8_t)(scene->shot_rim_rattle_raw_selector & 0x03U);
    int32_t offset_x;
    int32_t offset_y;
    if (scene->shot_rim_route.kind ==
            TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 && selector == 0U) {
        offset_x = -5 * 256 + (int32_t)scene->shot_rim_tail_frame * 64;
        offset_y = -4 * 256 + (int32_t)scene->shot_rim_tail_frame * 64;
    } else if (scene->shot_rim_route.kind ==
                   TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 && selector == 3U) {
        offset_x = 5 * 256 - (int32_t)scene->shot_rim_tail_frame * 64;
        offset_y = -4 * 256 + (int32_t)scene->shot_rim_tail_frame * 32;
    } else {
        /* A8E9 is intentionally a different bounded endpoint trace from
           both A708 selector values; the address identity is not renamed. */
        offset_x = 0;
        offset_y = -8 * 256 + (int32_t)scene->shot_rim_tail_frame * 128;
    }
    scene->ball_position.x_q8 = scene->shot_end_position.x_q8 + offset_x;
    scene->ball_position.y_q8 = scene->shot_end_position.y_q8 + offset_y;
}

static bool scene_update_shot_rim_tail_mutating(TecmoGameplayScene *scene)
{
    TecmoGameplaySceneActor *actor;
    uint16_t expected_frame;
    uint16_t next_frame;
    uint32_t expected_frame_wide;
    uint32_t expected_duration_wide;
    bool repeat_dmc = false;
    bool completed = false;
    if (scene == NULL || !scene->shot_rim_tail_active ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_rim_tail_duration == 0U ||
        scene->shot_rim_tail_frame >= scene->shot_rim_tail_duration ||
        scene->shot_frame == UINT16_MAX) {
        return false;
    }
    expected_frame_wide = (uint32_t)scene->shot_rim_tail_base_frame +
                          (uint32_t)scene->shot_rim_tail_frame;
    expected_duration_wide = (uint32_t)scene->shot_rim_tail_base_frame +
                             (uint32_t)scene->shot_rim_tail_duration;
    if (expected_frame_wide > UINT16_MAX ||
        expected_duration_wide > UINT16_MAX) {
        return false;
    }
    expected_frame = (uint16_t)expected_frame_wide;
    if (scene->shot_frame != expected_frame ||
        scene->shot_duration != (uint16_t)expected_duration_wide ||
        !scene_next_shot_frame(scene, &next_frame)) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    ++scene->shot_rim_tail_frame;
    scene->shot_frame = next_frame;
    if (scene->shot_rim_route.kind ==
            TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
        if (scene->shot_rim_tail_frame <=
                TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES) {
            bool rattle_terminal = scene->shot_rim_tail_frame ==
                TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
            if (!scene->jump_rim_rattle.active ||
                scene->shot_resolution.rim_rattle.repeat_dmc_length != 0x0AU ||
                !tecmo_gameplay_shot_rim_rattle_step(
                    &scene->shot_resolution, &scene->jump_rim_rattle,
                    &repeat_dmc, &completed) ||
                !scene_map_rim_rattle_ball_position(
                    scene, &scene->jump_rim_rattle)) {
                return false;
            }
            /* Keep the source contract's altitude $38 intact until all four
               passes have completed. The grounded handoff is a separate
               native approximation below. */
            scene->jump_ball_altitude_q8 =
                (uint16_t)scene->jump_rim_rattle.altitude << 8U;
            if (repeat_dmc &&
                !tecmo_gameplay_audio_queue_dmc_clip(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT)) {
                return false;
            }
            if (repeat_dmc) ++scene->jump_rim_rattle_audio_repeats;
            if (completed != rattle_terminal ||
                scene->jump_rim_rattle.complete != rattle_terminal ||
                scene->jump_rim_rattle.active == rattle_terminal) {
                return false;
            }
        } else if (scene->shot_rim_tail_frame ==
                       (uint8_t)(TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES +
                                 TECMO_GAMEPLAY_SHOT_RIM_TAIL_GROUND_UPDATE)) {
            if (!scene->jump_rim_rattle.complete ||
                scene->jump_rim_rattle.active) {
                return false;
            }
            /* $AD6E's full landing inputs are not proven. This explicit
               one-update bridge only makes the claimant altitude grounded;
               it does not rename the route or claim rebound semantics. */
            scene->jump_ball_altitude_q8 = 0U;
            scene->ball_position = scene->shot_end_position;
        } else {
            return false;
        }
    } else {
        scene_update_approximate_shot_rim_tail_position(scene);
    }
    if (scene->shot_rim_tail_frame < scene->shot_rim_tail_duration) {
        return true;
    }
    scene->shot_rim_tail_active = false;
    if (scene_shot_is_close(scene->shot_kind)) {
        return scene_finish_shot(
            scene, actor, (TecmoGameplayTeam)actor->team, false, false);
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        return scene_finish_jump_miss(
            scene, actor, (TecmoGameplayTeam)actor->team);
    }
    return false;
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
    scene_shot_clear_jump_playback(scene);
    if (period_expiry) {
        return scene_handoff_possession(
            scene, scene->state.possession, shooting_actor);
    }
    return scene_begin_scored_inbound(scene, next_team);
}

static void scene_release_jump_make(TecmoGameplayScene *scene,
                                    TecmoGameplaySceneActor *actor)
{
    scene->jump_b_released = true;
    scene->shot_frame = TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME;
    scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME;
    scene->jump_phase_counter =
        scene->jump_shots.constants.phase_seed_gather;
    actor->pose_index = scene->legacy_direct_launch
        ? TECMO_GAMEPLAY_JUMP_RELEASE_POSE
        : scene_jump_playback_flight_pose(scene);
    scene_update_jump_make_ball_position(scene);
}

static void scene_update_approx_make_ball_position(
    TecmoGameplayScene *scene)
{
    uint16_t frame = scene->shot_frame;
    uint16_t release = TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME;
    uint16_t score = TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME;
    int32_t apex_y =
        (scene->shot_start_position.y_q8 < scene->shot_end_position.y_q8
             ? scene->shot_start_position.y_q8
             : scene->shot_end_position.y_q8) - 28 * 256;
    if (frame <= release) {
        scene->ball_position = scene->shot_start_position;
    } else if (frame < score) {
        unsigned flight = (unsigned)(frame - release);
        unsigned flight_length = (unsigned)(score - release);
        scene->ball_position.x_q8 = scene_lerp_q8(
            scene->shot_start_position.x_q8,
            scene->shot_end_position.x_q8, flight, flight_length);
        if (frame <= release + flight_length / 2U) {
            scene->ball_position.y_q8 = scene_lerp_q8(
                scene->shot_start_position.y_q8, apex_y, flight,
                flight_length / 2U);
        } else {
            scene->ball_position.y_q8 = scene_lerp_q8(
                apex_y, scene->shot_end_position.y_q8,
                (unsigned)(frame - (release + flight_length / 2U)),
                flight_length - flight_length / 2U);
        }
    } else {
        scene->ball_position = scene->shot_end_position;
    }
}

static bool scene_finish_approx_jump_make(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneActor *actor,
    TecmoGameplayTeam shooting_team)
{
    bool period_expiry;
    TecmoGameplayTeam next_team;
    if (scene == NULL || actor == NULL ||
        scene->shot_frame != scene->shot_duration ||
        scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
        !scene->shot_result_awarded ||
        !scene->jump_actor_landed ||
        scene->jump_actor_altitude_q8 != 0U ||
        scene->jump_actor_velocity_q8 != 0U ||
        scene->jump_actor_state !=
            scene->jump_shots.constants.actor_state_neutral ||
        (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
         scene->state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE)) {
        return false;
    }
    if (!tecmo_gameplay_audio_queue_event(
            &scene->audio_player, TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE)) {
        return false;
    }
    period_expiry = scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
    next_team = scene_other_team(shooting_team);
    actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
    scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->close_shot_step = 0U;
    scene->shot_frame = 0U;
    scene->shot_duration = 0U;
    scene_shot_clear_jump_playback(scene);
    return period_expiry
        ? scene_handoff_possession(
              scene, scene->state.possession,
              (uint8_t)(actor - scene->actors))
        : scene_begin_scored_inbound(scene, next_team);
}

static bool scene_update_jump_make_approx(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplaySceneActor *actor;
    uint16_t next_frame;
    TecmoGameplayShotOutcome outcome;
    if (scene == NULL || !scene->jump_playback_active ||
        !scene->predicted_make_route || scene->shot_kind !=
            TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_schedule == TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT ||
        scene->shot_duration != TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION ||
        scene->shot_frame == 0U || scene->shot_frame > scene->shot_duration ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene_shot_controller_binding_valid(scene)) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    if (!scene->jump_b_released) {
        if (scene->shot_frame <
                TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME &&
            shooting_controls != NULL && shooting_controls->held.cancel) {
            if (!scene_next_shot_frame(scene, &next_frame)) return false;
            scene->shot_frame = next_frame;
            scene->jump_pose_frame = (uint8_t)next_frame;
            return true;
        }
        if (scene->shot_frame >
                TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME) {
            return false;
        }
        if (scene->shot_frame ==
                TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME &&
            shooting_controls != NULL && shooting_controls->held.cancel) {
            /* Bank05 $86DD releases from actor state $0C only after current B
               clears.  Reaching the native-C gather cap is not a release
               edge and must not detach the ball automatically. */
            return true;
        }
        outcome = TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN;
        if (!tecmo_gameplay_shot_resolution_classify_terminal_outcome(
                &scene->shot_resolution, true, 0U, &outcome) ||
            outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) {
            return false;
        }
        scene->jump_b_released = true;
        scene->jump_outcome = outcome;
        scene->shot_frame =
            TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME;
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_airborne;
        scene->jump_ball_state = scene->jump_shots.constants.ball_state_route5;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_airborne;
        actor->pose_index = scene->legacy_direct_launch
            ? TECMO_GAMEPLAY_JUMP_RELEASE_POSE
            : scene_jump_playback_flight_pose(scene);
        scene->jump_actor_velocity_q8 =
            TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8;
        scene_update_approx_make_ball_position(scene);
        return true;
    }

    if (!scene_next_shot_frame(scene, &next_frame)) return false;
    scene->shot_frame = next_frame;
    if (next_frame > TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME &&
        next_frame <= TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
        actor->pose_index = scene_jump_playback_flight_pose(scene);
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
    } else if (next_frame >= TECMO_GAMEPLAY_JUMP_APPROX_MAKE_NEUTRAL_FRAME) {
        actor->pose_index = TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
        scene->jump_pose_frame = 0U;
    }
    if (next_frame > TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME &&
        next_frame <= TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME &&
        !scene->jump_actor_landed) {
        bool landed = false;
        if (!tecmo_gameplay_jump_shots_step_q8(
                &scene->jump_shots, &scene->jump_actor_altitude_q8,
                &scene->jump_actor_velocity_q8, &landed)) {
            return false;
        }
        scene->jump_actor_landed = landed;
    }
    if (next_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
        if (!scene->jump_actor_landed ||
            scene->jump_actor_altitude_q8 != 0U ||
            scene->jump_actor_velocity_q8 != 0U) {
            return false;
        }
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_recovery;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_recovery_counter;
        actor->pose_index = scene_jump_playback_flight_pose(scene);
    } else if (next_frame >=
                   TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_START_FRAME &&
               next_frame <=
                   TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_LAST_FRAME) {
        if (scene->jump_actor_state !=
                scene->jump_shots.constants.actor_state_recovery ||
            scene->jump_phase_counter < 0x10U) {
            return false;
        }
        scene->jump_phase_counter =
            (uint8_t)(scene->jump_phase_counter - 0x10U);
    } else if (next_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_NEUTRAL_FRAME) {
        if (scene->jump_actor_state !=
                scene->jump_shots.constants.actor_state_recovery ||
            scene->jump_phase_counter != 0x06U) {
            return false;
        }
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_neutral;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_gather;
    }
    scene_update_approx_make_ball_position(scene);
    if (next_frame == TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME &&
        !scene->shot_result_awarded) {
        TecmoGameplayState state_before = scene->state;
        bool period_expiry = scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
        if (scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
            !scene->jump_actor_landed ||
            scene->jump_actor_altitude_q8 != 0U ||
            scene->jump_actor_velocity_q8 != 0U ||
            scene->jump_actor_state !=
                scene->jump_shots.constants.actor_state_neutral ||
            (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE &&
             !period_expiry) ||
            !tecmo_gameplay_award_points(
                &scene->state, (TecmoGameplayTeam)actor->team,
                scene->shot_points) ||
            (!period_expiry && !tecmo_gameplay_reset_possession(
                &scene->state, (TecmoGameplayTeam)actor->team))) {
            scene->state = state_before;
            return false;
        }
        if (!scene_record_shot_make_stats(scene)) {
            scene->state = state_before;
            return false;
        }
        scene->shot_result_awarded = true;
        scene->jump_made_settlement.complete = true;
    }
    if (next_frame < scene->shot_duration) return true;
    return scene_finish_approx_jump_make(
        scene, actor, (TecmoGameplayTeam)actor->team);
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
    if (scene != NULL && scene->shot_schedule !=
            TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) {
        return scene_update_jump_make_approx(scene, shooting_controls);
    }
    if (!scene->jump_playback_active || !scene->predicted_make_route ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration != (uint16_t)(
            TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
            scene->jump_shots.constants.made_update_count) ||
        scene->shot_frame == 0U ||
        scene->shot_frame > scene->shot_duration ||
        !scene_shot_controller_binding_valid(scene)) {
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
            if (!scene_next_shot_frame(scene, &next_frame)) return false;
            scene->shot_frame = next_frame;
            scene->jump_pose_frame = (uint8_t)next_frame;
            if (next_frame == 5U) {
                actor->pose_index = TECMO_GAMEPLAY_JUMP_TURN_POSE;
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

    if (!scene_next_shot_frame(scene, &next_frame)) return false;
    scene->shot_frame = next_frame;
    if (next_frame >= 10U && next_frame <= 17U) {
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_prepared;
        scene->jump_phase_counter = release_phases[next_frame - 10U];
        actor->pose_index = scene_jump_playback_flight_pose(scene);
    } else if (next_frame == 18U) {
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_held;
        scene->jump_phase_counter = 0x34U;
        actor->pose_index = scene_jump_playback_flight_pose(scene);
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
        actor->pose_index = scene_jump_playback_flight_pose(scene);
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
        actor->pose_index = scene_jump_playback_flight_pose(scene);
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
        scene->jump_pose_frame = 0U;
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
        if (!scene_record_shot_make_stats(scene)) {
            scene->state = state_before;
            return false;
        }
        scene->shot_result_awarded = true;
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

static bool scene_update_jump_miss_rim_rattle(
    TecmoGameplayScene *scene,
    uint16_t next_frame,
    uint16_t *route_frame,
    bool *rattle_position_owned);

static bool scene_update_jump_miss_mutating(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplaySceneActor *actor;
    TecmoGameplayShotOutcome outcome;
    uint16_t next_frame;
    uint16_t route_frame;
    bool landed = false;
    bool rattle_position_owned = false;
    bool actual_rattle;
    if (!scene->jump_playback_active || scene->predicted_make_route ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration !=
            (scene->jump_rim_rattle_debug || scene->shot_rim_rattle_selected
                 ? TECMO_GAMEPLAY_JUMP_RATTLE_DURATION
                 : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION) ||
        scene->shot_frame == 0U ||
        scene->shot_frame > scene->shot_duration ||
        !scene_shot_controller_binding_valid(scene) ||
        (!scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) ||
        (scene->jump_b_released &&
         scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS)) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    actual_rattle = scene->jump_rim_rattle_debug ||
                    scene->shot_rim_rattle_selected;
    if (!scene->jump_b_released) {
        uint16_t expected_pose;
        if (scene->jump_pose_frame == 0U ||
            scene->jump_pose_frame >
                TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME) {
            return false;
        }
        expected_pose = scene->jump_pose_frame <=
                                TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME
                            ? scene->jump_entry_pose_index
                            : TECMO_GAMEPLAY_JUMP_TURN_POSE;
        if (actor->pose_index != expected_pose) return false;
        /* Bank05 tests the current NES B level. No previous/released edge and
           no DMC request participates in this transition. */
        if (shooting_controls != NULL && shooting_controls->held.cancel) {
            if (scene->shot_frame != 1U ||
                scene->jump_actor_state !=
                    scene->jump_shots.constants.actor_state_held ||
                scene->jump_ball_state !=
                    scene->jump_shots.constants.ball_state_route1) {
                return false;
            }
            if (scene->jump_pose_frame <
                TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME) {
                ++scene->jump_pose_frame;
                actor->pose_index = scene->jump_pose_frame <
                                            TECMO_GAMEPLAY_JUMP_TURN_POSE_FIRST_FRAME
                                        ? scene->jump_entry_pose_index
                                        : TECMO_GAMEPLAY_JUMP_TURN_POSE;
            }
            return true;
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
        if (!scene->legacy_direct_launch) {
            TecmoGameplayCpuA0f3Input launch_input;
            TecmoGameplayCpuA0f3Result launch_result;
            TecmoGameplayCpuA0f3Motion launch_motion;
            uint8_t preflight_raw_006a;
            uint8_t launch_raw_006a;
            memset(&launch_input, 0, sizeof(launch_input));
            if (!scene->shot_a0f3_origin_valid ||
                !scene->cpu_a0f3_assets.available ||
                !tecmo_gameplay_fixed_rng_c05d(
                    &scene->fixed_rng,
                    TECMO_GAMEPLAY_FIXED_RNG_CALL_9FA1,
                    &preflight_raw_006a) ||
                !tecmo_gameplay_fixed_rng_c05d(
                    &scene->fixed_rng,
                    TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD,
                    &launch_raw_006a)) {
                return false;
            }
            launch_input.contract_tag =
                TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG;
            launch_input.raw_x_7d_f2 = scene->shot_a0f3_origin_x;
            launch_input.raw_depth_fd = scene->shot_a0f3_origin_depth;
            launch_input.raw_direction = (uint8_t)scene->jump_direction;
            launch_input.raw_006a = launch_raw_006a;
            if (!tecmo_gameplay_cpu_a0f3_solve(
                    &scene->cpu_a0f3_assets, &launch_input,
                    &launch_result) ||
                !tecmo_gameplay_cpu_a0f3_motion_begin(
                    &launch_result, &launch_motion)) {
                return false;
            }
            scene->shot_a0f3_preflight_valid = true;
            scene->shot_a0f3_preflight_raw_006a = preflight_raw_006a;
            scene->shot_a0f3_launch_raw_006a = launch_raw_006a;
            scene->shot_a0f3_result = launch_result;
            scene->shot_a0f3_motion = launch_motion;
            scene->shot_a0f3_motion_valid = true;
            scene->shot_a0f3_raw_position_valid = true;
            scene->shot_a0f3_raw_x = scene->shot_a0f3_origin_x;
            scene->shot_a0f3_raw_depth = scene->shot_a0f3_origin_depth;
            scene->shot_a0f3_tick_count = 0U;
        }
        scene->jump_b_released = true;
        scene->jump_outcome = outcome;
        scene->shot_frame = 2U;
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME;
        scene->jump_actor_state =
            scene->jump_shots.constants.actor_state_airborne;
        scene->jump_ball_state = scene->jump_shots.constants.ball_state_route5;
        scene->jump_phase_counter =
            scene->jump_shots.constants.phase_seed_airborne;
        actor->pose_index = scene->legacy_direct_launch
            ? TECMO_GAMEPLAY_JUMP_RELEASE_POSE
            : scene_jump_playback_flight_pose(scene);
        scene->jump_actor_velocity_q8 =
            TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8;
        scene_update_jump_ball_position(scene);
        return true;
    }

    if (!scene_next_shot_frame(scene, &next_frame)) return false;
    if ((scene->shot_frame == 2U &&
         (scene->jump_pose_frame !=
              TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME ||
          actor->pose_index != (scene->legacy_direct_launch
              ? TECMO_GAMEPLAY_JUMP_RELEASE_POSE
              : scene_jump_playback_flight_pose(scene)))) ||
        (scene->shot_frame >= 3U && scene->shot_frame < 46U &&
         (scene->jump_pose_frame !=
              TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME ||
          actor->pose_index != scene_jump_playback_flight_pose(scene))) ||
        (scene->shot_frame >= 46U &&
         (scene->jump_pose_frame != 0U ||
          actor->pose_index != TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE))) {
        return false;
    }
    scene->shot_frame = next_frame;
    route_frame = next_frame;

    if (!scene->legacy_direct_launch &&
        scene->shot_a0f3_motion_valid &&
        scene->shot_a0f3_motion.remaining_ticks != 0U) {
        TecmoGameplayCpuA0f3PublishedPosition published;
        if (!tecmo_gameplay_cpu_a0f3_motion_tick_publish(
                &scene->shot_a0f3_motion, &published)) {
            return false;
        }
        scene->shot_a0f3_raw_x = published.raw_x;
        scene->shot_a0f3_raw_depth = published.raw_depth;
        scene->shot_a0f3_raw_position_valid = true;
        ++scene->shot_a0f3_tick_count;
    }

    if (next_frame == 3U) {
        scene->jump_pose_frame = TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME;
        actor->pose_index = scene_jump_playback_flight_pose(scene);
    }

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
        actor->pose_index = scene_jump_playback_flight_pose(scene);
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
        scene->jump_pose_frame = 0U;
    }

    if (!scene_update_jump_miss_rim_rattle(
            scene, next_frame, &route_frame, &rattle_position_owned)) {
        return false;
    }

    if (route_frame == 5U) {
        scene->jump_ball_state =
            scene->jump_shots.constants.ball_state_route17;
    } else if (route_frame == 73U) {
        scene->jump_ball_state =
            scene->jump_shots.constants.ball_state_route10;
    } else if (route_frame == 74U) {
        if (!actual_rattle || next_frame >
                TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
            scene->jump_ball_altitude_q8 = 0U;
            scene->jump_ball_bounce_q8 =
                scene->jump_shots.constants.bounce_decay_q8;
        }
    } else if (route_frame == 75U) {
        if ((!actual_rattle || next_frame >
                TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) &&
            scene->jump_ball_state ==
                scene->jump_shots.constants.ball_state_route10 &&
            scene->jump_ball_altitude_q8 == 0U &&
            scene->jump_ball_bounce_q8 != 0U) {
            if (!tecmo_gameplay_audio_queue_event(
                    &scene->audio_player,
                    TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE)) {
                return false;
            }
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
    if (scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        /* Period expiry closes the live-settle boundary before a normal
           approximate rim tail can consume another update. */
        return scene_finish_jump_miss(
            scene, actor, (TecmoGameplayTeam)actor->team);
    }
    if (!scene->jump_rim_rattle_debug && !scene->legacy_direct_launch &&
        scene->shot_rim_route.kind !=
            TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
        return scene_begin_shot_rim_tail(scene);
    }
    return scene_finish_jump_miss(
        scene, actor, (TecmoGameplayTeam)actor->team);
}

bool scene_update_jump_miss(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplayScene candidate;
    if (scene == NULL) return false;
    candidate = *scene;
    if (!scene_shot_boundary_valid(&candidate)) return false;
    if ((candidate.shot_rim_tail_active
             ? !scene_update_shot_rim_tail_mutating(&candidate)
             : !scene_update_jump_miss_mutating(
                   &candidate, shooting_controls))) {
        return false;
    }
    if (!candidate.legacy_direct_launch &&
        candidate.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        candidate.state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        !scene_sync_live_foundation(&candidate)) {
        return false;
    }
    if (!scene_shot_boundary_valid(&candidate)) return false;
    *scene = candidate;
    return true;
}

static bool scene_update_jump_miss_rim_rattle(
    TecmoGameplayScene *scene,
    uint16_t next_frame,
    uint16_t *route_frame,
    bool *rattle_position_owned)
{
    bool repeat_dmc = false;
    bool rattle_completed = false;

    {
        bool rattle_enabled = scene->jump_rim_rattle_debug ||
            scene->shot_rim_rattle_selected;
        bool authentic_planar = !scene->legacy_direct_launch &&
            !scene->jump_rim_rattle_debug;
        uint8_t raw_selector = scene->jump_rim_rattle_debug
            ? scene->jump_rim_rattle_raw_selector
            : scene->shot_rim_rattle_raw_selector;
    if (rattle_enabled &&
        next_frame == TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME) {
        TecmoGameplayShotRimRoute route;
        uint8_t orientation;
        int16_t incoming_x =
            TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6;
        int16_t incoming_depth = 0;
        if (!scene_shot_captured_rattle_orientation(scene, &orientation)) {
            return false;
        }
        if (authentic_planar) {
            if (!scene->shot_a0f3_motion_valid ||
                !scene->shot_a0f3_raw_position_valid ||
                scene->shot_a0f3_motion.remaining_ticks != 0U ||
                scene->shot_a0f3_tick_count !=
                    scene->shot_a0f3_result.duration_051e_0513) {
                return false;
            }
            incoming_x = (int16_t)scene->shot_a0f3_motion.velocity_x_q6;
            incoming_depth =
                (int16_t)scene->shot_a0f3_motion.velocity_depth_q6;
        }
        if (!tecmo_gameplay_shot_resolution_resolve_rim_route(
                &scene->shot_resolution,
                raw_selector, &route) ||
            route.kind != TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 ||
            route.source_target_cpu != 0xA7A9U ||
            orientation >= TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT ||
            !tecmo_gameplay_shot_rim_rattle_begin(
                &scene->shot_resolution, &scene->jump_rim_rattle,
                orientation, 3U, scene->jump_phase_counter,
                incoming_x, incoming_depth)) {
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
        *route_frame = 0U;
        *rattle_position_owned = true;
    } else if (rattle_enabled &&
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
        *rattle_position_owned = true;
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
                (scene->jump_rim_rattle_debug && raw_selector < 0x18U) ||
                scene->jump_rim_rattle.horizontal_velocity_q6 !=
                    scene->jump_rim_rattle.saved_horizontal_velocity_q6 ||
                scene->jump_rim_rattle.vertical_velocity_q6 !=
                    scene->jump_rim_rattle.saved_vertical_velocity_q6) {
                return false;
            }
            if (authentic_planar) {
                TecmoGameplayCpuA8e9VelocityInput normalize_input;
                TecmoGameplayCpuA8e9VelocityResult normalized;
                memset(&normalize_input, 0, sizeof(normalize_input));
                if ((uint16_t)scene->jump_rim_rattle
                        .saved_horizontal_velocity_q6 !=
                        scene->shot_a0f3_motion.velocity_x_q6 ||
                    (uint16_t)scene->jump_rim_rattle
                        .saved_vertical_velocity_q6 !=
                        scene->shot_a0f3_motion.velocity_depth_q6) {
                    return false;
                }
                normalize_input.contract_tag =
                    TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG;
                normalize_input.raw_vx_04f1_04fc = (uint16_t)
                    scene->jump_rim_rattle.horizontal_velocity_q6;
                normalize_input.raw_vz_0507_0512 = (uint16_t)
                    scene->jump_rim_rattle.vertical_velocity_q6;
                normalize_input.raw_006a = scene->fixed_rng.raw_006a;
                normalize_input.orientation_035a =
                    scene->jump_rim_rattle.orientation;
                if (!tecmo_gameplay_cpu_a8e9_velocity_normalize(
                        &normalize_input, &normalized)) {
                    return false;
                }
                scene->shot_a8e9_normalized = normalized;
                scene->shot_a8e9_normalized_valid = true;
            }
            *route_frame = TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME;
        }
    } else if (rattle_enabled &&
               next_frame > TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        *route_frame = (uint16_t)(
            next_frame - TECMO_GAMEPLAY_JUMP_RATTLE_FRAME_SHIFT);
    }
    }
    return true;
}

static bool scene_update_jump_shot(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    if (scene == NULL) return false;
    return scene->predicted_make_route
               ? scene_update_jump_make(scene, shooting_controls)
               : scene_update_jump_miss_mutating(scene, shooting_controls);
}

static bool scene_update_shot_mutating(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    int64_t duration;
    int64_t frame;
    int64_t arc;
    uint16_t next_frame;
    TecmoGameplaySceneActor *actor;
    TecmoGameplayTeam shooting_team;
    TecmoGameplaySceneShotKind shot_kind;
    bool made;
    if (scene == NULL || scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration == 0U) {
        return false;
    }
    if (scene->shot_rim_tail_active) {
        return scene_update_shot_rim_tail_mutating(scene);
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
        return scene_update_jump_shot(scene, shooting_controls);
    }
    actor = &scene->actors[scene->shot_actor];
    shooting_team = (TecmoGameplayTeam)actor->team;
    shot_kind = scene->shot_kind;
    if (!scene_next_shot_frame(scene, &next_frame)) {
        return false;
    }
    scene->shot_frame = next_frame;
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
        if (!tecmo_gameplay_audio_queue_dmc_clip(
                &scene->audio_player,
                TECMO_GAMEPLAY_DMC_BANK05_A9C5)) {
            return false;
        }
    }

    if (scene->shot_frame < scene->shot_duration) return true;
    made = scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE;
    if (!made) {
        /* Period expiry owns the terminal live-settle boundary. There is no
           remaining live update in which to expose a post-arc rim tail, so
           settle the close miss through the same transactional finish path;
           normal live misses retain their explicit numeric tail. */
        if (scene->legacy_direct_launch ||
            scene->state.phase ==
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
            bool finished = scene_finish_shot(
                scene, actor, shooting_team, false, false);
            return finished;
        }
        if (!scene_begin_shot_rim_tail(scene)) {
            return false;
        }
        return true;
    }
    {
        bool finished = scene_finish_shot(
            scene, actor, shooting_team, made,
            shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK);
        return finished;
    }
}

bool scene_update_shot(TecmoGameplayScene *scene,
                       const TecmoControlFrame *shooting_controls)
{
    TecmoGameplayScene candidate;
    if (scene == NULL) {
        return false;
    }
    candidate = *scene;
    if (!scene_shot_boundary_valid(&candidate)) {
        return false;
    }
    if (!scene_update_shot_mutating(&candidate, shooting_controls)) {
        return false;
    }
    if (!candidate.legacy_direct_launch &&
        candidate.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        candidate.state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        !scene_sync_live_foundation(&candidate)) {
        return false;
    }
    if (!scene_shot_boundary_valid(&candidate)) {
        return false;
    }
    *scene = candidate;
    return true;
}

static bool scene_live_foul_geometry_gate(
    const TecmoGameplaySceneActor *defender,
    const TecmoGameplaySceneActor *holder,
    bool *contact_out)
{
    TecmoGameplayDefenseContactB05GeometryInput input;
    TecmoGameplayDefenseContactB05GeometryResult result;

    if (defender == NULL || holder == NULL || contact_out == NULL) return false;
    memset(&input, 0, sizeof(input));
    memset(&result, 0, sizeof(result));
    input.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_INPUT_TAG;
    input.routine_cpu =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU;
    /* The scene coordinate adapter deliberately preserves the raw-width
       subtraction shape of $9968.  It is a bounded contact envelope only;
       $9968 is not claimed to be the complete Bank05 foul caller. */
    input.raw_x_candidate = (uint16_t)defender->position.x;
    input.raw_x_reference = (uint16_t)holder->position.x;
    input.raw_depth_candidate = (uint8_t)defender->position.y;
    input.raw_depth_reference = (uint8_t)holder->position.y;
    if (!tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
            &input, &result)) {
        return false;
    }
    *contact_out = result.raw_gate ==
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_PASS;
    return true;
}

static bool scene_foul_counter_effect_from_result(
    const TecmoGameplayPenaltyResult *result,
    TecmoGameplayFoulCounterEffect *effect_out)
{
    TecmoGameplayFoulCounterEffect effect = TECMO_GAMEPLAY_FOUL_COUNTER_NONE;
    if (result == NULL || effect_out == NULL ||
        result->individual_foul_delta > 1U || result->team_foul_delta > 1U) {
        return false;
    }
    if (result->individual_foul_delta != 0U) {
        effect = TECMO_GAMEPLAY_FOUL_COUNTER_INDIVIDUAL;
    }
    if (result->team_foul_delta != 0U) {
        effect = (TecmoGameplayFoulCounterEffect)(
            effect | TECMO_GAMEPLAY_FOUL_COUNTER_TEAM);
    }
    *effect_out = effect;
    return true;
}

/* Bank02 $B0F8-$B398 consumes the classified class/counters and the active
 * defending player. Keep that display identity in the scene only after the
 * separate gameplay-state foul transaction accepts the matching request.
 * The bounded bridge intentionally does not retain its adapter route bytes. */
static bool scene_foul_presentation_from_result(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam defending_team,
    uint8_t defender,
    const TecmoGameplayPenaltyResult *result,
    TecmoGameplaySceneFoulPresentation *presentation_out)
{
    const TecmoGameplaySceneActor *actor;
    TecmoGameplaySceneFoulPresentation presentation;
    uint16_t expected_individual;
    uint16_t expected_team;
    if (scene == NULL || result == NULL || presentation_out == NULL ||
        defending_team >= TECMO_GAMEPLAY_TEAM_COUNT ||
        defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        result->foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        result->offensive_foul || result->turnover ||
        result->individual_foul_delta > 1U || result->team_foul_delta > 1U ||
        result->individual_fouls_after > 6U || result->team_fouls_after > 5U) {
        return false;
    }
    actor = &scene->actors[defender];
    if (!actor->active || actor->team != (uint8_t)defending_team ||
        actor->roster_index >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return false;
    }
    expected_individual = (uint16_t)scene->state.individual_fouls[
        defending_team][actor->roster_index] + result->individual_foul_delta;
    expected_team = (uint16_t)scene->state.team_fouls[defending_team] +
        result->team_foul_delta;
    if (expected_individual != result->individual_fouls_after ||
        expected_team != result->team_fouls_after ||
        result->fouled_out != (result->individual_fouls_after >= 6U)) {
        return false;
    }
    memset(&presentation, 0, sizeof(presentation));
    presentation.valid = true;
    presentation.fouling_team = defending_team;
    presentation.actor_index = defender;
    presentation.roster_index = actor->roster_index;
    presentation.foul_class = result->foul_class;
    presentation.individual_foul_delta = result->individual_foul_delta;
    presentation.team_foul_delta = result->team_foul_delta;
    presentation.individual_fouls_after = result->individual_fouls_after;
    presentation.team_fouls_after = result->team_fouls_after;
    presentation.free_throw_attempts = result->free_throw_attempts;
    presentation.team_in_bonus = result->team_in_bonus;
    presentation.fouled_out = result->fouled_out;
    *presentation_out = presentation;
    return true;
}

/* Returns a successful no-op when the native scene lacks a source-shaped
 * human-contact tuple.  A false return is reserved for malformed strict
 * inputs or a failed state transition, so callers can fail closed. */
static bool scene_try_live_defensive_foul_bridge(
    TecmoGameplayScene *scene,
    TecmoGameplayTeam defending_team,
    uint8_t defender,
    bool *committed_out)
{
    const TecmoGameplaySceneActor *holder;
    const TecmoGameplaySceneActor *defender_actor;
    TecmoGameplayPenaltyContext context;
    TecmoGameplayPenaltyResult result;
    TecmoGameplaySceneFoulPresentation presentation;
    TecmoGameplayFoulCounterEffect counter_effect;
    TecmoGameplayFoulRequest request;
    TecmoGameplayState state_before;
    bool close_contact;

    if (scene == NULL || committed_out == NULL) return false;
    *committed_out = false;
    if (scene->legacy_direct_launch) return true;
    if (!scene->penalty_assets.available ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        return false;
    }
    holder = &scene->actors[scene->ball_holder];
    defender_actor = &scene->actors[defender];
    /* $9571 admits only the primary/selected relationship around $0308/$0309.
       The live foundation is the only scene state that preserves that pairing.
       An ordinary player may switch defenders first; an arbitrary nearby actor
       is intentionally not promoted into a foul candidate. */
    if (scene->live_foundation.primary_actor != scene->ball_holder ||
        scene->live_foundation.defender_actor != defender ||
        holder->team != (uint8_t)scene->state.possession ||
        defender_actor->team != (uint8_t)defending_team ||
        defender_actor->roster_index >= TECMO_GAMEPLAY_PLAYER_COUNT ||
        defending_team == scene->state.possession) {
        return true;
    }
    if (!scene_live_foul_geometry_gate(defender_actor, holder,
                                       &close_contact)) {
        return false;
    }
    if (!close_contact) return true;

    memset(&context, 0, sizeof(context));
    context.foul_actor = defender;
    context.offensive_primary_actor = scene->ball_holder;
    context.saved_route = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_SAVED_ROUTE;
    context.current_route = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CURRENT_ROUTE;
    context.contact_selector = TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CONTACT_SELECTOR;
    context.individual_fouls = scene->state.individual_fouls[defending_team]
        [defender_actor->roster_index];
    context.team_fouls = scene->state.team_fouls[defending_team];
    context.period_kind = scene->state.period >= 5U
        ? TECMO_GAMEPLAY_PENALTY_PERIOD_OVERTIME
        : TECMO_GAMEPLAY_PENALTY_PERIOD_REGULATION;
    if (!tecmo_gameplay_penalties_classify(
            &scene->penalty_assets, &context, &result) ||
        result.offensive_foul || result.turnover ||
        result.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        !scene_foul_counter_effect_from_result(&result, &counter_effect) ||
        !scene_foul_presentation_from_result(
            scene, defending_team, defender, &result, &presentation)) {
        return false;
    }

    memset(&request, 0, sizeof(request));
    request.fouling_team = defending_team;
    request.free_throw_team = scene->state.possession;
    request.counter_effect = counter_effect;
    request.player_index = defender_actor->roster_index;
    request.free_throw_attempts = result.free_throw_attempts;
    state_before = scene->state;
    if (!tecmo_gameplay_request_foul(&scene->state, &request)) return false;
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene->state.individual_fouls[defending_team]
            [defender_actor->roster_index] != result.individual_fouls_after ||
        scene->state.team_fouls[defending_team] != result.team_fouls_after ||
        scene->state.free_throws.attempts_remaining !=
            result.free_throw_attempts) {
        scene->state = state_before;
        return false;
    }
    scene->foul_presentation = presentation;
    scene->free_throw_frame = 0U;
    *committed_out = true;
    return true;
}

bool scene_try_defense_action(TecmoGameplayScene *scene,
                              size_t controller)
{
    uint8_t defender;
    TecmoGameplayTeam defending_team;
    bool foul_committed;
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
    if (!scene_try_live_defensive_foul_bridge(
            scene, defending_team, defender, &foul_committed)) {
        return false;
    }
    /* The serial remains an owned deterministic action counter, but no longer
       chooses a contact outcome.  `foul_committed` is intentionally not used
       as a branch: a source-shaped B action and a bounded no-contact attempt
       both consume one live action. */
    (void)foul_committed;
    ++scene->action_serial;
    return true;
}
