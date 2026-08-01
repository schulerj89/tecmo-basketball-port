#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_gameplay_scene_test_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool tecmo_gameplay_scene_test_close_clock_collision(
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

bool tecmo_gameplay_scene_test_jump_period_expiry(
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

bool tecmo_gameplay_scene_test_jump_make_period_expiry(
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

bool tecmo_gameplay_scene_test_combined_restart_is_inert(
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

bool tecmo_gameplay_scene_test_shot_clock(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneLaunch launch = test->launch;
    TecmoControlFrame p1 = test->p1;
    TecmoControlFrame p2 = test->p2;
    char *message = test->message;
    size_t message_size = test->message_size;
    TecmoGameplayScene rattle_before;
    TecmoGameplayCameraState camera_before;
    TecmoGameplayScene draw_probe;
    uint32_t close_transition_serial;
    uint16_t expected_pose;
    uint16_t jump_entry_pose;
    uint16_t away_score_before;
    uint8_t holder;
    uint8_t shot_actor;
    int16_t x;
    int16_t y;
    size_t frame;

#define TEST_SCENE (*test->scene)
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    x = TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x;
    p1.held.confirm = true;
    p1.pressed.confirm = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "START changed live gameplay state");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT ||
        !tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x + 1) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "TGMO directional latency/movement contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    x = TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x;
    y = TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.y;
    memset(&p1, 0, sizeof(p1));
    p1.held.left = true;
    p1.held.right = true;
    p1.held.up = true;
    p1.held.down = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.x != x ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].position.y != y ||
        TEST_SCENE.actors[TEST_SCENE.controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "contradictory-axis neutral integration policy failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    holder = TEST_SCENE.ball_holder;
    {
        uint8_t pass_target = scene_next_teammate(&TEST_SCENE, holder);
        if (pass_target >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "NES A pass target setup failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        TEST_SCENE.actors[pass_target].position.x = 300;
        TEST_SCENE.actors[pass_target].anchor =
            TEST_SCENE.actors[pass_target].position;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.ball_holder == holder ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NES A pass contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (TEST_SCENE.controlled_actor[0] != TEST_SCENE.ball_holder) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pass ownership invariant failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x = 174;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].facing_right = true;
    /* The following close-shot checkpoints are deterministic makes. The later
       ordinary-jump checkpoint is the supported terminal miss. */
    TEST_SCENE.action_serial = 1U;
    scene_attach_ball(&TEST_SCENE);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
        TEST_SCENE.shot_duration != TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME ||
        TEST_SCENE.close_shot_step != 0U ||
        TEST_SCENE.close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        TEST_SCENE.close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
        scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
        !scene_close_pose_for_step(&TEST_SCENE, 0U, &expected_pose) ||
        TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index != expected_pose ||
        !tecmo_gameplay_scene_test_draw_exact_step(&TEST_SCENE)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NES B dunk/TGCS variant-0 contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_1;
    if (scene_close_pose_for_step(&TEST_SCENE, 0U, &expected_pose)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "unsupported live TGCS direction was accepted");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    close_transition_serial =
        TEST_SCENE.state.close_shot_subtype01.transition_serial;
    shot_actor = TEST_SCENE.shot_actor;
    for (frame = 0U; frame < 140U &&
         TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p1, 0, sizeof(p1));
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (tecmo_gameplay_scene_in_dunk_presentation(&TEST_SCENE) !=
                   (TEST_SCENE.shot_frame >=
                        TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
                    TEST_SCENE.shot_frame <
                        TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME) ||
               !scene_close_pose_for_step(&TEST_SCENE, TEST_SCENE.close_shot_step,
                                          &expected_pose) ||
               TEST_SCENE.actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
               scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
               TEST_SCENE.state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !tecmo_gameplay_scene_test_draw_exact_step(&TEST_SCENE)))) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "dunk/TGCS variant-0 replay failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            if ((TEST_SCENE.shot_frame == 22U && TEST_SCENE.close_shot_step != 22U) ||
                (TEST_SCENE.shot_frame == 23U && TEST_SCENE.close_shot_step != 22U) ||
                (TEST_SCENE.shot_frame == 70U && TEST_SCENE.close_shot_step != 22U) ||
                (TEST_SCENE.shot_frame == 71U && TEST_SCENE.close_shot_step != 23U) ||
                (TEST_SCENE.shot_frame == 79U && TEST_SCENE.close_shot_step != 31U) ||
                (TEST_SCENE.shot_frame == 86U && TEST_SCENE.audio_player.dmc.active) ||
                (TEST_SCENE.shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME &&
                 (!TEST_SCENE.audio_player.dmc.active ||
                  TEST_SCENE.audio_player.dmc.byte_index != 0U ||
                  TEST_SCENE.audio_player.dmc.byte_count !=
                      TEST_SCENE.audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].byte_count ||
                  TEST_SCENE.audio_player.dmc.pool_index !=
                      TEST_SCENE.audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].pool_index)) ||
                (TEST_SCENE.shot_frame == 88U &&
                 TEST_SCENE.audio_player.dmc.byte_index != 1U)) {
                tecmo_gameplay_scene_test_message(message, message_size,
                                   "dunk presentation timing/audio boundary failed");
                tecmo_gameplay_scene_destroy(&TEST_SCENE);
                return false;
            }
            if (TEST_SCENE.shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME) {
                /* A later accidental requeue would reset this sentinel. */
                TEST_SCENE.audio_player.dmc.byte_index = 1U;
            }
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        TEST_SCENE.actors[TEST_SCENE.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.controlled_actor[1] != TEST_SCENE.ball_holder ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 12U ||
        !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
        scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "dunk/TGCS variant-0 settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 12U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "dunk side-result mailbox was not last-write-wins");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x = 578;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    /* Left-facing animation mirrors the supported direction-0 slice; no ROM
       mapping to another TGCS direction entry is claimed by this milestone. */
    TEST_SCENE.actors[TEST_SCENE.ball_holder].facing_right = false;
    scene_attach_ball(&TEST_SCENE);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_LAYUP ||
        TEST_SCENE.close_shot_step != 0U ||
        TEST_SCENE.close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        TEST_SCENE.close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
        scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
        !scene_close_pose_for_step(&TEST_SCENE, 0U, &expected_pose) ||
        TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index != expected_pose ||
        !tecmo_gameplay_scene_test_draw_exact_step(&TEST_SCENE)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "NES B layup/TGCS variant-2 contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    close_transition_serial =
        TEST_SCENE.state.close_shot_subtype01.transition_serial;
    shot_actor = TEST_SCENE.shot_actor;
    for (frame = 0U; frame < 24U &&
         TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p2, 0, sizeof(p2));
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (!scene_close_pose_for_step(&TEST_SCENE, TEST_SCENE.close_shot_step,
                                          &expected_pose) ||
               TEST_SCENE.actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
               scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
               TEST_SCENE.state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !tecmo_gameplay_scene_test_draw_exact_step(&TEST_SCENE)))) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "layup/TGCS variant-2 replay failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        TEST_SCENE.actors[TEST_SCENE.ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        TEST_SCENE.controlled_actor[0] != TEST_SCENE.ball_holder ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 11U ||
        !scene_test_close_semantic_chain_untouched(&TEST_SCENE) ||
        scene_test_has_close_semantic_event(&TEST_SCENE.events) ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "layup/TGCS variant-2 settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 11U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "layup crowd-only mailbox boundary failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x = 0x013CU;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y = 180;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].facing_right = true;
    scene_attach_ball(&TEST_SCENE);
    TEST_SCENE.action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.action_serial != 1U ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "pressed-only NES B started a jump shot");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    /* The player may have last moved away from the offensive hoop. Launch
       ownership, not stale movement facing, must select and face the target. */
    rattle_before = TEST_SCENE;
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
        tecmo_gameplay_scene_test_message(message, message_size,
                           "away offensive-hoop shot ownership failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "away mirrored jump route did not advance");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }

    TEST_SCENE.actors[TEST_SCENE.ball_holder].facing_right = true;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.x = 0x0108;
    TEST_SCENE.actors[TEST_SCENE.ball_holder].position.y = 0x0070;
    TEST_SCENE.action_serial = 1U;
    scene_attach_ball(&TEST_SCENE);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.action_serial != 1U ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary two-point make was accepted");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    rattle_before = TEST_SCENE;
    if (!scene_handoff_possession(
            &rattle_before, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home-side shot ownership setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
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
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home offensive-hoop shot ownership failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X * 256) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "home mirrored jump route did not advance");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position.x =
        (int16_t)(TEST_SCENE.orientation_state.offensive_hoop.x + 50U);
    TEST_SCENE.actors[0].position.y = 128;
    TEST_SCENE.actors[0].facing_right = true;
    scene_attach_ball(&TEST_SCENE);
    rattle_before = TEST_SCENE;
    if (tecmo_gameplay_scene_start_rim_rattle_debug(&TEST_SCENE) ||
        memcmp(&TEST_SCENE, &rattle_before, sizeof(TEST_SCENE)) != 0) {
        tecmo_gameplay_scene_test_message(
            message, message_size,
            "rim-rattle rejected diagnostic mutated scene");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position.x = 0x013CU;
    TEST_SCENE.actors[0].position.y = 180;
    TEST_SCENE.actors[0].facing_right = true;
    scene_attach_ball(&TEST_SCENE);

    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    TEST_SCENE.action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        TEST_SCENE.shot_frame != 1U || TEST_SCENE.action_serial != 1U ||
        !scene_test_jump_make_checkpoint(&TEST_SCENE, 1U) ||
        TEST_SCENE.audio_player.dmc.active) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-make launch: shot=%u frame=%u serial=%u oracle=%u make=%u outcome=%u state=%u phase=%u alt=%u vel=%u pose=%u",
            (unsigned)TEST_SCENE.shot_kind, (unsigned)TEST_SCENE.shot_frame,
            (unsigned)TEST_SCENE.action_serial,
            TEST_SCENE.jump_oracle_active ? 1U : 0U,
            TEST_SCENE.jump_make_route ? 1U : 0U,
            (unsigned)TEST_SCENE.jump_outcome,
            (unsigned)TEST_SCENE.jump_actor_state,
            (unsigned)TEST_SCENE.jump_phase_counter,
            (unsigned)TEST_SCENE.jump_actor_altitude_q8,
            (unsigned)TEST_SCENE.jump_actor_velocity_q8,
            TEST_SCENE.shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index
                : UINT_MAX);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 2U; frame <= 8U; ++frame) {
        memset(&p1, 0, sizeof(p1));
        p1.held.cancel = true;
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            !scene_test_jump_make_checkpoint(
                &TEST_SCENE, (uint16_t)frame)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "ordinary-jump make held-B schedule failed");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 9U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        bool terminal = frame == TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (!terminal &&
             !scene_test_jump_make_checkpoint(
                 &TEST_SCENE, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "ordinary-jump make checkpoint %u failed",
                           (unsigned)frame);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        TEST_SCENE.actors[TEST_SCENE.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.controlled_actor[1] != TEST_SCENE.ball_holder ||
        TEST_SCENE.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 11U ||
        TEST_SCENE.events.count != 0U || TEST_SCENE.jump_oracle_active ||
        TEST_SCENE.jump_make_route ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump make settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 11U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump make crowd-only audio failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    if (!tecmo_gameplay_set_score(
            &TEST_SCENE.state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump early-release setup failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position.x = 0x013CU;
    TEST_SCENE.actors[0].position.y = 180;
    TEST_SCENE.actors[0].facing_right = true;
    scene_attach_ball(&TEST_SCENE);
    TEST_SCENE.action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_frame != 1U || !TEST_SCENE.jump_make_route) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump early-release launch failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_frame != TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME ||
        !TEST_SCENE.jump_b_released ||
        TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index !=
            TECMO_GAMEPLAY_JUMP_RELEASE_POSE) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump early release did not normalize");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 10U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2)) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "ordinary-jump early-release route stalled");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 11U ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump early-release settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    if (!scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "rim-rattle diagnostic reset failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position.x = 0x013CU;
    TEST_SCENE.actors[0].position.y = 180;
    TEST_SCENE.actors[0].facing_right = true;
    scene_attach_ball(&TEST_SCENE);
    away_score_before = TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_start_rim_rattle_debug(&TEST_SCENE) ||
        TEST_SCENE.shot_frame != 1U ||
        TEST_SCENE.shot_duration != TECMO_GAMEPLAY_JUMP_RATTLE_DURATION ||
        !TEST_SCENE.jump_rim_rattle_debug ||
        TEST_SCENE.jump_rim_rattle_raw_selector != 0x71U ||
        TEST_SCENE.jump_rim_rattle_audio_repeats != 0U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "rim-rattle diagnostic launch failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 2U; frame <= TECMO_GAMEPLAY_JUMP_RATTLE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (frame < TECMO_GAMEPLAY_JUMP_RATTLE_DURATION &&
             !scene_test_jump_rattle_checkpoint(
                 &TEST_SCENE, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "rim-rattle checkpoint %u diverged",
                           (unsigned)frame);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            away_score_before ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 12U ||
        TEST_SCENE.jump_rim_rattle_debug ||
        TEST_SCENE.jump_rim_rattle_audio_repeats != 0U ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "rim-rattle diagnostic settlement failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    if (!tecmo_gameplay_set_score(
            &TEST_SCENE.state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            &TEST_SCENE, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "ordinary-jump make test reset failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    TEST_SCENE.actors[0].position.x = 0x013CU;
    TEST_SCENE.actors[0].position.y = 180;
    TEST_SCENE.actors[0].facing_right = true;
    scene_attach_ball(&TEST_SCENE);

    /* Slot 0 follows the implementation-owned serial-2 predicted-miss branch.
       Audio from earlier close-shot coverage must not mask the no-release-DMC
       test. */
    TEST_SCENE.action_serial = 1U;
    away_score_before = TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        TEST_SCENE.shot_frame != 1U || TEST_SCENE.shot_controller != 0U ||
        TEST_SCENE.action_serial != 2U || !TEST_SCENE.jump_oracle_active ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        TEST_SCENE.jump_b_released ||
        TEST_SCENE.jump_actor_state != 0x0CU ||
        TEST_SCENE.jump_ball_state != 0x01U ||
        TEST_SCENE.jump_phase_counter != 0x31U ||
        TEST_SCENE.jump_pose_frame != 1U ||
        TEST_SCENE.jump_entry_pose_index ==
            TECMO_GAMEPLAY_JUMP_FLIGHT_POSE ||
        TEST_SCENE.jump_actor_altitude_q8 != 0x02E8U ||
        TEST_SCENE.jump_actor_velocity_q8 != 0x02E8U ||
        TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index !=
            TEST_SCENE.jump_entry_pose_index ||
        TEST_SCENE.audio_player.dmc.active) {
        char failure[256];
        (void)snprintf(
            failure, sizeof(failure),
            "NES B jump launch failed: shot=%u frame=%u controller=%u serial=%u oracle=%u make=%u outcome=%u released=%u actor=%u ball=%u phase=%u pose_frame=%u entry=%u pose=%u alt=%u vel=%u dmc=%u",
            (unsigned)TEST_SCENE.shot_kind, (unsigned)TEST_SCENE.shot_frame,
            (unsigned)TEST_SCENE.shot_controller, (unsigned)TEST_SCENE.action_serial,
            TEST_SCENE.jump_oracle_active ? 1U : 0U,
            TEST_SCENE.jump_make_route ? 1U : 0U,
            (unsigned)TEST_SCENE.jump_outcome,
            TEST_SCENE.jump_b_released ? 1U : 0U,
            (unsigned)TEST_SCENE.jump_actor_state,
            (unsigned)TEST_SCENE.jump_ball_state,
            (unsigned)TEST_SCENE.jump_phase_counter,
            (unsigned)TEST_SCENE.jump_pose_frame,
            (unsigned)TEST_SCENE.jump_entry_pose_index,
            TEST_SCENE.shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index
                : UINT_MAX,
            (unsigned)TEST_SCENE.jump_actor_altitude_q8,
            (unsigned)TEST_SCENE.jump_actor_velocity_q8,
            TEST_SCENE.audio_player.dmc.active ? 1U : 0U);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    jump_entry_pose = TEST_SCENE.jump_entry_pose_index;
    draw_probe = TEST_SCENE;
    draw_probe.jump_pose_frame = 0U;
    rattle_before = draw_probe;
    if (scene_update_jump_miss(&draw_probe, &p1) ||
        memcmp(&draw_probe, &rattle_before, sizeof(draw_probe)) != 0) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "malformed jump pose counter mutated playback");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
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
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            TEST_SCENE.shot_frame != 1U || TEST_SCENE.jump_b_released ||
            TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
            TEST_SCENE.jump_actor_state != 0x0CU ||
            TEST_SCENE.jump_ball_state != 0x01U ||
            TEST_SCENE.jump_pose_frame != frame ||
            TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index != windup_pose ||
            TEST_SCENE.jump_actor_altitude_q8 != 0x02E8U ||
            TEST_SCENE.jump_actor_velocity_q8 != 0x02E8U ||
            TEST_SCENE.audio_player.dmc.active) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "current-B held jump pose diverged");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
        TEST_SCENE.shot_frame != 2U || !TEST_SCENE.jump_b_released ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        TEST_SCENE.jump_actor_state != 0x0DU ||
        TEST_SCENE.jump_ball_state != 0x05U ||
        TEST_SCENE.jump_phase_counter != 0x04U ||
        TEST_SCENE.jump_pose_frame != 9U ||
        TEST_SCENE.actors[TEST_SCENE.shot_actor].pose_index != 1061U ||
        TEST_SCENE.jump_actor_altitude_q8 != 0x02E8U ||
        TEST_SCENE.jump_actor_velocity_q8 != 0x02E8U ||
        TEST_SCENE.audio_player.dmc.active) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "current-B jump release diverged");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    for (frame = 3U; frame <= 87U; ++frame) {
        if (!tecmo_gameplay_scene_update(&TEST_SCENE, &p1, &p2) ||
            (frame < 87U &&
             !scene_test_jump_slot0_checkpoint(&TEST_SCENE, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "jump-shot checkpoint %u diverged",
                           (unsigned)frame);
            tecmo_gameplay_scene_test_message(message, message_size, failure);
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
        if (frame < 87U &&
            TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
            tecmo_gameplay_scene_test_message(message, message_size,
                               "jump-shot actor/ball lifetime ended early");
            tecmo_gameplay_scene_destroy(&TEST_SCENE);
            return false;
        }
    }
    if (TEST_SCENE.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        TEST_SCENE.shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        TEST_SCENE.state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        TEST_SCENE.actors[TEST_SCENE.ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        TEST_SCENE.controlled_actor[1] != TEST_SCENE.ball_holder ||
        TEST_SCENE.state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        TEST_SCENE.state.clock_divider !=
            TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 12U ||
        TEST_SCENE.events.count != 0U || TEST_SCENE.jump_oracle_active ||
        TEST_SCENE.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&TEST_SCENE.state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-shot settlement failed: shot=%u score=%u-%u possession=%u holder=%u clock=%u/%u sfx=%u/%u events=%u outcome=%u",
            (unsigned)TEST_SCENE.shot_kind,
            (unsigned)TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_AWAY],
            (unsigned)TEST_SCENE.state.score[TECMO_GAMEPLAY_TEAM_HOME],
            (unsigned)TEST_SCENE.state.possession, (unsigned)TEST_SCENE.ball_holder,
            (unsigned)TEST_SCENE.state.shot_clock,
            (unsigned)TEST_SCENE.state.clock_divider,
            TEST_SCENE.audio_player.sfx_pending ? 1U : 0U,
            (unsigned)TEST_SCENE.audio_player.pending_sfx_id,
            (unsigned)TEST_SCENE.events.count, (unsigned)TEST_SCENE.jump_outcome);
        tecmo_gameplay_scene_test_message(message, message_size, failure);
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_render_samples(&TEST_SCENE.audio_player, NULL, 1024U);
    if (TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.current_sfx_id != 12U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "jump-miss side-result mailbox was not consumed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_audio_stop_all(&TEST_SCENE.audio_player);
    TEST_SCENE.state.clock_minutes = 0U;
    TEST_SCENE.state.clock_seconds = 1U;
    if (!scene_shot_queue_result_audio(&TEST_SCENE, TECMO_GAMEPLAY_TEAM_HOME) ||
        !TEST_SCENE.audio_player.sfx_pending ||
        TEST_SCENE.audio_player.pending_sfx_id != 11U) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "side-result clock gate below two seconds failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
    tecmo_gameplay_scene_end(&TEST_SCENE);
    if (TEST_SCENE.active || TEST_SCENE.result_ready || !TEST_SCENE.available) {
        tecmo_gameplay_scene_test_message(message, message_size,
                           "scene end lifecycle contract failed");
        tecmo_gameplay_scene_destroy(&TEST_SCENE);
        return false;
    }
#undef TEST_SCENE
    test->launch = launch;
    test->p1 = p1;
    test->p2 = p2;
    return true;
}
