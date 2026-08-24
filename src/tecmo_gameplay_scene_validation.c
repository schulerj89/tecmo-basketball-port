#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_internal.h"

#include <limits.h>
#include <string.h>

/* Fail-closed cross-module scene invariants.  This lower layer is shared by
   orchestration, court snapshots, and production rendering. */

static bool scene_shot_route_metadata_valid(
    const TecmoGameplayScene *scene)
{
    uint8_t selector;
    if (scene == NULL || !scene->shot_resolution.available ||
        scene->shot_resolution.route_selector_mask != 0x03U) {
        return false;
    }
    selector = (uint8_t)(scene->shot_rim_rattle_raw_selector &
                         scene->shot_resolution.route_selector_mask);
    if (scene->shot_rim_route.selector != selector) return false;
    switch (scene->shot_rim_route.kind) {
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708:
        return scene->shot_rim_route.source_target_cpu == 0xA708U &&
               (selector == 0U || selector == 3U);
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9:
        return scene->shot_rim_route.source_target_cpu == 0xA7A9U &&
               selector == 1U;
    case TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9:
        return scene->shot_rim_route.source_target_cpu == 0xA8E9U &&
               selector == 2U;
    default:
        return false;
    }
}

static bool scene_shot_rattle_zero(
    const TecmoGameplayShotRimRattle *rattle)
{
    return rattle != NULL && !rattle->active && !rattle->complete &&
           rattle->object_state == 0U && rattle->orientation == 0U &&
           rattle->timer_remaining == 0U && rattle->passes_remaining == 0U &&
           rattle->animation_phase == 0U && rattle->altitude == 0U &&
           rattle->x == 0 && rattle->y == 0 &&
           rattle->horizontal_velocity_q6 == 0 &&
           rattle->vertical_velocity_q6 == 0 &&
           rattle->saved_horizontal_velocity_q6 == 0 &&
           rattle->saved_vertical_velocity_q6 == 0 &&
           rattle->render_script_address == 0U;
}

static bool scene_shot_made_settlement_zero(
    const TecmoGameplayJumpShotMadeSettlement *settlement)
{
    return settlement != NULL && settlement->state == 0U &&
           settlement->timer == 0U && settlement->stage == 0U &&
           settlement->updates == 0U && !settlement->complete;
}

static bool scene_validation_expected_rattle(
    const TecmoGameplayScene *scene,
    uint8_t animation_phase,
    uint8_t steps,
    TecmoGameplayShotRimRattle *rattle_out,
    TecmoGameplayCourtCoordinateQ8 *position_out);

/* Bound production stores the TGOR-selected hoop X in the shot endpoint at
   launch.  Recompute selector deltas from that captured X plus the validated
   TGOR hoop Y, not from a later live orientation transition, so a coordinated
   profile/direction/family/pose mutation cannot pass the shot boundary. */
static bool scene_shot_captured_target_delta(
    const TecmoGameplayScene *scene,
    int16_t *target_delta_x,
    int16_t *target_delta_y)
{
    int32_t launch_x;
    int32_t launch_y;
    int32_t target_x;
    int32_t target_y;
    int32_t expected_start_x_q8;
    int32_t expected_start_y_q8;
    int32_t expected_end_x_q8;
    int32_t expected_end_y_q8;
    int32_t delta_x;
    int32_t delta_y;
    if (scene == NULL || target_delta_x == NULL ||
        target_delta_y == NULL) {
        return false;
    }
    launch_x = scene->shot_actor_launch_position.x;
    launch_y = scene->shot_actor_launch_position.y;
    target_x = scene->shot_end_position.x_q8 / 256;
    target_y = TECMO_GAMEPLAY_COURT_HOOP_Y;
    expected_start_x_q8 = (launch_x +
        (scene->shot_launch_facing_right ? 7 : -7)) * 256;
    expected_start_y_q8 = (launch_y - 18) * 256;
    expected_end_x_q8 = target_x * 256;
    expected_end_y_q8 = TECMO_GAMEPLAY_SHOT_TARGET_Y * 256;
    if ((!scene->legacy_direct_launch &&
         scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP
             ? (scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS &&
                !scene->jump_rim_rattle_debug
                    ? (!scene->shot_a0f3_origin_valid ||
                scene->shot_a0f3_origin_x != (uint16_t)(
                    ((uint16_t)((uint32_t)scene->shot_start_position.x_q8 >>
                                16U) << 8U) |
                    (uint8_t)((uint32_t)scene->shot_start_position.x_q8 >>
                              8U)) ||
                scene->shot_a0f3_origin_depth != (uint8_t)(
                    (uint32_t)scene->shot_start_position.y_q8 >> 8U))
                    : false)
             : (scene->shot_start_position.x_q8 != expected_start_x_q8 ||
                scene->shot_start_position.y_q8 != expected_start_y_q8)) ||
        scene->shot_end_position.x_q8 != expected_end_x_q8 ||
        scene->shot_end_position.y_q8 != expected_end_y_q8 ||
        (target_x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X &&
         target_x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X)) {
        return false;
    }
    delta_x = target_x - launch_x;
    delta_y = target_y - launch_y;
    if (delta_x < INT16_MIN || delta_x > INT16_MAX ||
        delta_y < INT16_MIN || delta_y > INT16_MAX) {
        return false;
    }
    *target_delta_x = (int16_t)delta_x;
    *target_delta_y = (int16_t)delta_y;
    return *target_delta_x == scene->shot_target_delta_x &&
           *target_delta_y == scene->shot_target_delta_y &&
           !(*target_delta_x == 0 && *target_delta_y == 0);
}

static bool scene_shot_expected_close_context(
    const TecmoGameplayScene *scene,
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint32_t native_policy_sample,
    bool *close_out)
{
    TecmoGameplayShotDirectionSlot direction;
    int32_t target_x;
    int approach_distance_x;
    int distance_y;
    bool close;
    if (scene == NULL || close_out == NULL ||
        !tecmo_gameplay_shot_resolution_direction_for_delta(
            target_delta_x, target_delta_y, &direction)) {
        return false;
    }
    target_x = scene->shot_end_position.x_q8 / 256;
    approach_distance_x = target_x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X
        ? (int)scene->shot_actor_launch_position.x - (int)target_x
        : (int)target_x - (int)scene->shot_actor_launch_position.x;
    distance_y = TECMO_GAMEPLAY_SHOT_TARGET_Y -
        (int)scene->shot_actor_launch_position.y;
    close = approach_distance_x >= -8 &&
        approach_distance_x <= TECMO_GAMEPLAY_CLOSE_DISTANCE_X &&
        distance_y >= -64 && distance_y <= 80;
    if (close &&
        (direction == TECMO_GAMEPLAY_SHOT_DIRECTION_DOWN ||
         direction == TECMO_GAMEPLAY_SHOT_DIRECTION_UP) &&
        (native_policy_sample & 0x00000200U) != 0U) {
        close = false;
    }
    *close_out = close;
    return true;
}

static bool scene_shot_bound_selectors_valid(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    TecmoGameplayShotDirectionSlot direction;
    uint8_t profile;
    int16_t target_delta_x;
    int16_t target_delta_y;
    int32_t target_x;
    int approach_distance_x;
    int distance_y;
    TecmoGameplayCloseShotVariant expected_variant;
    int16_t variant_selection_approach;
    bool expected_close;
    if (scene == NULL || scene->legacy_direct_launch ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return true;
    }
    actor = &scene->actors[scene->shot_actor];
    player = scene_actor_player(scene, actor);
    if (player == NULL ||
        scene->shot_actor_team != actor->team ||
        scene->shot_actor_roster_index != actor->roster_index ||
        actor->facing_right != scene->shot_launch_facing_right ||
        !scene_shot_captured_target_delta(
            scene, &target_delta_x, &target_delta_y) ||
        !tecmo_gameplay_shot_profile_from_profile_byte2(
            player->profile[2], &profile) ||
        !tecmo_gameplay_shot_resolution_direction_for_delta(
            target_delta_x, target_delta_y, &direction)) {
        return false;
    }
    if (!scene_shot_expected_close_context(
            scene, target_delta_x, target_delta_y, scene->native_policy_sample,
            &expected_close) ||
        scene->shot_close_context != expected_close ||
        scene_shot_is_close(scene->shot_kind) != expected_close) {
        return false;
    }
    if (expected_close) {
        target_x = scene->shot_end_position.x_q8 / 256;
        approach_distance_x = target_x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X
            ? (int)scene->shot_actor_launch_position.x - (int)target_x
            : (int)target_x - (int)scene->shot_actor_launch_position.x;
        distance_y = TECMO_GAMEPLAY_SHOT_TARGET_Y -
            (int)scene->shot_actor_launch_position.y;
        variant_selection_approach =
            scene_close_variant_selection_approach(
                approach_distance_x, direction, scene->native_policy_sample);
        if (!tecmo_gameplay_close_shots_select_numeric_variant(
                variant_selection_approach, (int16_t)distance_y,
                (scene->native_policy_sample & 0x00000100U) != 0U,
                &expected_variant)) {
            return false;
        }
        return scene->close_shot_profile ==
                   (TecmoGameplayCloseShotProfile)profile &&
               scene->close_shot_direction ==
                   (TecmoGameplayCloseShotDirection)direction &&
               scene->close_shot_variant == expected_variant;
    }
    /* Bank05 $842C consumes the selector tuple assembled by the source
       path: family base ($038C), retained profile bit ($04B0 bit 5), and
       $05A0's 8-way hoop vector.  Native owns only the reset family 0; it
       must nevertheless validate the owned profile and direction rather
       than silently accepting the historical captured 0/0/1 fixture. */
    return scene->jump_profile == (TecmoGameplayJumpShotProfile)profile &&
           scene->jump_direction == (TecmoGameplayJumpShotDirection)direction &&
           scene->jump_family == (TecmoGameplayJumpShotFamily)
               scene_shot_family_for_context(
                   target_delta_x, target_delta_y, scene->native_policy_sample);
}

