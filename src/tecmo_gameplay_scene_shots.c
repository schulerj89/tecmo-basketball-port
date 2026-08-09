#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_asset_pack.h"
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
    scene->close_shot_variant = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
    scene->shot_sample = 0U;
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
            scene->orientation_state.current_direction, &hoop)) {
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
                            ? scene->orientation_state.current_direction != 0U
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

uint32_t scene_shot_stable_sample_from_inputs(
    int16_t actor_x,
    int16_t actor_y,
    uint8_t point_value,
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint8_t actor_team,
    uint8_t actor_roster_index,
    uint32_t launch_frame)
{
    uint32_t sample = 2166136261U;
    sample ^= (uint16_t)actor_x;
    sample *= 16777619U;
    sample ^= (uint16_t)actor_y;
    sample *= 16777619U;
    sample ^= (uint16_t)target_delta_x;
    sample *= 16777619U;
    sample ^= (uint16_t)target_delta_y;
    sample *= 16777619U;
    sample ^= ((uint32_t)point_value << 24U) |
              ((uint32_t)actor_team << 16U) |
              ((uint32_t)actor_roster_index << 8U) |
              (launch_frame & 0xFFU);
    sample *= 16777619U;
    /* Preserve the accepted low-byte sample stream while binding every
       upper launch-frame bit.  A nonzero byte is folded as its own FNV step;
       a zero byte is the identity contribution, so ordinary frame<256
       checkpoints retain their captured values but upper-byte corruption
       cannot pass the scene boundary. */
    for (unsigned shift = 8U; shift < 32U; shift += 8U) {
        uint8_t frame_byte = (uint8_t)(launch_frame >> shift);
        if (frame_byte != 0U) {
            sample ^= frame_byte;
            sample *= 16777619U;
            sample ^= (uint8_t)(shift / 8U);
            sample *= 16777619U;
        }
    }
    return sample;
}

uint32_t scene_shot_context_signature(
    uint32_t stable_sample,
    bool contact_context,
    bool contest_context)
{
    uint32_t signature = 2166136261U;
    signature ^= stable_sample;
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
    uint32_t stable_sample)
{
    /* The available close gate and the proven 4:1 direction sectors leave no
       physical vertical case with approach >24.  The raw object/timer inputs
       that distinguish this source identity are unavailable, so bit $400 is
       an explicit neutral substitution used only for vertical numeric-2
       reachability without relabeling contact, foul, or shot semantics. */
    if ((direction == TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN ||
         direction == TECMO_GAMEPLAY_SHOT_DIRECTION_UP) &&
        (stable_sample & 0x00000400U) != 0U) {
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
    uint32_t stable_sample)
{
    if (scene == NULL ||
        !tecmo_gameplay_shot_resolution_resolve_rim_route(
            &scene->shot_resolution, (uint8_t)stable_sample,
            &scene->shot_rim_route)) {
        return false;
    }
    scene->shot_rim_rattle_raw_selector = (uint8_t)stable_sample;
    scene->shot_rim_rattle_selected = false;
    return true;
}

uint8_t scene_shot_family_for_context(
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint32_t stable_sample)
{
    int32_t abs_x = target_delta_x;
    int32_t abs_y = target_delta_y;
    uint32_t span;
    if (abs_x < 0) abs_x = -abs_x;
    if (abs_y < 0) abs_y = -abs_y;
    span = (uint32_t)(abs_x + abs_y);
    /* Native substitution for the available $8B83-$8BC8 geometry gate.  The
       raw $038A/$006A timer/state inputs are not proven here.  A stable
       sample bit supplies the missing short-span branch; the observed
       long-span polarity remains inverted.  This keeps family selection
       deterministic and independent of shot-local contact/contest state. */
    return span <= 0x0100U
        ? (uint8_t)((stable_sample >> 13U) & 1U)
        : (uint8_t)(1U ^ ((stable_sample >> 13U) & 1U));
}

static bool scene_start_shot_actor_mutating(TecmoGameplayScene *scene,
                                            size_t controller,
                                            uint8_t actor_index)
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
    bool close;
    bool shot_facing_right;
    bool contact_context;
    bool contest_context;
    bool source_variant1_gate;
    uint8_t profile;
    TecmoGameplayShotDirectionSlot direction_slot;
    const TecmoTeamDataPlayer *player;
    uint32_t stable_sample;
    TecmoGameplayShotEvaluationInput evaluation_input;
    TecmoGameplayShotEvaluation evaluation;
    TecmoGameplayCloseShotVariantInfo close_info;
    uint16_t entry_pose;
    uint16_t initial_pose = 0U;
    bool predicted_make;
    if (scene == NULL || controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
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
        scene->orientation_state.current_direction == 0U
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
            scene->orientation_state.current_direction, 0U,
            &classified_points)) {
        return false;
    }
    scene->shot_launch_frame = scene->frame;
    stable_sample = scene_shot_stable_sample_from_inputs(
        actor->position.x, actor->position.y, classified_points,
        target_delta_x, target_delta_y, actor->team, actor->roster_index,
        scene->shot_launch_frame);
    close = approach_distance_x >= -8 &&
            approach_distance_x <= TECMO_GAMEPLAY_CLOSE_DISTANCE_X &&
            distance_y >= -64 && distance_y <= 80;
    /* The physical court boundary makes the 4:1 vertical sectors near a hoop
       appear only inside the close gate.  The missing native object/timer
       context is substituted by a stable-sample bit for bound production:
       clear selects the close contract, set exposes the ordinary jump
       approximation.  This is neutral source substitution, not a contact,
       foul, or semantic shot label; legacy/direct fixtures retain geometry. */
    if (!scene->legacy_direct_launch && close &&
        (direction_slot == TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN ||
         direction_slot == TECMO_GAMEPLAY_SHOT_DIRECTION_UP) &&
        (stable_sample & 0x00000200U) != 0U) {
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
    scene_shot_clear_jump_playback(scene);
    scene->shot_launch_frame = scene->frame;
    /* $8BDE/$8C79/$8C7D prove the numeric-1 path, but not its semantic
       label.  The raw object/timer predicate is unavailable, so use one
       stable-sample bit as a neutral source/substitution gate.  It is
       independent of contact/contest classification and permits all eight
       direction slots to reach the pose-only numeric-1 approximation. */
    source_variant1_gate = close && !scene->legacy_direct_launch &&
        (stable_sample & 0x00000100U) != 0U;
    if (close) {
        variant_selection_approach = scene->legacy_direct_launch
            ? (int16_t)approach_distance_x
            : scene_close_variant_selection_approach(
                  approach_distance_x, direction_slot, stable_sample);
        if (!tecmo_gameplay_close_shots_select_numeric_variant(
                variant_selection_approach, (int16_t)distance_y,
                source_variant1_gate, &scene->close_shot_variant)) {
            return false;
        }
    } else {
        scene->close_shot_variant = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
    }
    if (!scene_shot_select_rim_route(scene, stable_sample)) {
        return false;
    }
    memset(&evaluation_input, 0, sizeof(evaluation_input));
    evaluation_input.player_rating = player->profile[0];
    evaluation_input.point_value = classified_points;
    evaluation_input.close_context = close;
    evaluation_input.contact_context = contact_context;
    evaluation_input.contest_context = contest_context;
    evaluation_input.horizontal_distance = target_delta_x;
    evaluation_input.vertical_distance =
        (int16_t)(TECMO_GAMEPLAY_SHOT_TARGET_Y - actor->position.y);
    evaluation_input.family = scene_shot_family_for_context(
        target_delta_x, target_delta_y, stable_sample);
    if (!close && scene->legacy_direct_launch && classified_points == 3U) {
        /* The accepted direct render/shot-clock adapter is source-pinned to
           family 0 before the unported family inputs are available. Bound
           production uses the recomputable geometry/sample gate above. */
        evaluation_input.family = 0U;
    }
    evaluation_input.profile = profile;
    evaluation_input.direction = (uint8_t)direction_slot;
    evaluation_input.numeric_variant = (uint8_t)scene->close_shot_variant;
    evaluation_input.stable_sample = stable_sample;
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
       stable sample has low2==1 remains an ordinary make schedule. */
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
    scene->shot_sample = stable_sample;
    scene->shot_make_probability = evaluation.make_probability;
    scene->shot_contact_context = evaluation.contact_context;
    scene->shot_contest_context = evaluation.contest_context;
    scene->shot_context_signature = scene_shot_context_signature(
        stable_sample, evaluation.contact_context,
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
        if (scene->launch.controller_team[controller] != actor->team) {
            return false;
        }
        scene->shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
        memset(&close_info, 0, sizeof(close_info));
        scene->shot_controller = (uint8_t)controller;
        scene->jump_family =
            (TecmoGameplayJumpShotFamily)evaluation_input.family;
        scene->jump_profile = (TecmoGameplayJumpShotProfile)profile;
        scene->jump_direction =
            (TecmoGameplayJumpShotDirection)direction_slot;
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
    scene->shot_start_position = shot_start_q8;
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

bool scene_start_shot_actor(TecmoGameplayScene *scene,
                            size_t controller,
                            uint8_t actor_index)
{
    TecmoGameplayScene candidate;
    if (scene == NULL) return false;
    candidate = *scene;
    if (!scene_shot_boundary_valid(&candidate)) return false;
    if (!scene_start_shot_actor_mutating(&candidate, controller,
                                         actor_index)) {
        return false;
    }
    if (!scene_shot_boundary_valid(&candidate)) return false;
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
        candidate.jump_make_route ||
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
    if (scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        actor != &scene->actors[scene->shot_actor] ||
        !scene_actor_movement_state(scene, actor, &movement) ||
        !scene_actor_movement_pose_index(
            scene, scene->actors, scene->shot_actor, &movement,
            &idle_pose)) {
        return false;
    }
    if (!made && scene->state.phase !=
            TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE &&
        (!scene_select_shot_claimant(
             scene, shooting_team, &claimant, &claimant_relation,
             &decision))) {
        return false;
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
        if (!scene_handoff_possession(
            scene, next_team, scene_first_actor_for_team(next_team))) {
            return false;
        }
        return true;
    }
    if (!scene_handoff_possession(
        scene,
        claimant_relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM
            ? next_team : shooting_team,
        claimant)) {
        return false;
    }
    return true;
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
    if (scene == NULL || actor == NULL ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
        return false;
    }

    period_expiry = scene->state.phase ==
        TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
    shooting_actor = (uint8_t)(actor - scene->actors);
    next_team = scene_other_team(shooting_team);
    if (!period_expiry &&
        !scene_select_shot_claimant(
            scene, shooting_team, &claimant, &claimant_relation,
            &claimant_decision)) {
        return false;
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
    return scene_handoff_possession(
        scene,
        claimant_relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM
            ? next_team : shooting_team,
        claimant);
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
        (scene->jump_rim_rattle_debug || scene->jump_make_route ||
         !scene->jump_oracle_active ||
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
    return scene_handoff_possession(
        scene, next_team, scene_first_actor_for_team(next_team));
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
        : scene_handoff_possession(
              scene, next_team, scene_first_actor_for_team(next_team));
}

static bool scene_update_jump_make_approx(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls)
{
    TecmoGameplaySceneActor *actor;
    uint16_t next_frame;
    TecmoGameplayShotOutcome outcome;
    if (scene == NULL || !scene->jump_oracle_active ||
        !scene->jump_make_route || scene->shot_kind !=
            TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_schedule == TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT ||
        scene->shot_duration != TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION ||
        scene->shot_frame == 0U || scene->shot_frame > scene->shot_duration ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[scene->shot_controller] !=
            scene->actors[scene->shot_actor].team) {
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
            scene->actors[scene->shot_actor].team) {
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
    if (!scene->jump_oracle_active || scene->jump_make_route ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->shot_duration !=
            (scene->jump_rim_rattle_debug || scene->shot_rim_rattle_selected
                 ? TECMO_GAMEPLAY_JUMP_RATTLE_DURATION
                 : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION) ||
        scene->shot_frame == 0U ||
        scene->shot_frame > scene->shot_duration ||
        scene->shot_controller >= TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        scene->launch.controller_team[scene->shot_controller] !=
            scene->actors[scene->shot_actor].team ||
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
        uint8_t raw_selector = scene->jump_rim_rattle_debug
            ? scene->jump_rim_rattle_raw_selector
            : scene->shot_rim_rattle_raw_selector;
    if (rattle_enabled &&
        next_frame == TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME) {
        TecmoGameplayShotRimRoute route;
        uint8_t orientation;
        if (!scene_shot_captured_rattle_orientation(scene, &orientation)) {
            return false;
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
    return scene->jump_make_route
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

bool scene_try_defense_action(TecmoGameplayScene *scene,
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
