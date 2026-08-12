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

typedef struct TecmoGameplaySceneShotClockTestRun {
    TecmoGameplayScene *scene;
    TecmoGameplaySceneLaunch launch;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    char *message;
    size_t message_size;
} TecmoGameplaySceneShotClockTestRun;

static bool scene_test_shot_clock_fail(
    TecmoGameplaySceneShotClockTestRun *run,
    const char *message)
{
    tecmo_gameplay_scene_test_message(
        run->message, run->message_size, message);
    tecmo_gameplay_scene_destroy(run->scene);
    return false;
}

static bool scene_test_shot_clock_live_input_pass(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    uint8_t holder;
    int16_t x;
    int16_t y;

    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    x = scene->actors[scene->controlled_actor[0]].position.x;
    p1.held.confirm = true;
    p1.pressed.confirm = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        return scene_test_shot_clock_fail(
            run, "START changed live gameplay state");
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.right = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x ||
        scene->actors[scene->controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT ||
        !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x + 1) {
        return scene_test_shot_clock_fail(
            run, "TGMO directional latency/movement contract failed");
    }
    x = scene->actors[scene->controlled_actor[0]].position.x;
    y = scene->actors[scene->controlled_actor[0]].position.y;
    memset(&p1, 0, sizeof(p1));
    p1.held.left = true;
    p1.held.right = true;
    p1.held.up = true;
    p1.held.down = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->actors[scene->controlled_actor[0]].position.x != x ||
        scene->actors[scene->controlled_actor[0]].position.y != y ||
        scene->actors[scene->controlled_actor[0]].movement_action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
        return scene_test_shot_clock_fail(
            run, "contradictory-axis neutral integration policy failed");
    }
    holder = scene->ball_holder;
    {
        uint8_t pass_target = scene_next_teammate(scene, holder);
        if (pass_target >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
            return scene_test_shot_clock_fail(
                run, "NES A pass target setup failed");
        }
        scene->actors[pass_target].position.x = 300;
        scene->actors[pass_target].anchor =
            scene->actors[pass_target].position;
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.shoot = true;
    p1.pressed.shoot = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        (!scene->legacy_direct_launch &&
         (!scene_pass_active(scene) || scene->ball_holder != holder)) ||
        (scene->legacy_direct_launch && scene->ball_holder == holder) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        return scene_test_shot_clock_fail(run, "NES A pass contract failed");
    }
    for (size_t pass_guard = 0U;
         scene_pass_active(scene) && pass_guard < 40U; ++pass_guard) {
        memset(&p1, 0, sizeof(p1));
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2))
            return scene_test_shot_clock_fail(run, "NES A pass update failed");
    }
    if (scene_pass_active(scene) || scene->ball_holder == holder ||
        scene->controlled_actor[0] != scene->ball_holder) {
        return scene_test_shot_clock_fail(
            run, "pass ownership invariant failed");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

static bool scene_test_shot_clock_dunk_layup(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    uint32_t close_transition_serial;
    uint16_t expected_pose;
    uint8_t shot_actor;
    size_t frame;

    scene->actors[scene->ball_holder].position.x = 174;
    scene->actors[scene->ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    scene->actors[scene->ball_holder].facing_right = true;
    /* The following close-shot checkpoints are deterministic makes. The later
       ordinary-jump checkpoint is the supported terminal miss. */
    scene->action_serial = 1U;
    scene_attach_ball(scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_DUNK ||
        scene->shot_duration != TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME ||
        scene->close_shot_step != 0U ||
        scene->close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene->close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene_test_has_close_semantic_event(&scene->events) ||
        !scene_close_pose_for_step(scene, 0U, &expected_pose) ||
        scene->actors[scene->shot_actor].pose_index != expected_pose ||
        !tecmo_gameplay_scene_test_draw_exact_step(scene)) {
        return scene_test_shot_clock_fail(
            run, "NES B dunk/TGCS variant-0 contract failed");
    }
    scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_1;
    if (scene_close_pose_for_step(scene, 0U, &expected_pose)) {
        return scene_test_shot_clock_fail(
            run, "unsupported live TGCS direction was accepted");
    }
    scene->close_shot_direction = TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0;
    close_transition_serial =
        scene->state.close_shot_subtype01.transition_serial;
    shot_actor = scene->shot_actor;
    for (frame = 0U; frame < 140U &&
         scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p1, 0, sizeof(p1));
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (tecmo_gameplay_scene_in_dunk_presentation(scene) !=
                   (scene->shot_frame >=
                        TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
                    scene->shot_frame <
                        TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME) ||
               !scene_close_pose_for_step(scene, scene->close_shot_step,
                                          &expected_pose) ||
               scene->actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(scene) ||
               scene_test_has_close_semantic_event(&scene->events) ||
               scene->state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !tecmo_gameplay_scene_test_draw_exact_step(scene)))) {
            return scene_test_shot_clock_fail(
                run, "dunk/TGCS variant-0 replay failed");
        }
        if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            if ((scene->shot_frame == 22U && scene->close_shot_step != 22U) ||
                (scene->shot_frame == 23U && scene->close_shot_step != 22U) ||
                (scene->shot_frame == 70U && scene->close_shot_step != 22U) ||
                (scene->shot_frame == 71U && scene->close_shot_step != 23U) ||
                (scene->shot_frame == 79U && scene->close_shot_step != 31U) ||
                (scene->shot_frame == 86U && scene->audio_player.dmc.active) ||
                (scene->shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME &&
                 (!scene->audio_player.dmc.active ||
                  scene->audio_player.dmc.byte_index != 0U ||
                  scene->audio_player.dmc.byte_count !=
                      scene->audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].byte_count ||
                  scene->audio_player.dmc.pool_index !=
                      scene->audio_asset.dmc_clips[
                          TECMO_GAMEPLAY_DMC_BANK05_A9C5].pool_index)) ||
                (scene->shot_frame == 88U &&
                 scene->audio_player.dmc.byte_index != 1U)) {
                return scene_test_shot_clock_fail(
                    run, "dunk presentation timing/audio boundary failed");
            }
            if (scene->shot_frame == TECMO_GAMEPLAY_DUNK_A9C5_FRAME) {
                /* A later accidental requeue would reset this sentinel. */
                scene->audio_player.dmc.byte_index = 1U;
            }
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->controlled_actor[1] != scene->ball_holder ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 12U ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene_test_has_close_semantic_event(&scene->events) ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "dunk/TGCS variant-0 settlement failed");
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 12U) {
        return scene_test_shot_clock_fail(
            run, "dunk side-result mailbox was not last-write-wins");
    }

    scene->actors[scene->ball_holder].position.x = 578;
    scene->actors[scene->ball_holder].position.y =
        TECMO_GAMEPLAY_COURT_HOOP_Y;
    /* Left-facing animation mirrors the supported direction-0 slice; no ROM
       mapping to another TGCS direction entry is claimed by this milestone. */
    scene->actors[scene->ball_holder].facing_right = false;
    scene_attach_ball(scene);
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_LAYUP ||
        scene->close_shot_step != 0U ||
        scene->close_shot_profile != TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 ||
        scene->close_shot_direction !=
            TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene_test_has_close_semantic_event(&scene->events) ||
        !scene_close_pose_for_step(scene, 0U, &expected_pose) ||
        scene->actors[scene->shot_actor].pose_index != expected_pose ||
        !tecmo_gameplay_scene_test_draw_exact_step(scene)) {
        return scene_test_shot_clock_fail(
            run, "NES B layup/TGCS variant-2 contract failed");
    }
    close_transition_serial =
        scene->state.close_shot_subtype01.transition_serial;
    shot_actor = scene->shot_actor;
    for (frame = 0U; frame < 24U &&
         scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE; ++frame) {
        memset(&p2, 0, sizeof(p2));
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
              (!scene_close_pose_for_step(scene, scene->close_shot_step,
                                          &expected_pose) ||
               scene->actors[shot_actor].pose_index != expected_pose ||
               !scene_test_close_semantic_chain_untouched(scene) ||
               scene_test_has_close_semantic_event(&scene->events) ||
               scene->state.close_shot_subtype01.transition_serial !=
                   close_transition_serial ||
               !tecmo_gameplay_scene_test_draw_exact_step(scene)))) {
            return scene_test_shot_clock_fail(
                run, "layup/TGCS variant-2 replay failed");
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->controlled_actor[0] != scene->ball_holder ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        !scene_test_close_semantic_chain_untouched(scene) ||
        scene_test_has_close_semantic_event(&scene->events) ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "layup/TGCS variant-2 settlement failed");
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 11U) {
        return scene_test_shot_clock_fail(
            run, "layup crowd-only mailbox boundary failed");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

static bool scene_test_shot_clock_jump_targeting(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    TecmoGameplayScene rattle_before;
    TecmoGameplaySceneActor rejected_actor_before;
    TecmoGameplayCourtCoordinateQ8 rejected_ball_before;
    uint8_t rejected_holder_before;
    uint32_t rejected_action_serial_before;
    TecmoGameplayCameraState camera_before;

    scene->actors[scene->ball_holder].position.x = 0x013CU;
    scene->actors[scene->ball_holder].position.y = 180;
    scene->actors[scene->ball_holder].facing_right = true;
    scene_attach_ball(scene);
    scene->action_serial = 1U;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->action_serial != 1U ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return scene_test_shot_clock_fail(
            run, "pressed-only NES B started a jump shot");
    }

    /* The player may have last moved away from the offensive hoop. Launch
       ownership, not stale movement facing, must select and face the target. */
    rattle_before = *scene;
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
        return scene_test_shot_clock_fail(
            run, "away offensive-hoop shot ownership failed");
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X * 256) {
        return scene_test_shot_clock_fail(
            run, "away mirrored jump route did not advance");
    }

    scene->actors[scene->ball_holder].facing_right = true;
    scene->actors[scene->ball_holder].position.x = 0x0108;
    scene->actors[scene->ball_holder].position.y = 0x0070;
    scene->action_serial = 1U;
    scene_attach_ball(scene);
    rejected_holder_before = scene->ball_holder;
    rejected_actor_before = scene->actors[rejected_holder_before];
    rejected_ball_before = scene->ball_position;
    rejected_action_serial_before = scene->action_serial;
    if (scene_start_shot_actor(
            scene, 0U, rejected_holder_before) ||
        memcmp(&scene->actors[rejected_holder_before],
               &rejected_actor_before, sizeof(rejected_actor_before)) != 0 ||
        scene->ball_holder != rejected_holder_before ||
        scene->ball_position.x_q8 != rejected_ball_before.x_q8 ||
        scene->ball_position.y_q8 != rejected_ball_before.y_q8 ||
        scene->action_serial != rejected_action_serial_before) {
        return scene_test_shot_clock_fail(
            run, "ordinary two-point make rejection was not transactional");
    }
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->action_serial != 1U ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "ordinary two-point make was accepted");
    }
    rattle_before = *scene;
    if (!scene_handoff_possession(
            &rattle_before, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
        return scene_test_shot_clock_fail(
            run, "home-side shot ownership setup failed");
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
        return scene_test_shot_clock_fail(
            run, "home offensive-hoop shot ownership failed");
    }
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_update(&rattle_before, &p1, &p2) ||
        rattle_before.shot_frame != 2U ||
        rattle_before.jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        rattle_before.shot_end_position.x_q8 !=
            TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X * 256) {
        return scene_test_shot_clock_fail(
            run, "home mirrored jump route did not advance");
    }
    scene->actors[0].position.x =
        (int16_t)(scene->orientation_state.offensive_hoop.x + 50U);
    scene->actors[0].position.y = 128;
    scene->actors[0].facing_right = true;
    scene_attach_ball(scene);
    rattle_before = *scene;
    if (tecmo_gameplay_scene_start_rim_rattle_debug(scene) ||
        memcmp(scene, &rattle_before, sizeof(*scene)) != 0) {
        return scene_test_shot_clock_fail(
            run, "rim-rattle rejected diagnostic mutated scene");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

static bool scene_test_shot_clock_jump_make(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    size_t frame;

    scene->actors[0].position.x = 0x013CU;
    scene->actors[0].position.y = 180;
    scene->actors[0].facing_right = true;
    scene_attach_ball(scene);

    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    scene->action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != 1U || scene->action_serial != 1U ||
        !scene_test_jump_make_checkpoint(scene, 1U) ||
        scene->audio_player.dmc.active) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-make launch: shot=%u frame=%u serial=%u oracle=%u make=%u outcome=%u state=%u phase=%u alt=%u vel=%u pose=%u",
            (unsigned)scene->shot_kind, (unsigned)scene->shot_frame,
            (unsigned)scene->action_serial,
            scene->jump_oracle_active ? 1U : 0U,
            scene->jump_make_route ? 1U : 0U,
            (unsigned)scene->jump_outcome,
            (unsigned)scene->jump_actor_state,
            (unsigned)scene->jump_phase_counter,
            (unsigned)scene->jump_actor_altitude_q8,
            (unsigned)scene->jump_actor_velocity_q8,
            scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)scene->actors[scene->shot_actor].pose_index
                : UINT_MAX);
        return scene_test_shot_clock_fail(run, failure);
    }
    for (frame = 2U; frame <= 8U; ++frame) {
        memset(&p1, 0, sizeof(p1));
        p1.held.cancel = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_test_jump_make_checkpoint(
                scene, (uint16_t)frame)) {
            return scene_test_shot_clock_fail(
                run, "ordinary-jump make held-B schedule failed");
        }
    }
    memset(&p1, 0, sizeof(p1));
    for (frame = 9U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        bool terminal = frame == TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (!terminal &&
             !scene_test_jump_make_checkpoint(
                 scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "ordinary-jump make checkpoint %u failed",
                           (unsigned)frame);
            return scene_test_shot_clock_fail(run, failure);
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->controlled_actor[1] != scene->ball_holder ||
        scene->state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        scene->events.count != 0U || scene->jump_oracle_active ||
        scene->jump_make_route ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump make settlement failed");
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 11U) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump make crowd-only audio failed");
    }
    if (!tecmo_gameplay_set_score(
            &scene->state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump early-release setup failed");
    }
    scene->actors[0].position.x = 0x013CU;
    scene->actors[0].position.y = 180;
    scene->actors[0].facing_right = true;
    scene_attach_ball(scene);
    scene->action_serial = 0U;
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != 1U || !scene->jump_make_route) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump early-release launch failed");
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME ||
        !scene->jump_b_released ||
        scene->actors[scene->shot_actor].pose_index !=
            TECMO_GAMEPLAY_JUMP_RELEASE_POSE) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump early release did not normalize");
    }
    for (frame = 10U; frame <= TECMO_GAMEPLAY_JUMP_MAKE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            return scene_test_shot_clock_fail(
                run, "ordinary-jump early-release route stalled");
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 5U ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump early-release settlement failed");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

static bool scene_test_shot_clock_rim_rattle_period_expiry(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    uint16_t away_score_before;
    size_t frame;

    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    if (!scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return scene_test_shot_clock_fail(
            run, "rim-rattle diagnostic reset failed");
    }
    scene->actors[0].position.x = 0x013CU;
    scene->actors[0].position.y = 180;
    scene->actors[0].facing_right = true;
    scene_attach_ball(scene);
    away_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    if (!tecmo_gameplay_scene_start_rim_rattle_debug(scene) ||
        scene->shot_frame != 1U ||
        scene->shot_duration != TECMO_GAMEPLAY_JUMP_RATTLE_DURATION ||
        !scene->jump_rim_rattle_debug ||
        scene->jump_rim_rattle_raw_selector != 0x71U ||
        scene->jump_rim_rattle_audio_repeats != 0U) {
        return scene_test_shot_clock_fail(
            run, "rim-rattle diagnostic launch failed");
    }
    for (frame = 2U; frame <= TECMO_GAMEPLAY_JUMP_RATTLE_DURATION;
         ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (frame < TECMO_GAMEPLAY_JUMP_RATTLE_DURATION &&
             !scene_test_jump_rattle_checkpoint(
                 scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "rim-rattle checkpoint %u diverged",
                           (unsigned)frame);
            return scene_test_shot_clock_fail(run, failure);
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] !=
            away_score_before ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 12U ||
        scene->jump_rim_rattle_debug ||
        scene->jump_rim_rattle_audio_repeats != 0U ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        return scene_test_shot_clock_fail(
            run, "rim-rattle diagnostic settlement failed");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

static bool scene_test_shot_clock_terminal_miss_lifecycle(
    TecmoGameplaySceneShotClockTestRun *run)
{
    TecmoGameplayScene *scene = run->scene;
    TecmoControlFrame p1 = run->p1;
    TecmoControlFrame p2 = run->p2;
    TecmoGameplayScene rattle_before;
    TecmoGameplayScene draw_probe;
    uint16_t jump_entry_pose;
    uint16_t away_score_before;
    size_t frame;

    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    if (!tecmo_gameplay_set_score(
            &scene->state, TECMO_GAMEPLAY_TEAM_AWAY, 2U) ||
        !scene_handoff_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return scene_test_shot_clock_fail(
            run, "ordinary-jump make test reset failed");
    }
    scene->actors[0].position.x = 0x013CU;
    scene->actors[0].position.y = 180;
    scene->actors[0].facing_right = true;
    scene_attach_ball(scene);

    /* Slot 0 follows the implementation-owned serial-2 predicted-miss branch.
       Audio from earlier close-shot coverage must not mask the no-release-DMC
       test. */
    scene->action_serial = 1U;
    away_score_before = scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    memset(&p1, 0, sizeof(p1));
    p1.held.cancel = true;
    p1.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
        scene->shot_frame != 1U || scene->shot_controller != 0U ||
        scene->action_serial != 2U || !scene->jump_oracle_active ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        scene->jump_b_released ||
        scene->jump_actor_state != 0x0CU ||
        scene->jump_ball_state != 0x01U ||
        scene->jump_phase_counter != 0x31U ||
        scene->jump_pose_frame != 1U ||
        scene->jump_entry_pose_index ==
            TECMO_GAMEPLAY_JUMP_FLIGHT_POSE ||
        scene->jump_actor_altitude_q8 != 0x02E8U ||
        scene->jump_actor_velocity_q8 != 0x02E8U ||
        scene->actors[scene->shot_actor].pose_index !=
            scene->jump_entry_pose_index ||
        scene->audio_player.dmc.active) {
        char failure[256];
        (void)snprintf(
            failure, sizeof(failure),
            "NES B jump launch failed: shot=%u frame=%u controller=%u serial=%u oracle=%u make=%u outcome=%u released=%u actor=%u ball=%u phase=%u pose_frame=%u entry=%u pose=%u alt=%u vel=%u dmc=%u",
            (unsigned)scene->shot_kind, (unsigned)scene->shot_frame,
            (unsigned)scene->shot_controller, (unsigned)scene->action_serial,
            scene->jump_oracle_active ? 1U : 0U,
            scene->jump_make_route ? 1U : 0U,
            (unsigned)scene->jump_outcome,
            scene->jump_b_released ? 1U : 0U,
            (unsigned)scene->jump_actor_state,
            (unsigned)scene->jump_ball_state,
            (unsigned)scene->jump_phase_counter,
            (unsigned)scene->jump_pose_frame,
            (unsigned)scene->jump_entry_pose_index,
            scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                ? (unsigned)scene->actors[scene->shot_actor].pose_index
                : UINT_MAX,
            (unsigned)scene->jump_actor_altitude_q8,
            (unsigned)scene->jump_actor_velocity_q8,
            scene->audio_player.dmc.active ? 1U : 0U);
        return scene_test_shot_clock_fail(run, failure);
    }
    jump_entry_pose = scene->jump_entry_pose_index;
    draw_probe = *scene;
    draw_probe.jump_pose_frame = 0U;
    rattle_before = draw_probe;
    if (scene_update_jump_miss(&draw_probe, &p1) ||
        memcmp(&draw_probe, &rattle_before, sizeof(draw_probe)) != 0) {
        return scene_test_shot_clock_fail(
            run, "malformed jump pose counter mutated playback");
    }
    for (frame = 2U;
         frame <= TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME; ++frame) {
        uint16_t windup_pose =
            frame <= TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME
                ? jump_entry_pose
                : TECMO_GAMEPLAY_JUMP_TURN_POSE;
        memset(&p1, 0, sizeof(p1));
        p1.held.cancel = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->shot_frame != 1U || scene->jump_b_released ||
            scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
            scene->jump_actor_state != 0x0CU ||
            scene->jump_ball_state != 0x01U ||
            scene->jump_pose_frame != frame ||
            scene->actors[scene->shot_actor].pose_index != windup_pose ||
            scene->jump_actor_altitude_q8 != 0x02E8U ||
            scene->jump_actor_velocity_q8 != 0x02E8U ||
            scene->audio_player.dmc.active) {
            return scene_test_shot_clock_fail(
                run, "current-B held jump pose diverged");
        }
    }
    memset(&p1, 0, sizeof(p1));
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->shot_frame != 2U || !scene->jump_b_released ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        scene->jump_actor_state != 0x0DU ||
        scene->jump_ball_state != 0x05U ||
        scene->jump_phase_counter != 0x04U ||
        scene->jump_pose_frame != 9U ||
        scene->actors[scene->shot_actor].pose_index != 1061U ||
        scene->jump_actor_altitude_q8 != 0x02E8U ||
        scene->jump_actor_velocity_q8 != 0x02E8U ||
        scene->audio_player.dmc.active) {
        return scene_test_shot_clock_fail(
            run, "current-B jump release diverged");
    }
    for (frame = 3U; frame <= 87U; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            (frame < 87U &&
             !scene_test_jump_slot0_checkpoint(scene, (uint16_t)frame))) {
            char failure[192];
            (void)snprintf(failure, sizeof(failure),
                           "jump-shot checkpoint %u diverged",
                           (unsigned)frame);
            return scene_test_shot_clock_fail(run, failure);
        }
        if (frame < 87U &&
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP) {
            return scene_test_shot_clock_fail(
                run, "jump-shot actor/ball lifetime ended early");
        }
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->shot_actor != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != away_score_before ||
        scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY] != 2U ||
        scene->state.score[TECMO_GAMEPLAY_TEAM_HOME] != 2U ||
        scene->actors[scene->ball_holder].team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->controlled_actor[1] != scene->ball_holder ||
        scene->state.shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        scene->state.clock_divider !=
            TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 12U ||
        scene->events.count != 0U || scene->jump_oracle_active ||
        scene->jump_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN ||
        !tecmo_gameplay_state_valid(&scene->state)) {
        char failure[192];
        (void)snprintf(
            failure, sizeof(failure),
            "jump-shot settlement failed: shot=%u score=%u-%u possession=%u holder=%u clock=%u/%u sfx=%u/%u events=%u outcome=%u",
            (unsigned)scene->shot_kind,
            (unsigned)scene->state.score[TECMO_GAMEPLAY_TEAM_AWAY],
            (unsigned)scene->state.score[TECMO_GAMEPLAY_TEAM_HOME],
            (unsigned)scene->state.possession, (unsigned)scene->ball_holder,
            (unsigned)scene->state.shot_clock,
            (unsigned)scene->state.clock_divider,
            scene->audio_player.sfx_pending ? 1U : 0U,
            (unsigned)scene->audio_player.pending_sfx_id,
            (unsigned)scene->events.count, (unsigned)scene->jump_outcome);
        return scene_test_shot_clock_fail(run, failure);
    }
    tecmo_gameplay_audio_render_samples(&scene->audio_player, NULL, 1024U);
    if (scene->audio_player.sfx_pending ||
        scene->audio_player.current_sfx_id != 12U) {
        return scene_test_shot_clock_fail(
            run, "jump-miss side-result mailbox was not consumed");
    }
    tecmo_gameplay_audio_stop_all(&scene->audio_player);
    scene->state.clock_minutes = 0U;
    scene->state.clock_seconds = 1U;
    if (!scene_shot_queue_result_audio(scene, TECMO_GAMEPLAY_TEAM_HOME) ||
        !scene->audio_player.sfx_pending ||
        scene->audio_player.pending_sfx_id != 11U) {
        return scene_test_shot_clock_fail(
            run, "side-result clock gate below two seconds failed");
    }
    tecmo_gameplay_scene_end(scene);
    if (scene->active || scene->result_ready || !scene->available) {
        return scene_test_shot_clock_fail(
            run, "scene end lifecycle contract failed");
    }
    run->p1 = p1;
    run->p2 = p2;
    return true;
}

bool tecmo_gameplay_scene_test_shot_clock(
    TecmoGameplaySceneTestContext *test)
{
    TecmoGameplaySceneShotClockTestRun run;

    run.scene = test->scene;
    run.launch = test->launch;
    run.p1 = test->p1;
    run.p2 = test->p2;
    run.message = test->message;
    run.message_size = test->message_size;

    if (!scene_test_shot_clock_live_input_pass(&run) ||
        !scene_test_shot_clock_dunk_layup(&run) ||
        !scene_test_shot_clock_jump_targeting(&run) ||
        !scene_test_shot_clock_jump_make(&run) ||
        !scene_test_shot_clock_rim_rattle_period_expiry(&run) ||
        !scene_test_shot_clock_terminal_miss_lifecycle(&run)) {
        return false;
    }
    test->launch = run.launch;
    test->p1 = run.p1;
    test->p2 = run.p2;
    return true;
}