static bool scene_shot_bound_evaluation_valid(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *actor;
    const TecmoTeamDataPlayer *player;
    TecmoGameplayShotEvaluationInput input;
    TecmoGameplayShotEvaluation evaluation;
    TecmoGameplayShotDirectionSlot direction;
    int16_t target_delta_x;
    int16_t target_delta_y;
    uint8_t profile;
    uint8_t family;
    uint8_t captured_orientation;
    uint8_t point_value;
    int32_t target_x;
    uint32_t expected_native_policy_sample;
    bool expected_facing_right;
    bool close;
    if (scene == NULL || scene->legacy_direct_launch) return true;
    actor = &scene->actors[scene->shot_actor];
    player = scene_actor_player(scene, actor);
    if (player == NULL ||
        scene->shot_actor_team != actor->team ||
        scene->shot_actor_roster_index != actor->roster_index ||
        actor->facing_right != scene->shot_launch_facing_right ||
        !scene_shot_captured_target_delta(
            scene, &target_delta_x, &target_delta_y) ||
        !tecmo_gameplay_shot_profile_from_profile_byte2(
            player->profile[2], &profile) ||
        !tecmo_gameplay_shot_resolution_direction_for_delta(
            target_delta_x, target_delta_y, &direction)) {
        return false;
    }
    target_x = scene->shot_end_position.x_q8 / 256;
    if (target_x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X) {
        captured_orientation = 0U;
    } else if (target_x == TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X) {
        captured_orientation = 1U;
    } else {
        return false;
    }
    if ((scene->shot_flags &
            (uint8_t)~scene->shot_resolution.point_shot_flags_mask) != 0U ||
        !tecmo_gameplay_shot_resolution_classify_point_value(
            &scene->shot_resolution,
            (uint16_t)scene->shot_actor_launch_position.x,
            (uint8_t)scene->shot_actor_launch_position.y,
            captured_orientation, scene->shot_flags, &point_value) ||
        point_value != scene->shot_points) {
        return false;
    }
    expected_native_policy_sample = scene_shot_native_policy_sample_from_inputs(
        scene->shot_actor_launch_position.x,
        scene->shot_actor_launch_position.y,
        point_value, target_delta_x, target_delta_y,
        scene->shot_actor_team, scene->shot_actor_roster_index,
        scene->shot_launch_frame);
    expected_facing_right = target_delta_x > 0 ||
        (target_delta_x == 0 && captured_orientation != 0U);
    if (scene->native_policy_sample != expected_native_policy_sample ||
        scene->shot_launch_facing_right != expected_facing_right) {
        return false;
    }
    if (!scene_shot_expected_close_context(
            scene, target_delta_x, target_delta_y,
            expected_native_policy_sample, &close) ||
        scene->shot_close_context != close ||
        scene_shot_is_close(scene->shot_kind) != close) {
        return false;
    }
    family = scene_shot_family_for_context(
        target_delta_x, target_delta_y, expected_native_policy_sample);
    memset(&input, 0, sizeof(input));
    input.player_rating = player->profile[0];
    input.point_value = point_value;
    input.close_context = close;
    input.contact_context = scene->shot_contact_context;
    input.contest_context = scene->shot_contest_context;
    input.horizontal_distance = target_delta_x;
    input.vertical_distance = (int16_t)(
        TECMO_GAMEPLAY_SHOT_TARGET_Y -
        (int)scene->shot_actor_launch_position.y);
    /* The Bank05 $842C tuple is shared by ordinary and close source paths.
       Family remains the bounded reset-family approximation, while the
       profile-bit and eight-way direction are retained exactly. */
    input.family = family;
    input.profile = profile;
    input.direction = (uint8_t)direction;
    input.numeric_variant = close
        ? (uint8_t)scene->close_shot_variant : 0U;
    input.native_policy_sample = expected_native_policy_sample;
    if (!tecmo_gameplay_shot_resolution_evaluate(
            &scene->shot_resolution, &input, &evaluation) ||
        evaluation.point_value != scene->shot_points ||
        evaluation.make_probability != scene->shot_make_probability ||
        evaluation.contact_context != scene->shot_contact_context ||
        evaluation.contest_context != scene->shot_contest_context ||
        evaluation.outcome != scene->shot_outcome ||
        evaluation.schedule != scene->shot_schedule) {
        return false;
    }
    return true;
}

static bool scene_shot_rattle_state_valid(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplayShotRimRattle *rattle;
    bool rattle_route;
    if (scene == NULL) return false;
    if (scene->jump_rim_rattle_debug &&
        scene->jump_rim_rattle_raw_selector != 0x71U) {
        return false;
    }
    if (!scene->jump_rim_rattle_debug &&
        scene->jump_rim_rattle_raw_selector != 0U) {
        return false;
    }
    if (scene->shot_rim_rattle_selected &&
        (scene->legacy_direct_launch ||
         scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
         scene->shot_rim_route.kind != TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9)) {
        return false;
    }
    rattle = &scene->jump_rim_rattle;
    rattle_route = scene->shot_rim_route.kind ==
        TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9;
    if (!rattle_route && scene->jump_rim_rattle_debug &&
        (scene->jump_rim_rattle_raw_selector & 0x03U) == 1U) {
        /* The diagnostic deliberately overrides the launch-time route with
           observed raw $6A=$71. This is an explicit A7A9 debug identity, not
           a general permission for a route/metadata mismatch. */
        rattle_route = true;
    }
    if (rattle->active && rattle->complete) return false;
    if (rattle->active) {
        if (!rattle_route || scene->shot_outcome !=
                TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
            rattle->altitude != scene->shot_resolution.rim_rattle.altitude ||
            (scene->shot_rim_tail_active &&
             scene->jump_ball_altitude_q8 !=
                 (uint16_t)scene->shot_resolution.rim_rattle.altitude <<
                     8U)) {
            return false;
        }
    }
    if (rattle->complete && !rattle_route) {
        return false;
    }
    if (scene->shot_rim_tail_active && rattle_route) {
        uint8_t frame = scene->shot_rim_tail_frame;
        if (scene->shot_rim_tail_duration !=
                TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES +
                    TECMO_GAMEPLAY_SHOT_RIM_TAIL_GROUND_UPDATE ||
            frame > TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES) {
            return false;
        }
        if (frame < TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES) {
            if (!rattle->active || rattle->complete ||
                scene->jump_ball_altitude_q8 !=
                    (uint16_t)scene->shot_resolution.rim_rattle.altitude <<
                        8U) {
                return false;
            }
        } else if (frame == TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES &&
                   (!rattle->complete || rattle->active ||
                    scene->jump_ball_altitude_q8 !=
                        (uint16_t)scene->shot_resolution.rim_rattle.altitude <<
                            8U)) {
            return false;
        }
    }
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
        (scene->jump_rim_rattle_debug || scene->shot_rim_rattle_selected)) {
        uint16_t frame = scene->shot_frame;
        if (frame >= TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME &&
            frame <= TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME + 13U) {
            uint8_t steps = frame <= TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME
                ? (uint8_t)(frame - TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME)
                : TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
            TecmoGameplayShotRimRattle expected;
            TecmoGameplayCourtCoordinateQ8 ignored_position;
            if (!scene_validation_expected_rattle(
                    scene, 0U, steps, &expected, &ignored_position) ||
                memcmp(&expected, rattle, sizeof(expected)) != 0) {
                return false;
            }
        }
    }
    if (!rattle->active && !rattle->complete &&
        !scene_shot_rattle_zero(rattle)) {
        return false;
    }
    return true;
}

static bool scene_shot_pose_state_valid(
    const TecmoGameplayScene *scene)
{
    const TecmoGameplaySceneActor *actor;
    uint16_t expected_pose;
    if (scene == NULL || scene->shot_actor >=
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    actor = &scene->actors[scene->shot_actor];
    if (scene_shot_is_close(scene->shot_kind)) {
        TecmoGameplayCloseShotVariantInfo info;
        uint16_t selected;
        uint16_t source_frame = scene->shot_rim_tail_active
            ? scene->shot_rim_tail_base_frame : scene->shot_frame;
        uint8_t expected_step;
        if (scene->shot_kind ==
                       TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1) {
            selected = source_frame < TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION
                ? source_frame
                : (uint16_t)(TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION - 1U);
            expected_step = (uint8_t)selected;
        } else if (!tecmo_gameplay_close_shots_get_variant_info(
                       &scene->close_shots,
                       scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                           ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
                           : TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                       &info)) {
            return false;
        } else if (scene->shot_kind ==
                       TECMO_GAMEPLAY_SCENE_SHOT_DUNK) {
            selected = source_frame <= 22U
                ? source_frame
                : source_frame < TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME
                    ? 22U
                    : (uint16_t)(22U + source_frame -
                        (TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME - 1U));
            if (selected >= info.step_count) {
                selected = (uint16_t)(info.step_count - 1U);
            }
            expected_step = (uint8_t)selected;
        } else {
            selected = source_frame < info.step_count
                ? source_frame : (uint16_t)(info.step_count - 1U);
            expected_step = (uint8_t)selected;
        }
        if (!scene_close_pose_for_step(
                scene, expected_step, &expected_pose) ||
            scene->close_shot_step != expected_step ||
            actor->pose_index != expected_pose) {
            return false;
        }
        return true;
    }
    if (!tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
            &scene->jump_shots, scene->jump_family, scene->jump_profile,
            scene->jump_direction, &expected_pose) ||
        scene->jump_resolved_pose_index != expected_pose) {
        return false;
    }
    if (!scene->jump_b_released) {
        if (scene->predicted_make_route && scene->shot_schedule ==
                TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION) {
            /* The bounded approximate make holds its selected entry/gather
               pose through frames 1..5; unlike the captured exact/miss
               controller it does not turn at pose-frame 5. */
            expected_pose = scene->jump_entry_pose_index;
            return scene->jump_pose_frame >= 1U &&
                   scene->jump_pose_frame <=
                       TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME &&
                   actor->pose_index == expected_pose;
        }
        expected_pose = scene->jump_pose_frame <=
                TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME
            ? scene->jump_entry_pose_index
            : TECMO_GAMEPLAY_JUMP_TURN_POSE;
        return scene->jump_pose_frame >= 1U &&
               scene->jump_pose_frame <=
                   TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME &&
               actor->pose_index == expected_pose;
    }
    {
        uint16_t phase_pose = 0U;
        uint16_t release_frame = scene->predicted_make_route
            ? (scene->shot_schedule ==
                   TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION
                   ? TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME
                   : TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME)
            : 2U;
        uint16_t neutral_frame = scene->predicted_make_route
            ? (scene->shot_schedule ==
                   TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION
                   ? TECMO_GAMEPLAY_JUMP_APPROX_MAKE_NEUTRAL_FRAME
                   : TECMO_GAMEPLAY_JUMP_MAKE_NEUTRAL_FRAME)
            : 46U;
        if (!scene->legacy_direct_launch &&
            !tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
                &scene->jump_shots, scene->jump_family, scene->jump_profile,
                scene->jump_direction, scene->jump_phase_counter,
                &phase_pose)) {
            return false;
        }
        if (scene->shot_frame == release_frame) {
            return scene->jump_pose_frame ==
                       TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME &&
                   actor->pose_index == (scene->legacy_direct_launch
                       ? TECMO_GAMEPLAY_JUMP_RELEASE_POSE : phase_pose);
        }
        if (scene->shot_frame < neutral_frame) {
            return scene->jump_pose_frame ==
                       TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME &&
                   actor->pose_index == (scene->legacy_direct_launch
                       ? TECMO_GAMEPLAY_JUMP_FLIGHT_POSE
                       : phase_pose);
        }
        return scene->jump_pose_frame == 0U &&
               actor->pose_index == TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE;
    }
}

static int32_t scene_validation_lerp_q8(
    int32_t start,
    int32_t end,
    unsigned step,
    unsigned duration)
{
    if (duration == 0U || step >= duration) return end;
    return start + (int32_t)(((int64_t)(end - start) *
                              (int64_t)step) / (int64_t)duration);
}

/* Reconstruct the source-backed rattle object locally.  The live object is
   compared with this replay; it is never used as the expected-position input.
   Close tails use their captured base-frame phase; the captured jump route
   starts at the neutral phase used by the $73 handoff. */
static bool scene_validation_expected_rattle(
    const TecmoGameplayScene *scene,
    uint8_t animation_phase,
    uint8_t steps,
    TecmoGameplayShotRimRattle *rattle_out,
    TecmoGameplayCourtCoordinateQ8 *position_out)
{
    const TecmoGameplayCloseShotSourceSpan *source;
    uint16_t source_target_x;
    uint16_t source_snap_x;
    uint8_t orientation;
    bool repeat_dmc;
    bool completed;
    int16_t incoming_x =
        TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6;
    int16_t incoming_depth = 0;
    if (scene == NULL || rattle_out == NULL || position_out == NULL ||
        !scene->shot_resolution.available) {
        return false;
    }
    if (!scene_shot_captured_rattle_orientation(scene, &orientation) ||
        orientation >= TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT) {
        return false;
    }
    memset(rattle_out, 0, sizeof(*rattle_out));
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_JUMP &&
        !scene->legacy_direct_launch && !scene->jump_rim_rattle_debug) {
        if (!scene->shot_a0f3_motion_valid ||
            scene->shot_a0f3_motion.remaining_ticks != 0U ||
            scene->shot_a0f3_tick_count !=
                scene->shot_a0f3_result.duration_051e_0513) {
            return false;
        }
        incoming_x = (int16_t)scene->shot_a0f3_motion.velocity_x_q6;
        incoming_depth =
            (int16_t)scene->shot_a0f3_motion.velocity_depth_q6;
    }
    if (!tecmo_gameplay_shot_rim_rattle_begin(
            &scene->shot_resolution, rattle_out, orientation, 3U,
            animation_phase, incoming_x, incoming_depth)) {
        return false;
    }
    for (uint8_t step = 0U; step < steps; ++step) {
        repeat_dmc = false;
        completed = false;
        if (!tecmo_gameplay_shot_rim_rattle_step(
                &scene->shot_resolution, rattle_out, &repeat_dmc,
                &completed)) {
            return false;
        }
    }
    source = tecmo_gameplay_close_shots_find_source(
        &scene->close_shots,
        TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BDEF_BDF6);
    if (source == NULL || source->bytes == NULL ||
        source->byte_count != 8U || source->cpu_start != 0xBDEFU ||
        source->cpu_end != 0xBDF6U ||
        scene->shot_resolution.rim_rattle.start_y !=
            TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y + 4U) {
        return false;
    }
    source_target_x = (uint16_t)(
        (uint16_t)source->bytes[orientation] |
        ((uint16_t)source->bytes[2U + orientation] << 8U));
    source_snap_x = (uint16_t)(
        (uint16_t)source->bytes[4U + orientation] |
        ((uint16_t)source->bytes[6U + orientation] << 8U));
    if (source_snap_x !=
            scene->shot_resolution.rim_rattle.orientation_start_x[
                orientation]) {
        return false;
    }
    position_out->x_q8 = scene->shot_end_position.x_q8 +
        ((int32_t)rattle_out->x - (int32_t)source_target_x) * 256;
    position_out->y_q8 = scene->shot_end_position.y_q8 +
        ((int32_t)rattle_out->y -
         TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y) * 256;
    return true;
}

static bool scene_validation_expected_tail_ball_position(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayCourtCoordinateQ8 expected;
    uint8_t selector;
    if (scene == NULL || !scene->shot_rim_tail_active) return false;
    if (scene->jump_rim_rattle_debug || scene->shot_rim_rattle_selected) {
        TecmoGameplayShotRimRattle rattle;
        uint8_t phase = scene_shot_is_close(scene->shot_kind)
            ? (uint8_t)(scene->shot_rim_tail_base_frame & 0x0FU) : 0U;
        if (!scene_validation_expected_rattle(
                scene, phase, scene->shot_rim_tail_frame,
                &rattle, &expected) ||
            memcmp(&rattle, &scene->jump_rim_rattle,
                   sizeof(rattle)) != 0) {
            return false;
        }
        return scene->ball_position.x_q8 == expected.x_q8 &&
               scene->ball_position.y_q8 == expected.y_q8;
    }
    if (scene->shot_rim_tail_frame == 0U) {
        expected = scene->shot_end_position;
    } else {
        selector = (uint8_t)(scene->shot_rim_rattle_raw_selector & 0x03U);
        if (scene->shot_rim_route.kind ==
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 && selector == 0U) {
            expected.x_q8 = scene->shot_end_position.x_q8 -
                5 * 256 + (int32_t)scene->shot_rim_tail_frame * 64;
            expected.y_q8 = scene->shot_end_position.y_q8 -
                4 * 256 + (int32_t)scene->shot_rim_tail_frame * 64;
        } else if (scene->shot_rim_route.kind ==
                       TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 && selector == 3U) {
            expected.x_q8 = scene->shot_end_position.x_q8 +
                5 * 256 - (int32_t)scene->shot_rim_tail_frame * 64;
            expected.y_q8 = scene->shot_end_position.y_q8 -
                4 * 256 + (int32_t)scene->shot_rim_tail_frame * 32;
        } else if (scene->shot_rim_route.kind ==
                       TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9) {
            expected.x_q8 = scene->shot_end_position.x_q8;
            expected.y_q8 = scene->shot_end_position.y_q8 -
                8 * 256 + (int32_t)scene->shot_rim_tail_frame * 128;
        } else {
            return false;
        }
    }
    return scene->ball_position.x_q8 == expected.x_q8 &&
           scene->ball_position.y_q8 == expected.y_q8;
}

static bool scene_validation_expected_ball_position(
    const TecmoGameplayScene *scene)
{
    TecmoGameplayCourtCoordinateQ8 expected;
    int32_t apex_y;
    int64_t arc;
    int64_t duration;
    int64_t frame;
    if (scene == NULL) return false;
    if (scene->shot_rim_tail_active) {
        return scene_validation_expected_tail_ball_position(scene);
    }
    if (scene_shot_is_close(scene->shot_kind)) {
        duration = scene->shot_duration;
        frame = scene->shot_frame < scene->shot_duration
            ? scene->shot_frame : scene->shot_duration;
        expected.x_q8 = scene->shot_start_position.x_q8 + (int32_t)(
            ((int64_t)(scene->shot_end_position.x_q8 -
                       scene->shot_start_position.x_q8) * frame) /
            duration);
        expected.y_q8 = scene->shot_start_position.y_q8 + (int32_t)(
            ((int64_t)(scene->shot_end_position.y_q8 -
                       scene->shot_start_position.y_q8) * frame) /
            duration);
        arc = (4LL * frame * (duration - frame) * 18LL * 256LL) /
            (duration * duration);
        expected.y_q8 -= (int32_t)arc;
        return scene->ball_position.x_q8 == expected.x_q8 &&
               scene->ball_position.y_q8 == expected.y_q8;
    }

    if (scene->predicted_make_route) {
        if (scene->shot_schedule ==
                TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) {
            apex_y = (scene->shot_start_position.y_q8 <
                          scene->shot_end_position.y_q8
                      ? scene->shot_start_position.y_q8
                      : scene->shot_end_position.y_q8) - 34 * 256;
            if (scene->shot_frame <=
                    TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME) {
                expected = scene->shot_start_position;
            } else if (scene->shot_frame <= 47U) {
                expected.x_q8 = scene_validation_lerp_q8(
                    scene->shot_start_position.x_q8,
                    scene->shot_end_position.x_q8,
                    (unsigned)(scene->shot_frame -
                        TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME), 76U);
                expected.y_q8 = scene_validation_lerp_q8(
                    scene->shot_start_position.y_q8, apex_y,
                    (unsigned)(scene->shot_frame -
                        TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME), 38U);
            } else if (scene->shot_frame <
                           TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) {
                expected.x_q8 = scene_validation_lerp_q8(
                    scene->shot_start_position.x_q8,
                    scene->shot_end_position.x_q8,
                    (unsigned)(scene->shot_frame -
                        TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME), 76U);
                expected.y_q8 = scene_validation_lerp_q8(
                    apex_y, scene->shot_end_position.y_q8,
                    (unsigned)(scene->shot_frame - 47U), 38U);
            } else {
                expected = scene->shot_end_position;
            }
        } else {
            uint16_t release = TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME;
            uint16_t score = TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME;
            uint16_t flight = (uint16_t)(score - release);
            uint16_t half = (uint16_t)(flight / 2U);
            apex_y = (scene->shot_start_position.y_q8 <
                          scene->shot_end_position.y_q8
                      ? scene->shot_start_position.y_q8
                      : scene->shot_end_position.y_q8) - 28 * 256;
            if (scene->shot_frame <= release) {
                expected = scene->shot_start_position;
            } else if (scene->shot_frame < score) {
                unsigned flight_step = (unsigned)(scene->shot_frame - release);
                expected.x_q8 = scene_validation_lerp_q8(
                    scene->shot_start_position.x_q8,
                    scene->shot_end_position.x_q8,
                    flight_step, flight);
                expected.y_q8 = scene->shot_frame <= release + half
                    ? scene_validation_lerp_q8(
                        scene->shot_start_position.y_q8, apex_y,
                        flight_step, half)
                    : scene_validation_lerp_q8(
                        apex_y, scene->shot_end_position.y_q8,
                        (unsigned)(scene->shot_frame - release - half),
                        (unsigned)(flight - half));
            } else {
                expected = scene->shot_end_position;
            }
        }
        return scene->ball_position.x_q8 == expected.x_q8 &&
               scene->ball_position.y_q8 == expected.y_q8;
    }

    if ((scene->jump_rim_rattle_debug ||
         scene->shot_rim_rattle_selected) &&
        scene->shot_frame >= TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME &&
        scene->shot_frame <= TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        TecmoGameplayShotRimRattle rattle;
        uint8_t steps = (uint8_t)(scene->shot_frame -
            TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME);
        if (!scene_validation_expected_rattle(
                scene, 0U, steps, &rattle, &expected) ||
            memcmp(&rattle, &scene->jump_rim_rattle,
                   sizeof(rattle)) != 0) {
            return false;
        }
        return scene->ball_position.x_q8 == expected.x_q8 &&
               scene->ball_position.y_q8 == expected.y_q8;
    }
    if (scene->shot_frame <= 4U) {
        expected = scene->shot_start_position;
    } else if (scene->shot_frame <= 32U) {
        expected.x_q8 = scene_validation_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(scene->shot_frame - 4U), 69U);
        apex_y = (scene->shot_start_position.y_q8 <
                      scene->shot_end_position.y_q8
                  ? scene->shot_start_position.y_q8
                  : scene->shot_end_position.y_q8) - 34 * 256;
        expected.y_q8 = scene_validation_lerp_q8(
            scene->shot_start_position.y_q8, apex_y,
            (unsigned)(scene->shot_frame - 4U), 28U);
    } else if (scene->shot_frame == 33U) {
        expected.x_q8 = scene_validation_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            29U, 69U);
        apex_y = (scene->shot_start_position.y_q8 <
                      scene->shot_end_position.y_q8
                  ? scene->shot_start_position.y_q8
                  : scene->shot_end_position.y_q8) - 34 * 256;
        expected.y_q8 = apex_y;
    } else if (scene->shot_frame <= 73U) {
        expected.x_q8 = scene_validation_lerp_q8(
            scene->shot_start_position.x_q8, scene->shot_end_position.x_q8,
            (unsigned)(scene->shot_frame - 4U), 69U);
        apex_y = (scene->shot_start_position.y_q8 <
                      scene->shot_end_position.y_q8
                  ? scene->shot_start_position.y_q8
                  : scene->shot_end_position.y_q8) - 34 * 256;
        expected.y_q8 = scene_validation_lerp_q8(
            apex_y, scene->shot_end_position.y_q8,
            (unsigned)(scene->shot_frame - 33U), 40U);
    } else {
        expected = scene->shot_end_position;
    }
    return scene->ball_position.x_q8 == expected.x_q8 &&
           scene->ball_position.y_q8 == expected.y_q8;
}

static bool scene_validation_flight_state_valid(
    const TecmoGameplayScene *scene,
    uint16_t first_step_frame,
    uint16_t land_frame,
    uint16_t initial_altitude,
    uint16_t initial_velocity)
{
    uint16_t altitude = initial_altitude;
    uint16_t velocity = initial_velocity;
    bool landed = false;
    if (scene == NULL) return false;
    if (scene->shot_frame >= first_step_frame) {
        for (uint16_t frame = first_step_frame;
             frame <= scene->shot_frame && frame <= land_frame; ++frame) {
            if (!tecmo_gameplay_jump_shots_step_q8(
                    &scene->jump_shots, &altitude, &velocity, &landed)) {
                return false;
            }
        }
    }
    if (scene->shot_frame >= land_frame && !landed) return false;
    return scene->jump_actor_altitude_q8 == altitude &&
           scene->jump_actor_velocity_q8 == velocity &&
           scene->jump_actor_landed == landed;
}

static bool scene_validation_jump_timeline_valid(
    const TecmoGameplayScene *scene)
{
    static const uint8_t make_release_phases[8] = {
        0x31U, 0x21U, 0x11U, 0x01U,
        0x32U, 0x22U, 0x12U, 0x02U
    };
    uint16_t frame;
    uint8_t expected_phase;
    uint8_t expected_ball_state;
    uint8_t expected_repeats;
    bool rattle_route;
    if (scene == NULL || scene->shot_kind !=
            TECMO_GAMEPLAY_SCENE_SHOT_JUMP) return false;
    frame = scene->shot_frame;
    if (scene->predicted_make_route) {
        if (scene->jump_ball_altitude_q8 != 0U ||
            scene->jump_ball_bounce_q8 != 0U) {
            return false;
        }
        if (scene->shot_schedule ==
                TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION) {
            if (!scene_validation_flight_state_valid(
                    scene, (uint16_t)(
                        TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME + 1U),
                    TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME, 0U,
                    TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8)) {
                return false;
            }
            return true;
        }
        if (frame <= 8U) {
            expected_phase = frame <= 4U
                ? (uint8_t)((uint16_t)scene->jump_shots.constants
                    .phase_seed_gather - 0x10U * (uint16_t)(frame - 1U))
                : (uint8_t)((uint16_t)scene->jump_shots.constants
                    .phase_seed_gather - 0x10U * (uint16_t)(frame - 5U));
            return scene->jump_actor_state ==
                        scene->jump_shots.constants.actor_state_gather &&
                    scene->jump_ball_state ==
                        scene->jump_shots.constants.ball_state_neutral &&
                    !scene->jump_b_released &&
                    scene->jump_outcome ==
                        TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
                   scene->jump_phase_counter == expected_phase &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame == TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME) {
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_gather &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_b_released &&
                   scene->jump_outcome ==
                       TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
                   scene->jump_phase_counter ==
                       scene->jump_shots.constants.phase_seed_gather &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame >= 10U && frame <= 17U) {
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_prepared &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_b_released &&
                   scene->jump_outcome ==
                       TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
                   scene->jump_phase_counter ==
                       make_release_phases[frame - 10U] &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame == 18U) {
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_held &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_b_released &&
                   scene->jump_outcome ==
                       TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
                   scene->jump_phase_counter == 0x34U &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame >= 19U && frame <
                TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME) {
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_airborne &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_phase_counter == 0x35U &&
                   scene->jump_outcome ==
                       TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame == TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME) {
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_recovery &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_phase_counter ==
                       scene->jump_shots.constants.phase_seed_recovery_counter &&
                   scene->jump_outcome ==
                       TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        if (frame >= 58U && frame <= 62U) {
            expected_phase = (uint8_t)((uint16_t)scene->jump_shots.constants
                .phase_seed_recovery_counter -
                0x10U * (uint16_t)(frame - 57U));
            return scene->jump_actor_state ==
                       scene->jump_shots.constants.actor_state_recovery &&
                   scene->jump_ball_state ==
                       scene->jump_shots.constants.ball_state_neutral &&
                   scene->jump_phase_counter == expected_phase &&
                   scene_validation_flight_state_valid(
                       scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                       0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
        }
        return scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_neutral &&
               scene->jump_ball_state ==
                   scene->jump_shots.constants.ball_state_neutral &&
               scene->jump_phase_counter ==
                   scene->jump_shots.constants.phase_seed_gather &&
               scene_validation_flight_state_valid(
                   scene, 20U, TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME,
                   0U, TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8);
    }

    rattle_route = scene->jump_rim_rattle_debug ||
                   scene->shot_rim_rattle_selected;
    if (rattle_route) {
        if (frame < TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME) {
            expected_repeats = 0U;
        } else {
            uint16_t rattle_steps = (uint16_t)(frame -
                TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME);
            if (rattle_steps > TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES) {
                rattle_steps = TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES;
            }
            expected_repeats = (uint8_t)(rattle_steps / 4U);
            if (expected_repeats > 3U) expected_repeats = 3U;
        }
    } else {
        expected_repeats = 0U;
    }
    if (scene->jump_rim_rattle_audio_repeats != expected_repeats) {
        return false;
    }
    if (rattle_route && frame >=
            TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME && frame <=
            TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        if (scene->jump_ball_altitude_q8 != 0x3800U ||
            scene->jump_ball_bounce_q8 != 0U) {
            return false;
        }
    } else if (rattle_route && frame >
                   TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME) {
        if (scene->jump_ball_altitude_q8 != 0U ||
            scene->jump_ball_bounce_q8 !=
                (frame == TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME + 1U
                    ? scene->jump_shots.constants.bounce_decay_q8 : 0U)) {
            return false;
        }
    } else if (scene->jump_ball_altitude_q8 != 0U ||
               scene->jump_ball_bounce_q8 !=
                   (frame == 74U
                        ? scene->jump_shots.constants.bounce_decay_q8 : 0U)) {
        return false;
    }
    if (frame == 1U) {
        return !scene->jump_b_released &&
               scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_held &&
               scene->jump_ball_state ==
                   scene->jump_shots.constants.ball_state_route1 &&
               scene->jump_phase_counter ==
                   scene->jump_shots.constants.phase_seed_prepared &&
               scene_validation_flight_state_valid(
                   scene, 4U, 40U,
                   TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
                   TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
    }
    if (frame >= 2U && frame < 40U) {
        expected_ball_state = frame >= 5U
            ? scene->jump_shots.constants.ball_state_route17
            : scene->jump_shots.constants.ball_state_route5;
        return scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_airborne &&
               scene->jump_ball_state == expected_ball_state &&
               scene->jump_phase_counter ==
                   scene->jump_shots.constants.phase_seed_airborne &&
               scene_validation_flight_state_valid(
                   scene, 4U, 40U,
                   TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
                   TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
    }
    if (frame == 40U) {
        return scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_recovery &&
               scene->jump_ball_state ==
                   scene->jump_shots.constants.ball_state_route17 &&
               scene->jump_phase_counter ==
                   scene->jump_shots.constants.phase_seed_recovery_counter &&
               scene_validation_flight_state_valid(
                   scene, 4U, 40U,
                   TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
                   TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
    }
    if (frame >= 41U && frame <= 45U) {
        expected_phase = (uint8_t)((uint16_t)scene->jump_shots.constants
            .phase_seed_recovery_counter -
            0x10U * (uint16_t)(frame - 40U));
        return scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_recovery &&
               scene->jump_ball_state ==
                   scene->jump_shots.constants.ball_state_route17 &&
               scene->jump_phase_counter == expected_phase &&
               scene_validation_flight_state_valid(
                   scene, 4U, 40U,
                   TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
                   TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
    }
    if (frame == 46U || frame < 73U) {
        return scene->jump_actor_state ==
                   scene->jump_shots.constants.actor_state_neutral &&
               scene->jump_ball_state ==
                   scene->jump_shots.constants.ball_state_route17 &&
               scene->jump_phase_counter == 0U &&
               scene_validation_flight_state_valid(
                   scene, 4U, 40U,
                   TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
                   TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
    }
    expected_ball_state = rattle_route && frame < 89U
        ? scene->shot_resolution.rim_rattle.object_state
        : scene->jump_shots.constants.ball_state_route10;
    return scene->jump_actor_state ==
               scene->jump_shots.constants.actor_state_neutral &&
           scene->jump_ball_state == expected_ball_state &&
           scene->jump_phase_counter == 0U &&
           scene_validation_flight_state_valid(
               scene, 4U, 40U,
               TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8,
               TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8);
}

static bool scene_raw_launch_zero(const TecmoGameplayScene *scene)
{
    TecmoGameplayCpuA0f3Result zero_result;
    TecmoGameplayCpuA0f3Motion zero_motion;
    TecmoGameplayCpuA8e9VelocityResult zero_normalized;
    memset(&zero_result, 0, sizeof(zero_result));
    memset(&zero_motion, 0, sizeof(zero_motion));
    memset(&zero_normalized, 0, sizeof(zero_normalized));
    return scene != NULL && !scene->shot_a0f3_origin_valid &&
        scene->shot_a0f3_origin_x == 0U &&
        scene->shot_a0f3_origin_depth == 0U &&
        !scene->shot_a0f3_preflight_valid &&
        scene->shot_a0f3_preflight_raw_006a == 0U &&
        scene->shot_a0f3_launch_raw_006a == 0U &&
        scene->shot_a0f3_release_raw_0053 == 0U &&
        scene->shot_a0f3_rng_start_raw_006a == 0U &&
        scene->shot_a0f3_release_c05d_serial == 0U &&
        memcmp(&scene->shot_a0f3_result, &zero_result,
               sizeof(zero_result)) == 0 &&
        memcmp(&scene->shot_a0f3_motion, &zero_motion,
               sizeof(zero_motion)) == 0 &&
        !scene->shot_a0f3_motion_valid &&
        !scene->shot_a0f3_raw_position_valid &&
        scene->shot_a0f3_raw_x == 0U &&
        scene->shot_a0f3_raw_depth == 0U &&
        scene->shot_a0f3_tick_count == 0U &&
        !scene->shot_a8e9_normalized_valid &&
        scene->shot_a8e9_raw_006a == 0U &&
        memcmp(&scene->shot_a8e9_normalized, &zero_normalized,
               sizeof(zero_normalized)) == 0;
}

static uint8_t scene_validation_rng_mix(uint8_t raw_006a, uint8_t raw_0053)
{
    uint8_t shifted = (uint8_t)(raw_006a << 1U);
    if ((raw_006a & 0x80U) != 0U) shifted ^= 0x1DU;
    if (shifted == 0U) shifted ^= raw_0053;
    return shifted;
}

static bool scene_raw_launch_active_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplayCpuA0f3Input input;
    TecmoGameplayCpuA0f3Result result;
    TecmoGameplayCpuA0f3Motion motion;
    TecmoGameplayCpuA0f3PublishedPosition published;
    TecmoGameplayCpuA8e9VelocityInput normalize_input;
    TecmoGameplayCpuA8e9VelocityResult normalized;
    uint16_t expected_ticks;
    uint16_t tick;
    uint16_t expected_x;
    uint8_t expected_depth;
    uint8_t expected_preflight;
    uint8_t expected_launch;
    uint8_t orientation;
    bool terminal_normalized;
    if (scene == NULL || scene->legacy_direct_launch ||
        scene->jump_rim_rattle_debug ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->predicted_make_route ||
        scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        !scene->shot_a0f3_origin_valid) return false;
    if (!scene->jump_b_released) {
        TecmoGameplayCpuA0f3Result zero_result;
        TecmoGameplayCpuA0f3Motion zero_motion;
        TecmoGameplayCpuA8e9VelocityResult zero_normalized;
        memset(&zero_result, 0, sizeof(zero_result));
        memset(&zero_motion, 0, sizeof(zero_motion));
        memset(&zero_normalized, 0, sizeof(zero_normalized));
        return scene->shot_frame == 1U &&
            !scene->shot_a0f3_preflight_valid &&
            scene->shot_a0f3_preflight_raw_006a == 0U &&
            scene->shot_a0f3_launch_raw_006a == 0U &&
            scene->shot_a0f3_release_raw_0053 == 0U &&
            scene->shot_a0f3_rng_start_raw_006a == 0U &&
            scene->shot_a0f3_release_c05d_serial == 0U &&
            memcmp(&scene->shot_a0f3_result, &zero_result,
                   sizeof(zero_result)) == 0 &&
            memcmp(&scene->shot_a0f3_motion, &zero_motion,
                   sizeof(zero_motion)) == 0 &&
            !scene->shot_a0f3_motion_valid &&
            !scene->shot_a0f3_raw_position_valid &&
            scene->shot_a0f3_raw_x == 0U &&
            scene->shot_a0f3_raw_depth == 0U &&
            scene->shot_a0f3_tick_count == 0U &&
            !scene->shot_a8e9_normalized_valid &&
            scene->shot_a8e9_raw_006a == 0U &&
            memcmp(&scene->shot_a8e9_normalized, &zero_normalized,
                   sizeof(zero_normalized)) == 0;
    }
    expected_preflight = scene_validation_rng_mix(
        (uint8_t)(scene->shot_a0f3_rng_start_raw_006a ^
                  scene->shot_a0f3_release_raw_0053),
        scene->shot_a0f3_release_raw_0053);
    expected_launch = scene_validation_rng_mix(
        (uint8_t)(expected_preflight ^
                  scene->shot_a0f3_release_raw_0053),
        scene->shot_a0f3_release_raw_0053);
    if (!scene->shot_a0f3_preflight_valid ||
        scene->shot_a0f3_preflight_raw_006a != expected_preflight ||
        scene->shot_a0f3_launch_raw_006a != expected_launch ||
        scene->shot_a0f3_release_c05d_serial < 2U ||
        scene->fixed_rng.c05d_serial !=
            scene->shot_a0f3_release_c05d_serial ||
        scene->fixed_rng.last_callsite !=
            TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD ||
        !scene->shot_a0f3_motion_valid ||
        !scene->shot_a0f3_raw_position_valid) return false;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG;
    input.raw_x_7d_f2 = scene->shot_a0f3_origin_x;
    input.raw_depth_fd = scene->shot_a0f3_origin_depth;
    input.raw_direction = (uint8_t)scene->jump_direction;
    input.raw_006a = scene->shot_a0f3_launch_raw_006a;
    if (!tecmo_gameplay_cpu_a0f3_solve(
            &scene->cpu_a0f3_assets, &input, &result) ||
        memcmp(&result, &scene->shot_a0f3_result, sizeof(result)) != 0 ||
        !tecmo_gameplay_cpu_a0f3_motion_begin(&result, &motion)) return false;
    expected_ticks = scene->shot_frame > 2U
        ? (uint16_t)(scene->shot_frame - 2U) : 0U;
    if (expected_ticks > result.duration_051e_0513)
        expected_ticks = result.duration_051e_0513;
    expected_x = scene->shot_a0f3_origin_x;
    expected_depth = scene->shot_a0f3_origin_depth;
    for (tick = 0U; tick < expected_ticks; ++tick) {
        if (!tecmo_gameplay_cpu_a0f3_motion_tick_publish(
                &motion, &published)) return false;
        expected_x = published.raw_x;
        expected_depth = published.raw_depth;
    }
    if (scene->shot_a0f3_tick_count != expected_ticks ||
        scene->shot_a0f3_raw_x != expected_x ||
        scene->shot_a0f3_raw_depth != expected_depth ||
        memcmp(&motion, &scene->shot_a0f3_motion, sizeof(motion)) != 0)
        return false;
    terminal_normalized = scene->shot_rim_rattle_selected &&
        scene->shot_frame >= TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME;
    if (!terminal_normalized) {
        TecmoGameplayCpuA8e9VelocityResult zero_normalized;
        memset(&zero_normalized, 0, sizeof(zero_normalized));
        return !scene->shot_a8e9_normalized_valid &&
            scene->shot_a8e9_raw_006a == 0U &&
            memcmp(&scene->shot_a8e9_normalized, &zero_normalized,
                   sizeof(zero_normalized)) == 0;
    }
    if (!scene->shot_a8e9_normalized_valid ||
        !scene_shot_captured_rattle_orientation(scene, &orientation) ||
        (scene->shot_frame == TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME &&
         scene->shot_a8e9_raw_006a != scene->fixed_rng.raw_006a) ||
        (uint16_t)scene->jump_rim_rattle.saved_horizontal_velocity_q6 !=
            motion.velocity_x_q6 ||
        (uint16_t)scene->jump_rim_rattle.saved_vertical_velocity_q6 !=
            motion.velocity_depth_q6) return false;
    memset(&normalize_input, 0, sizeof(normalize_input));
    normalize_input.contract_tag =
        TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG;
    normalize_input.raw_vx_04f1_04fc = motion.velocity_x_q6;
    normalize_input.raw_vz_0507_0512 = motion.velocity_depth_q6;
    normalize_input.raw_006a = scene->shot_a8e9_raw_006a;
    normalize_input.orientation_035a = orientation;
    return tecmo_gameplay_cpu_a8e9_velocity_normalize(
               &normalize_input, &normalized) &&
           memcmp(&normalized, &scene->shot_a8e9_normalized,
                  sizeof(normalized)) == 0;
}

/* This is intentionally a scene-boundary validator rather than a second
   gameplay resolver.  It proves ownership, schedule coherence, raw route
   identity, and the source-backed rattle prefix.  The `legacy_direct_launch`
   and `jump_rim_rattle_debug` bits are narrow compatibility allowances for
   pre-R2/direct diagnostic adapters; they do not relax actor, frame, result,
   or transactional invariants. */
bool scene_shot_state_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplayCloseShotVariant expected_variant =
        TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0;
    TecmoGameplayCloseShotVariantInfo close_info = {0};
    uint32_t tail_end;
    bool close;
    if (scene == NULL) return false;
    if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        return scene->shot_actor == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               scene->shot_frame == 0U && scene->shot_duration == 0U &&
               !scene->shot_result_awarded &&
               scene->shot_flags == 0U &&
               scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
               scene->shot_schedule ==
                   TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION &&
               scene->native_policy_sample == 0U &&
               scene->shot_make_probability == 0U &&
               !scene->shot_contact_context &&
               !scene->shot_contest_context &&
               scene->shot_context_signature == 0U &&
               scene->shot_rim_rattle_raw_selector == 0U &&
               scene->shot_rim_route.selector == 0U &&
               scene->shot_rim_route.kind == 0 &&
               scene->shot_rim_route.source_target_cpu == 0U &&
               !scene->shot_rim_rattle_selected &&
               !scene->shot_rim_tail_active &&
               scene->shot_rim_tail_frame == 0U &&
               scene->shot_rim_tail_duration == 0U &&
               scene->shot_rim_tail_base_frame == 0U &&
               scene->shot_actor_launch_position.x == 0 &&
               scene->shot_actor_launch_position.y == 0 &&
               scene->shot_actor_team == 0U &&
               scene->shot_actor_roster_index == 0U &&
               !scene->shot_launch_facing_right &&
               scene->shot_target_delta_x == 0 &&
               scene->shot_target_delta_y == 0 &&
               scene->shot_launch_frame == 0U &&
               !scene->shot_close_context &&
               !scene->jump_playback_active && !scene->predicted_make_route &&
               !scene->jump_b_released &&
               scene->jump_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN &&
               !scene->jump_actor_landed &&
               !scene->jump_rim_rattle_debug &&
               scene->jump_rim_rattle_raw_selector == 0U &&
               scene->jump_rim_rattle_audio_repeats == 0U &&
               scene_shot_rattle_zero(&scene->jump_rim_rattle) &&
               scene_shot_made_settlement_zero(
                   &scene->jump_made_settlement) &&
               scene->jump_actor_altitude_q8 == 0U &&
               scene->jump_actor_velocity_q8 == 0U &&
               scene->jump_ball_altitude_q8 == 0U &&
               scene->jump_ball_bounce_q8 == 0U &&
               scene->jump_entry_pose_index == 0U &&
               scene->jump_resolved_pose_index == 0U &&
               scene->jump_actor_state == 0U &&
               scene->jump_ball_state == 0U &&
               scene->jump_phase_counter == 0U &&
               scene->jump_pose_frame == 0U &&
               scene->shot_controller == TECMO_GAMEPLAY_SCENE_NO_ACTOR &&
               scene->jump_family == TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 &&
               scene->jump_profile == TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 &&
               scene->jump_direction == TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0 &&
               scene->close_shot_variant ==
                   TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0 &&
               scene_raw_launch_zero(scene);
    }

    close = scene_shot_is_close(scene->shot_kind);
    if ((!close && scene->shot_kind !=
             TECMO_GAMEPLAY_SCENE_SHOT_JUMP) ||
        scene->shot_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[scene->shot_actor].active ||
        (scene->actors[scene->shot_actor].team != TECMO_GAMEPLAY_TEAM_AWAY &&
         scene->actors[scene->shot_actor].team != TECMO_GAMEPLAY_TEAM_HOME) ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->shot_duration == 0U || scene->shot_frame >= scene->shot_duration ||
        scene->shot_points < 1U || scene->shot_points > 3U ||
        scene->shot_close_context != close ||
        (scene->shot_flags &
            (uint8_t)~scene->shot_resolution.point_shot_flags_mask) != 0U ||
        scene->shot_make_probability < 5U ||
        scene->shot_make_probability > 95U ||
        (uint8_t)scene->native_policy_sample !=
            scene->shot_rim_rattle_raw_selector ||
        (scene->shot_contact_context && !scene->shot_contest_context) ||
        scene->shot_context_signature != scene_shot_context_signature(
            scene->native_policy_sample, scene->shot_contact_context,
            scene->shot_contest_context) ||
        (scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE &&
         scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) ||
        (scene->shot_result_awarded &&
         scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) ||
        !scene_shot_route_metadata_valid(scene) ||
        !scene_shot_rattle_state_valid(scene)) {
        return false;
    }
    if (scene->shot_rim_rattle_selected &&
        scene->shot_rim_route.kind != TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
        return false;
    }
    if (scene->shot_rim_route.kind == TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 &&
        scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MISS &&
        !scene->legacy_direct_launch && !scene->jump_rim_rattle_debug &&
        !scene->shot_rim_rattle_selected) {
        return false;
    }
    if (!scene_shot_bound_evaluation_valid(scene) ||
        !scene_shot_bound_selectors_valid(scene)) {
        return false;
    }
    if (close) {
        bool tail_rattle = scene->shot_rim_tail_active &&
            (scene->jump_rim_rattle_debug ||
             scene->shot_rim_rattle_selected);
        uint8_t expected_tail_repeats = tail_rattle
            ? (uint8_t)(scene->shot_rim_tail_frame / 4U)
            : 0U;
        if (expected_tail_repeats > 3U) expected_tail_repeats = 3U;
        expected_variant = scene->shot_kind ==
                TECMO_GAMEPLAY_SCENE_SHOT_DUNK
            ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
            : scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_LAYUP
                ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2
                : TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1;
        if (!scene_raw_launch_zero(scene) ||
            (unsigned)scene->close_shot_profile >=
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT ||
            (unsigned)scene->close_shot_direction >=
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT ||
            scene->close_shot_variant != expected_variant ||
            !tecmo_gameplay_close_shots_get_variant_info(
                &scene->close_shots, expected_variant, &close_info) ||
            (expected_variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1
                ? scene->shot_schedule !=
                    TECMO_GAMEPLAY_SHOT_SCHEDULE_CLOSE_NUMERIC_1
                : scene->shot_schedule !=
                    TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION)) {
            return false;
        }
        if (scene->shot_result_awarded ||
            scene->jump_playback_active || scene->predicted_make_route ||
            scene->jump_b_released || scene->jump_outcome !=
                TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
            scene->jump_actor_landed || scene->jump_rim_rattle_debug ||
            scene->jump_rim_rattle_raw_selector != 0U ||
            scene->jump_rim_rattle_audio_repeats !=
                expected_tail_repeats ||
            scene->jump_actor_altitude_q8 != 0U ||
            scene->jump_actor_velocity_q8 != 0U ||
            scene->jump_ball_bounce_q8 != 0U ||
            scene->jump_entry_pose_index != 0U ||
            scene->jump_resolved_pose_index != 0U ||
            scene->jump_actor_state != 0U ||
            scene->jump_phase_counter != 0U || scene->jump_pose_frame != 0U ||
            scene->shot_controller != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->jump_family != TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 ||
            scene->jump_profile != TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 ||
            scene->jump_direction != TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0 ||
            !scene_shot_made_settlement_zero(
                &scene->jump_made_settlement) ||
            (tail_rattle && scene->jump_ball_state !=
                 scene->shot_resolution.rim_rattle.object_state) ||
            (!tail_rattle &&
             (scene->jump_ball_altitude_q8 != 0U ||
             scene->jump_ball_state != 0U ||
            !scene_shot_rattle_zero(&scene->jump_rim_rattle)))) {
            return false;
        }
        if (scene->shot_rim_tail_active) {
            if (scene->shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
                scene->shot_rim_tail_base_frame !=
                    (expected_variant ==
                        TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
                        ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                        : expected_variant ==
                            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1
                            ? TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION
                            : close_info.step_count)) {
                return false;
            }
        } else if (scene->shot_duration !=
            (expected_variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
                ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                : expected_variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1
                    ? TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION
                    : close_info.step_count)) {
            return false;
        }
    } else {
        uint16_t expected_jump_pose;
        if ((scene->predicted_make_route || scene->legacy_direct_launch ||
             scene->jump_rim_rattle_debug)
                ? !scene_raw_launch_zero(scene)
                : !scene_raw_launch_active_valid(scene)) {
            return false;
        }
        if (!scene->legacy_direct_launch &&
            !scene->jump_rim_rattle_debug &&
            scene->jump_rim_rattle_raw_selector != 0U) {
            return false;
        }
        if (!tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
                &scene->jump_shots, scene->jump_family,
                scene->jump_profile, scene->jump_direction,
                &expected_jump_pose) ||
            scene->jump_resolved_pose_index != expected_jump_pose) {
            return false;
        }
        if (scene->predicted_make_route && scene->jump_entry_pose_index !=
                TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE) {
            return false;
        }
        if (!scene->predicted_make_route &&
            (scene->shot_result_awarded ||
             !scene_shot_made_settlement_zero(
                 &scene->jump_made_settlement))) {
            return false;
        }
        if ((unsigned)scene->jump_family >=
                TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT ||
            (unsigned)scene->jump_profile >=
                TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT ||
            (unsigned)scene->jump_direction >=
                TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT ||
            !scene->jump_playback_active ||
            scene->shot_schedule ==
                TECMO_GAMEPLAY_SHOT_SCHEDULE_CLOSE_NUMERIC_1 ||
            (scene->shot_schedule !=
                 TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION &&
             scene->shot_schedule !=
                 TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) ||
            !scene_shot_controller_binding_valid(scene) ||
            scene->predicted_make_route !=
                (scene->shot_outcome == TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE) ||
            (!scene->predicted_make_route && scene->shot_outcome !=
                TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) ||
            (!scene->jump_b_released && scene->jump_outcome !=
                TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) ||
            (scene->jump_b_released &&
             scene->predicted_make_route &&
             scene->shot_schedule ==
                 TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT &&
             scene->shot_frame < TECMO_GAMEPLAY_JUMP_MAKE_DECISION_FRAME &&
             scene->jump_outcome !=
                 TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN) ||
            (scene->jump_b_released &&
             !(scene->predicted_make_route &&
               scene->shot_schedule ==
                   TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT &&
               scene->shot_frame <
                   TECMO_GAMEPLAY_JUMP_MAKE_DECISION_FRAME) &&
             scene->jump_outcome != scene->shot_outcome)) {
            return false;
        }
        if (scene->shot_schedule ==
                 TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT &&
             (scene->shot_points != 3U || scene->jump_family !=
                 TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 || scene->jump_profile !=
                 TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 || scene->jump_direction !=
                 TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1)) {
             return false;
         }
        if (scene->shot_points == 3U && scene->jump_family ==
                TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 &&
            scene->jump_profile == TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 &&
            scene->jump_direction == TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1 &&
            scene->shot_schedule !=
                TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) {
            return false;
        }
        if (scene->predicted_make_route) {
            uint16_t expected_duration =
                scene->shot_schedule ==
                    TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT
                    ? (uint16_t)(TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME +
                                 scene->jump_shots.constants.made_update_count)
                    : TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION;
            if (scene->shot_duration != expected_duration) return false;
            if (scene->shot_schedule ==
                    TECMO_GAMEPLAY_SHOT_SCHEDULE_EXACT_THREE_POINT) {
                if (scene->shot_frame <
                        TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) {
                    if (scene->shot_result_awarded ||
                        !scene_shot_made_settlement_zero(
                            &scene->jump_made_settlement)) {
                        return false;
                    }
                } else {
                    TecmoGameplayJumpShotMadeSettlement settlement =
                        scene->jump_made_settlement;
                    if (!scene->shot_result_awarded ||
                        settlement.complete ||
                        settlement.updates !=
                            (uint8_t)(scene->shot_frame -
                                      TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME) ||
                        !tecmo_gameplay_jump_shots_made_settlement_step(
                            &scene->jump_shots, &settlement, true)) {
                        return false;
                    }
                }
            } else if (scene->shot_frame <
                           TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME) {
                if (scene->shot_result_awarded ||
                    !scene_shot_made_settlement_zero(
                        &scene->jump_made_settlement)) {
                    return false;
                }
            } else if (!scene->shot_result_awarded ||
                       scene->jump_made_settlement.state != 0U ||
                       scene->jump_made_settlement.timer != 0U ||
                       scene->jump_made_settlement.stage != 0U ||
                       scene->jump_made_settlement.updates != 0U ||
                       !scene->jump_made_settlement.complete) {
                return false;
            }
            if (scene->shot_schedule ==
                    TECMO_GAMEPLAY_SHOT_SCHEDULE_NATIVE_APPROXIMATION) {
                if (!scene->jump_b_released) {
                    if (scene->shot_frame >
                            TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME ||
                        scene->jump_actor_state !=
                            scene->jump_shots.constants.actor_state_gather ||
                        scene->jump_ball_state !=
                            scene->jump_shots.constants.ball_state_neutral ||
                        scene->jump_phase_counter !=
                            scene->jump_shots.constants.phase_seed_gather ||
                        scene->jump_actor_altitude_q8 != 0U ||
                        scene->jump_actor_velocity_q8 !=
                            TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8 ||
                        scene->jump_actor_landed) {
                        return false;
                    }
                } else if (scene->shot_frame <
                           TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
                    if (scene->jump_actor_state !=
                            scene->jump_shots.constants.actor_state_airborne ||
                        scene->jump_ball_state !=
                            scene->jump_shots.constants.ball_state_route5 ||
                        scene->jump_phase_counter !=
                            scene->jump_shots.constants.phase_seed_airborne ||
                        scene->jump_actor_landed) {
                        return false;
                    }
                } else if (scene->shot_frame ==
                           TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME) {
                    if (scene->jump_actor_state !=
                            scene->jump_shots.constants.actor_state_recovery ||
                        scene->jump_ball_state !=
                            scene->jump_shots.constants.ball_state_route5 ||
                        scene->jump_phase_counter !=
                            scene->jump_shots.constants.phase_seed_recovery_counter ||
                        !scene->jump_actor_landed ||
                        scene->jump_actor_altitude_q8 != 0U ||
                        scene->jump_actor_velocity_q8 != 0U) {
                        return false;
                    }
                } else if (scene->shot_frame <
                           TECMO_GAMEPLAY_JUMP_APPROX_MAKE_NEUTRAL_FRAME) {
                    if (scene->jump_actor_state !=
                            scene->jump_shots.constants.actor_state_recovery ||
                        scene->jump_ball_state !=
                            scene->jump_shots.constants.ball_state_route5 ||
                        scene->jump_phase_counter != (uint8_t)(
                            (uint16_t)scene->jump_shots.constants
                                .phase_seed_recovery_counter -
                            0x10U * (uint16_t)(scene->shot_frame -
                                TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME)) ||
                        !scene->jump_actor_landed ||
                        scene->jump_actor_altitude_q8 != 0U ||
                        scene->jump_actor_velocity_q8 != 0U) {
                        return false;
                    }
                } else if (scene->jump_actor_state !=
                               scene->jump_shots.constants.actor_state_neutral ||
                           scene->jump_ball_state !=
                               scene->jump_shots.constants.ball_state_route5 ||
                           scene->jump_phase_counter !=
                               scene->jump_shots.constants.phase_seed_gather ||
                           !scene->jump_actor_landed ||
                           scene->jump_actor_altitude_q8 != 0U ||
                           scene->jump_actor_velocity_q8 != 0U) {
                    return false;
                }
            }
        } else if (!scene->shot_rim_tail_active &&
                   !scene->jump_rim_rattle_debug &&
                   scene->shot_duration !=
                       (scene->shot_rim_rattle_selected
                            ? TECMO_GAMEPLAY_JUMP_RATTLE_DURATION
                            : TECMO_GAMEPLAY_JUMP_SLOT0_DURATION)) {
            return false;
        }
        if (!scene_validation_jump_timeline_valid(scene)) {
            return false;
        }
    }
    if (!scene_validation_expected_ball_position(scene)) {
        return false;
    }
    if (!scene_shot_pose_state_valid(scene)) {
        return false;
    }
    if (scene->shot_rim_tail_active) {
        if (scene->shot_rim_tail_duration == 0U ||
            scene->shot_rim_tail_frame >= scene->shot_rim_tail_duration) {
            return false;
        }
        tail_end = (uint32_t)scene->shot_rim_tail_base_frame +
                   (uint32_t)scene->shot_rim_tail_duration;
        if (tail_end > UINT16_MAX ||
            scene->shot_duration != (uint16_t)tail_end ||
            scene->shot_frame !=
                (uint16_t)((uint32_t)scene->shot_rim_tail_base_frame +
                           scene->shot_rim_tail_frame)) {
            return false;
        }
        if (scene->shot_rim_route.kind ==
                TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
            if (!scene_shot_is_close(scene->shot_kind) &&
                !scene->legacy_direct_launch &&
                !scene->jump_rim_rattle_debug) {
                return false;
            }
        } else if (scene->shot_rim_route.kind !=
                       TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 &&
                   scene->shot_rim_route.kind !=
                       TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9) {
            return false;
        }
        {
            uint8_t expected_tail_duration;
            uint8_t tail_selector = scene->shot_rim_route.selector;
            uint16_t expected_tail_base;
            if (scene_shot_is_close(scene->shot_kind)) {
                expected_tail_base = expected_variant ==
                        TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0
                    ? TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME
                    : expected_variant ==
                        TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_1
                        ? TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION
                        : close_info.step_count;
            } else {
                expected_tail_base = TECMO_GAMEPLAY_JUMP_SLOT0_DURATION;
            }
            if (scene->shot_rim_route.kind ==
                    TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708) {
                if (tail_selector == 0U) {
                    expected_tail_duration = 6U;
                } else if (tail_selector == 3U) {
                    expected_tail_duration = 8U;
                } else {
                    return false;
                }
            } else if (scene->shot_rim_route.kind ==
                           TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9) {
                if (tail_selector != 1U) return false;
                expected_tail_duration = (uint8_t)(
                    TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES +
                    TECMO_GAMEPLAY_SHOT_RIM_TAIL_GROUND_UPDATE);
            } else {
                if (tail_selector != 2U) return false;
                expected_tail_duration = 7U;
            }
            if (scene->shot_rim_tail_base_frame != expected_tail_base ||
                scene->shot_rim_tail_duration != expected_tail_duration) {
                return false;
            }
        }
    }
    return true;
}

bool scene_ownership_valid(const TecmoGameplayScene *scene)
{
    TecmoGameplaySceneCourtCoordinates coordinates;
    TecmoGameplayMovementState movement;
    bool seen_starter[TECMO_GAMEPLAY_TEAM_COUNT]
                       [TECMO_TEAM_DATA_PLAYERS_PER_TEAM];
    bool live_sync_required;
    size_t actor;
    size_t controller;
    if (scene == NULL || !scene->launch.starter_binding_bound ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        return false;
    }
    if (scene == NULL ||
        !scene->camera_assets.available ||
        !scene->movement_assets.available ||
        !scene->ball_dribble_assets.available ||
        !scene->cpu_steering_assets.available ||
        !scene->cpu_a0f3_assets.available ||
        (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
         !scene->legacy_direct_launch &&
         !tecmo_gameplay_fixed_rng_valid(&scene->fixed_rng)) ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation) ||
        !scene->penalty_assets.available ||
        !tecmo_gameplay_backcourt_state_valid(
            &scene->backcourt_assets, &scene->backcourt_state) ||
        !tecmo_gameplay_fatigue_state_valid(
            &scene->fatigue_assets, &scene->fatigue_state) ||
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
             ? !scene_court_free_throw_lineup_matches(scene)
             : scene->free_throw_lineup_active) ||
        !scene_pass_state_valid(scene) ||
        !scene_inbound_state_valid(scene) ||
        !scene_loose_ball_state_valid(scene) ||
        !tecmo_gameplay_scene_court_coordinates(
            scene, &coordinates)) {
        return false;
    }
    live_sync_required =
        !scene->legacy_direct_launch &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene->loose_ball_state.active &&
        !scene->live_foundation.first_sync_pending;
    memset(seen_starter, 0, sizeof(seen_starter));
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        size_t local = actor % TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        uint8_t side = actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
            ? TECMO_GAMEPLAY_TEAM_AWAY : TECMO_GAMEPLAY_TEAM_HOME;
        uint8_t team_id = side == TECMO_GAMEPLAY_TEAM_AWAY
            ? scene->launch.away_team : scene->launch.home_team;
        uint8_t expected_roster =
            scene->launch.starter_roster_index[side][local];
        const TecmoTeamDataPlayer *selected_player;
        if (expected_roster >= TECMO_TEAM_DATA_PLAYERS_PER_TEAM ||
            seen_starter[side][expected_roster] ||
            scene->actors[actor].team != side ||
            scene->actors[actor].roster_index != expected_roster ||
            team_id >= TECMO_TEAM_DATA_TEAM_COUNT ||
            (selected_player = scene_actor_player(
                scene, &scene->actors[actor])) == NULL ||
            selected_player !=
                &scene->pretip_team_data->players[team_id][expected_roster]) {
            return false;
        }
        seen_starter[side][expected_roster] = true;
        if (!scene_cpu_actor_state_valid(
                scene, actor, &scene->cpu_actors[actor]) ||
            scene->cpu_actors[actor].linked_actor !=
                scene->live_foundation.play_state.fixed_link[actor] ||
            (live_sync_required &&
             scene->live_foundation.actor_team[actor] !=
                 scene->actors[actor].team) ||
            (live_sync_required &&
             (scene->live_foundation.actor_position[actor].x !=
                  scene->actors[actor].position.x ||
              scene->live_foundation.actor_position[actor].y !=
                  scene->actors[actor].position.y)) ||
            (scene->actors[actor].active &&
             (!scene_actor_position_valid_for_scene(scene, actor) ||
              (!scene_actor_coordinate_valid(
                   &scene->actors[actor].anchor) &&
               !scene_actor_position_valid_for_scene(scene, actor)) ||
              !scene_actor_movement_state(
                  scene, &scene->actors[actor], &movement) ||
              scene->actors[actor].team >=
                   TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT ||
              scene->actors[actor].roster_index >=
                   TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT ||
              scene->actors[actor].condition !=
                   scene->fatigue_state.condition
                       [scene->actors[actor].team]
                       [scene->actors[actor].roster_index]))) {
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
        if (live_sync_required &&
            (scene->live_foundation.controller_team[controller] != team ||
             scene->live_foundation.last_controlled_actor[controller] !=
                 controlled)) {
            return false;
        }
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene->loose_ball_state.active &&
        (scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
         scene->actors[scene->ball_holder].team != scene->state.possession)) {
        return false;
    }
    if (scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
        scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
        !scene->loose_ball_state.active) {
        for (controller = 0U;
             controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
            if (scene->launch.controller_team[controller] ==
                    scene->state.possession &&
                scene->controlled_actor[controller] != scene->ball_holder) {
                return false;
            }
        }
    }
    if (live_sync_required &&
        (scene->live_foundation.orientation !=
             scene->orientation_state.attack_direction ||
         scene->live_foundation.last_possession !=
             (uint8_t)scene->state.possession ||
         scene->live_foundation.last_ball_holder != scene->ball_holder ||
         (!scene->live_foundation.score_restart_selection_active &&
          scene->live_foundation.primary_actor != scene->ball_holder) ||
         (scene->live_foundation.score_restart_selection_active &&
          scene->live_foundation.score_restart_passer !=
              scene->ball_holder) ||
         scene->live_foundation.defender_actor >=
             TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
         scene->actors[scene->live_foundation.defender_actor].team ==
             scene->state.possession)) {
        return false;
    }
    return scene_shot_state_valid(scene);
}
