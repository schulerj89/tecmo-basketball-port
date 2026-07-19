#include "tecmo_gameplay_state.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool gameplay_team_valid(TecmoGameplayTeam team)
{
    return team == TECMO_GAMEPLAY_TEAM_AWAY ||
           team == TECMO_GAMEPLAY_TEAM_HOME;
}

static bool gameplay_violation_valid(TecmoGameplayViolation violation)
{
    return violation >= TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS &&
           violation <= TECMO_GAMEPLAY_VIOLATION_GOALTENDING;
}

static bool gameplay_live_context_valid(const TecmoGameplayLiveContext *context)
{
    return context != NULL &&
           context->period_expiry >= TECMO_GAMEPLAY_EXPIRY_OTHER &&
           context->period_expiry < TECMO_GAMEPLAY_EXPIRY_CONTEXT_COUNT;
}

static bool gameplay_event_append(TecmoGameplayEventBuffer *events,
                                  TecmoGameplayEventKind kind,
                                  uint16_t value,
                                  uint16_t detail)
{
    TecmoGameplayEvent *event;

    if (events == NULL || events->count >= TECMO_GAMEPLAY_EVENT_CAPACITY ||
        kind < TECMO_GAMEPLAY_EVENT_SFX_REQUEST ||
        kind >= TECMO_GAMEPLAY_EVENT_KIND_COUNT) {
        return false;
    }

    event = &events->events[events->count++];
    event->kind = kind;
    event->value = value;
    event->detail = detail;
    return true;
}

static bool gameplay_close_shot_observation_valid(
    TecmoGameplayCloseShotObservation observation)
{
    return observation >= TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY &&
           observation < TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVATION_COUNT;
}

static bool gameplay_close_shot_observed_pose(
    TecmoGameplayCloseShotObservation observation,
    TecmoGameplayCloseShotPhase phase,
    uint16_t *actor_pose,
    uint16_t *ball_pose)
{
    static const uint16_t rightward_trace_poses[] = {
        509U, 445U, 254U, 255U, 257U, 258U, 259U
    };

    if (actor_pose == NULL || ball_pose == NULL ||
        observation != TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVED_RIGHTWARD_TRACE ||
        phase < TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL ||
        phase >= TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT) {
        return false;
    }
    *actor_pose = rightward_trace_poses[(size_t)phase];
    *ball_pose = 64U;
    return true;
}

static void gameplay_close_shot_set_phase(TecmoGameplayCloseShotState *shot,
                                          TecmoGameplayCloseShotPhase phase)
{
    shot->phase = phase;
    shot->observed_pose_available = gameplay_close_shot_observed_pose(
        shot->observation,
        phase,
        &shot->observed_actor_pose_index,
        &shot->observed_ball_pose_index);
    if (!shot->observed_pose_available) {
        shot->observed_actor_pose_index = UINT16_MAX;
        shot->observed_ball_pose_index = UINT16_MAX;
    }
    shot->active = phase != TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL;
    ++shot->transition_serial;
}

static void gameplay_clock_reset(TecmoGameplayState *state, uint8_t minutes)
{
    state->clock_minutes = minutes;
    state->clock_seconds = 0U;
    state->clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    state->period_expiry_zero_action_observed = false;
}

static void gameplay_prepare_period_clock(TecmoGameplayState *state,
                                          uint8_t minutes)
{
    state->clock_minutes = minutes;
    state->clock_seconds = 0U;
    state->clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;
    state->period_expiry_zero_action_observed = false;
}

static void gameplay_finish_period_banner_reset(TecmoGameplayState *state)
{
    memset(state->team_fouls, 0, sizeof(state->team_fouls));
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    state->clock_divider = TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES;
    state->period_expiry_zero_action_observed = false;
}

static bool gameplay_enter_violation(TecmoGameplayState *state,
                                     TecmoGameplayViolation violation,
                                     TecmoGameplayTeam restart_possession)
{
    if (!gameplay_violation_valid(violation) ||
        !gameplay_team_valid(restart_possession)) {
        return false;
    }

    state->phase = TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION;
    state->phase_frame = 0U;
    state->violation = violation;
    state->restart_possession = restart_possession;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    return true;
}

static bool gameplay_start_period_banner(TecmoGameplayState *state,
                                         TecmoGameplayBanner banner)
{
    uint8_t minutes;

    if (banner == TECMO_GAMEPLAY_BANNER_OVERTIME) {
        minutes = tecmo_gameplay_overtime_minutes(
            state->config.regulation_minutes);
    } else if (banner == TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
               banner == TECMO_GAMEPLAY_BANNER_THIRD_PERIOD ||
               banner == TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD) {
        minutes = state->config.regulation_minutes;
    } else {
        return false;
    }

    if (minutes == 0U) {
        return false;
    }

    /* Fixed E823 prepares M:00/divider 45 before the banner. */
    gameplay_prepare_period_clock(state, minutes);
    state->phase = TECMO_GAMEPLAY_PHASE_PERIOD_BANNER;
    state->phase_frame = 0U;
    state->expiry_wait_frames_remaining = 0U;
    state->banner = banner;
    state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;
    return true;
}

static bool gameplay_finish_period(TecmoGameplayState *state,
                                   TecmoGameplayEventBuffer *events)
{
    const uint8_t prepared_minutes =
        state->period == 5U
            ? tecmo_gameplay_overtime_minutes(
                  state->config.regulation_minutes)
            : state->config.regulation_minutes;
    bool tied;

    if (prepared_minutes == 0U) {
        return false;
    }

    /* `$E59B` reaches `$E823` before any completion branch is selected. */
    gameplay_prepare_period_clock(state, prepared_minutes);
    tied = state->score[TECMO_GAMEPLAY_TEAM_AWAY] ==
           state->score[TECMO_GAMEPLAY_TEAM_HOME];

    if (state->period == 5U && tied &&
        state->overtime_count == UINT8_MAX) {
        return false;
    }

    state->expiry_wait_frames_remaining = 0U;
    state->phase_frame = 0U;
    state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;

    if (state->period == 1U) {
        state->period = 2U;
        return gameplay_start_period_banner(
            state, TECMO_GAMEPLAY_BANNER_SECOND_PERIOD);
    }
    if (state->period == 2U) {
        state->period = 3U;
        state->phase = TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER;
        state->banner = TECMO_GAMEPLAY_BANNER_HALFTIME;
        return true;
    }
    if (state->period == 3U) {
        state->period = 4U;
        return gameplay_start_period_banner(
            state, TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD);
    }
    if (state->period == 4U) {
        state->period = 5U;
        if (tied) {
            state->overtime_count = 1U;
            return gameplay_start_period_banner(
                state, TECMO_GAMEPLAY_BANNER_OVERTIME);
        }
    }
    if (state->period == 5U && tied) {
        ++state->overtime_count;
        return gameplay_start_period_banner(
            state, TECMO_GAMEPLAY_BANNER_OVERTIME);
    }

    if (!gameplay_event_append(events,
                               TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                               TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID,
                               0U)) {
        return false;
    }
    state->phase = TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    return true;
}

static bool gameplay_enter_period_expiry(
    TecmoGameplayState *state,
    const TecmoGameplayLiveContext *live_context)
{
    state->phase_frame = 0U;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;

    if (live_context->period_expiry ==
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION) {
        state->period_expiry_zero_action_observed = true;
        state->phase = TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE;
        state->expiry_wait_frames_remaining = 0U;
        return true;
    }

    /* X starts at 30 and the ROM loop includes zero: exactly 31 waits. */
    state->phase = TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT;
    state->expiry_wait_frames_remaining =
        TECMO_GAMEPLAY_PERIOD_EXPIRY_WAIT_FRAMES;
    return true;
}

static bool gameplay_update_live_clock(
    TecmoGameplayState *state,
    const TecmoGameplayLiveContext *live_context,
    TecmoGameplayEventBuffer *events)
{
    bool shot_clock_expired = false;
    bool period_expired;

    if (state->clock_divider > 1U) {
        --state->clock_divider;
        return true;
    }

    state->clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;

    /* The fixed routine handles the shot clock before the game clock. */
    if (state->shot_clock > 0U) {
        if (state->clock_minutes == 0U && state->clock_seconds < 12U) {
            if (!gameplay_event_append(events,
                                       TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                       TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID,
                                       0U)) {
                return false;
            }
        }

        --state->shot_clock;
        if (state->shot_clock == 0U) {
            shot_clock_expired = true;
            if (!gameplay_event_append(events,
                                       TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                       TECMO_GAMEPLAY_SFX_EXPIRY_ID,
                                       0U) ||
                !gameplay_event_append(
                    events,
                    TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED,
                    TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK,
                    live_context->shot_clock_violation_exempt ? 1U : 0U)) {
                return false;
            }
        }
    }

    if (state->clock_seconds > 0U) {
        --state->clock_seconds;
    } else if (state->clock_minutes > 0U) {
        --state->clock_minutes;
        state->clock_seconds = 59U;
    }

    period_expired = state->clock_minutes == 0U &&
                     state->clock_seconds == 0U;
    if (period_expired) {
        if (!gameplay_event_append(events,
                                   TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                   TECMO_GAMEPLAY_SFX_EXPIRY_ID,
                                   0U)) {
            return false;
        }
        return gameplay_enter_period_expiry(state, live_context);
    }

    if (shot_clock_expired &&
        !live_context->shot_clock_violation_exempt) {
        const TecmoGameplayTeam restart =
            state->possession == TECMO_GAMEPLAY_TEAM_AWAY
                ? TECMO_GAMEPLAY_TEAM_HOME
                : TECMO_GAMEPLAY_TEAM_AWAY;
        return gameplay_enter_violation(
            state, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, restart);
    }

    return true;
}

bool tecmo_gameplay_config_init(TecmoGameplayConfig *config,
                                uint8_t regulation_minutes)
{
    if (config == NULL) {
        return false;
    }
    config->regulation_minutes = regulation_minutes;
    if (!tecmo_gameplay_config_valid(config)) {
        memset(config, 0, sizeof(*config));
        return false;
    }
    return true;
}

bool tecmo_gameplay_config_valid(const TecmoGameplayConfig *config)
{
    if (config == NULL) {
        return false;
    }

    return config->regulation_minutes == 2U ||
           config->regulation_minutes == 3U ||
           config->regulation_minutes == 4U ||
           config->regulation_minutes == 8U ||
           config->regulation_minutes == 12U;
}

uint8_t tecmo_gameplay_overtime_minutes(uint8_t regulation_minutes)
{
    switch (regulation_minutes) {
    case 2U:
    case 3U:
        return 1U;
    case 4U:
        return 2U;
    case 8U:
        return 3U;
    case 12U:
        return 5U;
    default:
        return 0U;
    }
}

void tecmo_gameplay_frame_input_clear(TecmoGameplayFrameInput *input)
{
    if (input != NULL) {
        memset(input, 0, sizeof(*input));
    }
}

void tecmo_gameplay_live_context_default(TecmoGameplayLiveContext *context)
{
    if (context != NULL) {
        memset(context, 0, sizeof(*context));
        context->period_expiry = TECMO_GAMEPLAY_EXPIRY_OTHER;
    }
}

static bool gameplay_buttons_any(const TecmoGameplayPadButtons *buttons)
{
    return buttons != NULL &&
           (buttons->dpad_up || buttons->dpad_down ||
            buttons->dpad_left || buttons->dpad_right ||
            buttons->nes_a_pass_switch ||
            buttons->nes_b_jump_steal_shot ||
            buttons->nes_select || buttons->nes_start);
}

static bool gameplay_pad_any(const TecmoGameplayPadInput *pad)
{
    return pad != NULL &&
           (gameplay_buttons_any(&pad->held) ||
            gameplay_buttons_any(&pad->released));
}

bool tecmo_gameplay_input_any(const TecmoGameplayFrameInput *input)
{
    if (input == NULL) {
        return false;
    }
    return gameplay_pad_any(&input->controllers[0]) ||
           gameplay_pad_any(&input->controllers[1]);
}

bool tecmo_gameplay_input_nes_a_pass_switch_held(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].held.nes_a_pass_switch;
}

bool tecmo_gameplay_input_nes_a_pass_switch_released(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].released.nes_a_pass_switch;
}

bool tecmo_gameplay_input_nes_b_jump_steal_shot_held(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].held.nes_b_jump_steal_shot;
}

bool tecmo_gameplay_input_nes_start_held(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].held.nes_start;
}

static bool gameplay_input_either_nes_a_released(
    const TecmoGameplayFrameInput *input)
{
    return tecmo_gameplay_input_nes_a_pass_switch_released(input, 0U) ||
           tecmo_gameplay_input_nes_a_pass_switch_released(input, 1U);
}

void tecmo_gameplay_events_clear(TecmoGameplayEventBuffer *events)
{
    if (events != NULL) {
        memset(events, 0, sizeof(*events));
    }
}

bool tecmo_gameplay_state_init(TecmoGameplayState *state,
                               const TecmoGameplayConfig *config,
                               TecmoGameplayTeam initial_possession)
{
    if (state == NULL) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    if (!tecmo_gameplay_config_valid(config) ||
        !gameplay_team_valid(initial_possession)) {
        return false;
    }

    state->config = *config;
    state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;
    state->possession = initial_possession;
    state->restart_possession = initial_possession;
    state->free_throws.scoring_team = initial_possession;
    state->period = 1U;
    gameplay_clock_reset(state, config->regulation_minutes);
    state->close_shot_subtype01.observation =
        TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY;
    gameplay_close_shot_set_phase(&state->close_shot_subtype01,
                                  TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL);
    state->close_shot_subtype01.transition_serial = 0U;
    state->initialized = true;
    return tecmo_gameplay_state_valid(state);
}

static bool gameplay_period_banner_matches_period(
    const TecmoGameplayState *state)
{
    switch (state->banner) {
    case TECMO_GAMEPLAY_BANNER_SECOND_PERIOD:
        return state->period == 2U && state->overtime_count == 0U;
    case TECMO_GAMEPLAY_BANNER_THIRD_PERIOD:
        return state->period == 3U && state->overtime_count == 0U;
    case TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD:
        return state->period == 4U && state->overtime_count == 0U;
    case TECMO_GAMEPLAY_BANNER_OVERTIME:
        return state->period == 5U && state->overtime_count > 0U;
    case TECMO_GAMEPLAY_BANNER_FIRST_PERIOD:
    case TECMO_GAMEPLAY_BANNER_HALFTIME:
    case TECMO_GAMEPLAY_BANNER_NONE:
    default:
        return false;
    }
}

static bool gameplay_clock_is_prepared(const TecmoGameplayState *state,
                                       uint8_t minutes)
{
    return minutes > 0U && state->clock_minutes == minutes &&
           state->clock_seconds == 0U &&
           state->clock_divider == TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;
}

bool tecmo_gameplay_state_valid(const TecmoGameplayState *state)
{
    uint8_t maximum_minutes;
    uint8_t prepared_minutes;
    uint16_t expected_actor_pose = UINT16_MAX;
    uint16_t expected_ball_pose = UINT16_MAX;
    bool expected_pose_available;

    if (state == NULL || !state->initialized ||
        !tecmo_gameplay_config_valid(&state->config) ||
        state->phase < TECMO_GAMEPLAY_PHASE_LIVE ||
        state->phase >= TECMO_GAMEPLAY_PHASE_COUNT ||
        !gameplay_team_valid(state->possession) ||
        !gameplay_team_valid(state->restart_possession) ||
        !gameplay_team_valid(state->free_throws.scoring_team) ||
        state->period < 1U || state->period > 5U ||
        state->clock_seconds > 59U ||
        state->clock_divider < 1U ||
        state->clock_divider > TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES ||
        state->shot_clock > TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
        state->free_throws.attempts_remaining > 3U ||
        state->close_shot_subtype01.phase <
            TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL ||
        state->close_shot_subtype01.phase >=
            TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT ||
        !gameplay_close_shot_observation_valid(
            state->close_shot_subtype01.observation)) {
        return false;
    }

    if (state->period < 5U) {
        maximum_minutes = state->config.regulation_minutes;
        if (state->overtime_count != 0U) {
            return false;
        }
    } else if (state->overtime_count == 0U) {
        maximum_minutes = state->config.regulation_minutes;
        if (state->phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN &&
            state->phase != TECMO_GAMEPLAY_PHASE_COMPLETE) {
            return false;
        }
    } else {
        maximum_minutes = tecmo_gameplay_overtime_minutes(
            state->config.regulation_minutes);
    }
    if (state->clock_minutes > maximum_minutes) {
        return false;
    }

    expected_pose_available = gameplay_close_shot_observed_pose(
        state->close_shot_subtype01.observation,
        state->close_shot_subtype01.phase,
        &expected_actor_pose,
        &expected_ball_pose);
    if (state->close_shot_subtype01.observed_pose_available !=
            expected_pose_available ||
        state->close_shot_subtype01.observed_actor_pose_index !=
            expected_actor_pose ||
        state->close_shot_subtype01.observed_ball_pose_index !=
            expected_ball_pose ||
        state->close_shot_subtype01.active !=
            (state->close_shot_subtype01.phase !=
             TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL)) {
        return false;
    }

    for (size_t team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT; ++team) {
        if (state->team_fouls[team] > 5U) {
            return false;
        }
        for (size_t player = 0U; player < TECMO_GAMEPLAY_PLAYER_COUNT;
             ++player) {
            if (state->individual_fouls[team][player] > 6U) {
                return false;
            }
        }
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT) {
        if (state->expiry_wait_frames_remaining < 1U ||
            state->expiry_wait_frames_remaining >
                TECMO_GAMEPLAY_PERIOD_EXPIRY_WAIT_FRAMES ||
            state->clock_minutes != 0U || state->clock_seconds != 0U ||
            state->clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES) {
            return false;
        }
    } else if (state->expiry_wait_frames_remaining != 0U) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        if (!state->period_expiry_zero_action_observed ||
            state->clock_minutes != 0U || state->clock_seconds != 0U ||
            state->clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES) {
            return false;
        }
    } else if (state->period_expiry_zero_action_observed) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER) {
        prepared_minutes =
            state->banner == TECMO_GAMEPLAY_BANNER_OVERTIME
                ? tecmo_gameplay_overtime_minutes(
                      state->config.regulation_minutes)
                : state->config.regulation_minutes;
        if (!gameplay_period_banner_matches_period(state) ||
            state->phase_frame >= TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES ||
            !gameplay_clock_is_prepared(state, prepared_minutes)) {
            return false;
        }
    } else if (state->phase == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER) {
        if (state->banner != TECMO_GAMEPLAY_BANNER_HALFTIME ||
            state->period != 3U || state->overtime_count != 0U ||
            state->phase_frame >= TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES ||
            !gameplay_clock_is_prepared(
                state, state->config.regulation_minutes)) {
            return false;
        }
    } else if (state->banner != TECMO_GAMEPLAY_BANNER_NONE) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN) {
        if (state->period != 3U || state->overtime_count != 0U ||
            !gameplay_clock_is_prepared(
                state, state->config.regulation_minutes)) {
            return false;
        }
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        state->phase == TECMO_GAMEPLAY_PHASE_COMPLETE) {
        prepared_minutes =
            state->overtime_count == 0U
                ? state->config.regulation_minutes
                : tecmo_gameplay_overtime_minutes(
                      state->config.regulation_minutes);
        if (state->period != 5U ||
            state->score[TECMO_GAMEPLAY_TEAM_AWAY] ==
                state->score[TECMO_GAMEPLAY_TEAM_HOME] ||
            !gameplay_clock_is_prepared(state, prepared_minutes)) {
            return false;
        }
    }

    switch (state->phase) {
    case TECMO_GAMEPLAY_PHASE_LIVE:
    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT:
    case TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED:
    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED:
    case TECMO_GAMEPLAY_PHASE_COMPLETE:
        if (state->phase_frame != 0U) {
            return false;
        }
        break;
    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE:
    case TECMO_GAMEPLAY_PHASE_PERIOD_BANNER:
    case TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER:
    case TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN:
    case TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN:
    case TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION:
    case TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION:
    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE:
        break;
    case TECMO_GAMEPLAY_PHASE_COUNT:
    default:
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        if (!gameplay_violation_valid(state->violation) ||
            state->phase_frame >=
                TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES) {
            return false;
        }
    } else if (state->violation != TECMO_GAMEPLAY_VIOLATION_NONE) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION &&
        state->phase_frame >= TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE) {
        if (state->free_throws.attempts_remaining == 0U ||
            state->shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
            (state->clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES &&
             state->clock_divider !=
                 TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES)) {
            return false;
        }
    } else if (state->phase ==
               TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED) {
        if (state->free_throws.attempts_remaining != 0U ||
            state->shot_clock != TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS ||
            (state->clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES &&
             state->clock_divider !=
                 TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES)) {
            return false;
        }
    } else if (state->phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION &&
               state->phase !=
                   TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED &&
               state->free_throws.attempts_remaining != 0U) {
        return false;
    }

    return true;
}

static bool gameplay_update_impl(TecmoGameplayState *state,
                                 const TecmoGameplayFrameInput *input,
                                 const TecmoGameplayLiveContext *live_context,
                                 TecmoGameplayEventBuffer *events)
{
    const bool a_released = gameplay_input_either_nes_a_released(input);

    if (events == NULL) {
        return false;
    }
    tecmo_gameplay_events_clear(events);
    if (input == NULL || !gameplay_live_context_valid(live_context) ||
        !tecmo_gameplay_state_valid(state)) {
        return false;
    }

    switch (state->phase) {
    case TECMO_GAMEPLAY_PHASE_LIVE:
        if (!gameplay_update_live_clock(state, live_context, events)) {
            return false;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE:
        if (state->period_expiry_zero_action_observed &&
            live_context->period_expiry ==
                TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED) {
            if (!gameplay_finish_period(state, events)) {
                return false;
            }
        } else if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT:
        --state->expiry_wait_frames_remaining;
        if (state->expiry_wait_frames_remaining == 0U) {
            if (!gameplay_finish_period(state, events)) {
                return false;
            }
        }
        break;

    case TECMO_GAMEPLAY_PHASE_PERIOD_BANNER:
        ++state->phase_frame;
        if (state->phase_frame >= TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES) {
            /* E6ED/E6FF clears precede the E765 reset after all 60 frames. */
            gameplay_finish_period_banner_reset(state);
            state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
            state->phase_frame = 0U;
            state->banner = TECMO_GAMEPLAY_BANNER_NONE;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER:
        ++state->phase_frame;
        if (state->phase_frame >= TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES) {
            if (!gameplay_event_append(events,
                                       TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                       TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID,
                                       0U)) {
                return false;
            }
            state->phase = TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN;
            state->phase_frame = 0U;
            state->banner = TECMO_GAMEPLAY_BANNER_NONE;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN:
        if (a_released) {
            if (!gameplay_start_period_banner(
                    state, TECMO_GAMEPLAY_BANNER_THIRD_PERIOD)) {
                return false;
            }
        } else if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN:
        if (a_released) {
            if (!gameplay_event_append(events,
                                       TECMO_GAMEPLAY_EVENT_GAME_COMPLETE,
                                       0U,
                                       0U)) {
                return false;
            }
            state->phase = TECMO_GAMEPLAY_PHASE_COMPLETE;
            state->phase_frame = 0U;
        } else if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION:
        ++state->phase_frame;
        if ((state->phase_frame >
                 TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES &&
             a_released) ||
            state->phase_frame >=
                TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES) {
            const TecmoGameplayTeam restart = state->restart_possession;
            state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
            state->phase_frame = 0U;
            state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;
            if (!tecmo_gameplay_reset_possession(state, restart) ||
                !gameplay_event_append(
                    events,
                    TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST,
                    TECMO_GAMEPLAY_RESTART_PLAY_ID,
                    (uint16_t)restart)) {
                return false;
            }
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION:
        ++state->phase_frame;
        if ((state->phase_frame >
                 TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES &&
             a_released) ||
            state->phase_frame >= TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES) {
            state->phase_frame = 0U;
            state->phase = TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED:
        break;

    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE:
        /* Aim, release timing, rebound, and ball motion remain evidence-gated. */
        if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED:
        break;

    case TECMO_GAMEPLAY_PHASE_COMPLETE:
        break;

    case TECMO_GAMEPLAY_PHASE_COUNT:
    default:
        return false;
    }

    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_update(TecmoGameplayState *state,
                           const TecmoGameplayFrameInput *input,
                           const TecmoGameplayLiveContext *live_context,
                           TecmoGameplayEventBuffer *events)
{
    TecmoGameplayState before;

    if (events == NULL) {
        return false;
    }
    tecmo_gameplay_events_clear(events);
    if (state == NULL) {
        return false;
    }

    before = *state;
    if (!gameplay_update_impl(state, input, live_context, events)) {
        *state = before;
        tecmo_gameplay_events_clear(events);
        return false;
    }
    return true;
}

bool tecmo_gameplay_reset_possession(TecmoGameplayState *state,
                                     TecmoGameplayTeam possession)
{
    TecmoGameplayState before;

    if (!tecmo_gameplay_state_valid(state) ||
        !gameplay_team_valid(possession)) {
        return false;
    }

    before = *state;
    state->possession = possession;
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    state->clock_divider = TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES;
    state->period_expiry_zero_action_observed = false;
    if (!tecmo_gameplay_state_valid(state)) {
        *state = before;
        return false;
    }
    return true;
}

bool tecmo_gameplay_set_score(TecmoGameplayState *state,
                              TecmoGameplayTeam team,
                              uint16_t score)
{
    uint16_t before;

    if (!tecmo_gameplay_state_valid(state) || !gameplay_team_valid(team)) {
        return false;
    }
    before = state->score[(size_t)team];
    state->score[(size_t)team] = score;
    if (!tecmo_gameplay_state_valid(state)) {
        state->score[(size_t)team] = before;
        return false;
    }
    return true;
}

bool tecmo_gameplay_award_points(TecmoGameplayState *state,
                                 TecmoGameplayTeam team,
                                 uint8_t points)
{
    uint16_t current;

    if (!tecmo_gameplay_state_valid(state) || !gameplay_team_valid(team) ||
        points < 1U || points > 3U) {
        return false;
    }

    current = state->score[(size_t)team];
    if ((uint32_t)current + (uint32_t)points > UINT16_MAX) {
        return false;
    }
    state->score[(size_t)team] = (uint16_t)(current + points);
    if (!tecmo_gameplay_state_valid(state)) {
        state->score[(size_t)team] = current;
        return false;
    }
    return true;
}

bool tecmo_gameplay_request_violation(TecmoGameplayState *state,
                                      TecmoGameplayViolation violation,
                                      TecmoGameplayTeam restart_possession)
{
    if (!tecmo_gameplay_state_valid(state) ||
        state->phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !gameplay_violation_valid(violation) ||
        !gameplay_team_valid(restart_possession)) {
        return false;
    }
    return gameplay_enter_violation(state, violation, restart_possession) &&
           tecmo_gameplay_state_valid(state);
}

static bool gameplay_foul_counter_effect_valid(
    TecmoGameplayFoulCounterEffect effect)
{
    return effect >= TECMO_GAMEPLAY_FOUL_COUNTER_NONE &&
           effect < TECMO_GAMEPLAY_FOUL_COUNTER_EFFECT_COUNT;
}

static bool gameplay_post_foul_clock_path_valid(
    TecmoGameplayPostFoulClockPath clock_path)
{
    return clock_path >= TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45 &&
           clock_path < TECMO_GAMEPLAY_POST_FOUL_CLOCK_PATH_COUNT;
}

static void gameplay_apply_post_foul_clock(
    TecmoGameplayState *state,
    TecmoGameplayTeam next_possession,
    TecmoGameplayPostFoulClockPath clock_path)
{
    state->possession = next_possession;
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    state->clock_divider =
        clock_path == TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45
            ? TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES
            : TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES;
    state->period_expiry_zero_action_observed = false;
}

bool tecmo_gameplay_request_foul(TecmoGameplayState *state,
                                 const TecmoGameplayFoulRequest *request)
{
    uint8_t *individual;
    uint8_t *team;

    if (request == NULL || !tecmo_gameplay_state_valid(state) ||
        state->phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !gameplay_team_valid(request->fouling_team) ||
        !gameplay_team_valid(request->free_throw_team) ||
        request->fouling_team == request->free_throw_team ||
        !gameplay_foul_counter_effect_valid(request->counter_effect) ||
        request->player_index >= TECMO_GAMEPLAY_PLAYER_COUNT ||
        request->free_throw_attempts > 3U) {
        return false;
    }

    individual = &state->individual_fouls[(size_t)request->fouling_team]
                                         [request->player_index];
    team = &state->team_fouls[(size_t)request->fouling_team];
    if ((request->counter_effect &
         TECMO_GAMEPLAY_FOUL_COUNTER_INDIVIDUAL) != 0 &&
        *individual < 6U) {
        ++*individual;
    }
    if ((request->counter_effect & TECMO_GAMEPLAY_FOUL_COUNTER_TEAM) != 0 &&
        *team < 5U) {
        ++*team;
    }

    state->free_throws.scoring_team = request->free_throw_team;
    state->free_throws.attempts_remaining = request->free_throw_attempts;
    state->phase = TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION;
    state->phase_frame = 0U;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_settle_foul_presentation(
    TecmoGameplayState *state,
    TecmoGameplayTeam next_possession,
    TecmoGameplayPostFoulClockPath clock_path)
{
    if (!tecmo_gameplay_state_valid(state) ||
        state->phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
        !gameplay_team_valid(next_possession) ||
        !gameplay_post_foul_clock_path_valid(clock_path)) {
        return false;
    }

    gameplay_apply_post_foul_clock(state, next_possession, clock_path);
    state->phase = state->free_throws.attempts_remaining > 0U
                       ? TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE
                       : TECMO_GAMEPLAY_PHASE_LIVE;
    state->phase_frame = 0U;
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_individual_fouled_out(const TecmoGameplayState *state,
                                          TecmoGameplayTeam team,
                                          uint8_t player_index)
{
    return tecmo_gameplay_state_valid(state) && gameplay_team_valid(team) &&
           player_index < TECMO_GAMEPLAY_PLAYER_COUNT &&
           state->individual_fouls[(size_t)team][player_index] >= 6U;
}

uint8_t tecmo_gameplay_bonus_threshold(const TecmoGameplayState *state)
{
    if (!tecmo_gameplay_state_valid(state)) {
        return 0U;
    }
    return state->period == 5U ? 4U : 5U;
}

bool tecmo_gameplay_team_in_bonus(const TecmoGameplayState *state,
                                  TecmoGameplayTeam team)
{
    const uint8_t threshold = tecmo_gameplay_bonus_threshold(state);
    return threshold != 0U && gameplay_team_valid(team) &&
           state->team_fouls[(size_t)team] >= threshold;
}

bool tecmo_gameplay_record_free_throw_result(
    TecmoGameplayState *state,
    bool made,
    TecmoGameplayEventBuffer *events)
{
    uint8_t remaining;

    if (!tecmo_gameplay_state_valid(state) || events == NULL ||
        events->count >= TECMO_GAMEPLAY_EVENT_CAPACITY ||
        state->phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        state->free_throws.attempts_remaining == 0U) {
        return false;
    }

    if (made && !tecmo_gameplay_award_points(
                    state, state->free_throws.scoring_team, 1U)) {
        return false;
    }

    --state->free_throws.attempts_remaining;
    remaining = state->free_throws.attempts_remaining;
    state->phase_frame = 0U;
    if (!gameplay_event_append(events,
                               TECMO_GAMEPLAY_EVENT_FREE_THROW_RESULT,
                               made ? 1U : 0U,
                               remaining)) {
        return false;
    }

    if (remaining == 0U) {
        /* Rebound/inbound possession is explicitly unresolved at this boundary. */
        state->phase =
            TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED;
    }
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_settle_free_throws(
    TecmoGameplayState *state,
    TecmoGameplayTeam next_possession,
    TecmoGameplayPostFoulClockPath clock_path)
{
    if (!tecmo_gameplay_state_valid(state) ||
        state->phase !=
            TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED ||
        !gameplay_team_valid(next_possession) ||
        !gameplay_post_foul_clock_path_valid(clock_path)) {
        return false;
    }

    gameplay_apply_post_foul_clock(state, next_possession, clock_path);
    state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
    state->phase_frame = 0U;
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_nes_b_begin_close_shot_subtype01(
    TecmoGameplayState *state,
    const TecmoGameplayFrameInput *input,
    size_t controller,
    TecmoGameplayCloseShotObservation observation,
    TecmoGameplayEventBuffer *events)
{
    if (!tecmo_gameplay_state_valid(state) || events == NULL ||
        events->count >= TECMO_GAMEPLAY_EVENT_CAPACITY ||
        state->phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state->close_shot_subtype01.active ||
        !gameplay_close_shot_observation_valid(observation) ||
        !tecmo_gameplay_input_nes_b_jump_steal_shot_held(input, controller)) {
        return false;
    }

    state->close_shot_subtype01.observation = observation;
    gameplay_close_shot_set_phase(
        &state->close_shot_subtype01,
        TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY);
    if (!gameplay_event_append(
            events,
            TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
            (uint16_t)state->close_shot_subtype01.phase,
            state->close_shot_subtype01.observed_actor_pose_index)) {
        return false;
    }
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_advance_close_shot_subtype01(
    TecmoGameplayState *state,
    TecmoGameplayEventBuffer *events)
{
    TecmoGameplayCloseShotPhase next;

    if (!tecmo_gameplay_state_valid(state) || events == NULL ||
        events->count >= TECMO_GAMEPLAY_EVENT_CAPACITY ||
        !state->close_shot_subtype01.active) {
        return false;
    }

    switch (state->close_shot_subtype01.phase) {
    case TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_A;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_A:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_B;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_B:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL:
    case TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT:
    default:
        return false;
    }

    gameplay_close_shot_set_phase(&state->close_shot_subtype01, next);
    if (!gameplay_event_append(
            events,
            TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
            (uint16_t)state->close_shot_subtype01.phase,
            state->close_shot_subtype01.observed_actor_pose_index)) {
        return false;
    }
    return tecmo_gameplay_state_valid(state);
}

const char *tecmo_gameplay_phase_name(TecmoGameplayPhase phase)
{
    switch (phase) {
    case TECMO_GAMEPLAY_PHASE_LIVE: return "live";
    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE:
        return "period-expiry-live-settle";
    case TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT:
        return "period-expiry-fixed-wait";
    case TECMO_GAMEPLAY_PHASE_PERIOD_BANNER: return "period-banner";
    case TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER: return "halftime-banner";
    case TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN:
        return "halftime-score-screen";
    case TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN: return "final-score-screen";
    case TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION:
        return "violation-presentation";
    case TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION: return "foul-presentation";
    case TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED:
        return "foul-settlement-required";
    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE: return "free-throw-sequence";
    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED:
        return "free-throw-settlement-required";
    case TECMO_GAMEPLAY_PHASE_COMPLETE: return "complete";
    case TECMO_GAMEPLAY_PHASE_COUNT:
    default:
        return "invalid";
    }
}

const char *tecmo_gameplay_violation_name(TecmoGameplayViolation violation)
{
    switch (violation) {
    case TECMO_GAMEPLAY_VIOLATION_NONE: return "NONE";
    case TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS: return "OUT OF BOUNDS";
    case TECMO_GAMEPLAY_VIOLATION_BACKCOURT: return "BACKCOURT";
    case TECMO_GAMEPLAY_VIOLATION_FIVE_SECONDS:
        return "5 SECOND VIOLATION";
    case TECMO_GAMEPLAY_VIOLATION_TEN_SECONDS:
        return "10 SECOND VIOLATION";
    case TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK:
        return "SHOT CLOCK VIOLATION";
    case TECMO_GAMEPLAY_VIOLATION_TRAVELING: return "TRAVELING";
    case TECMO_GAMEPLAY_VIOLATION_GOALTENDING: return "GOALTENDING";
    default: return "INVALID";
    }
}

static uint64_t gameplay_hash_byte(uint64_t hash, uint8_t value)
{
    hash ^= value;
    return hash * UINT64_C(1099511628211);
}

static uint64_t gameplay_hash_u16(uint64_t hash, uint16_t value)
{
    hash = gameplay_hash_byte(hash, (uint8_t)(value & 0xFFU));
    return gameplay_hash_byte(hash, (uint8_t)(value >> 8U));
}

static uint64_t gameplay_hash_u32(uint64_t hash, uint32_t value)
{
    hash = gameplay_hash_u16(hash, (uint16_t)(value & 0xFFFFU));
    return gameplay_hash_u16(hash, (uint16_t)(value >> 16U));
}

uint64_t tecmo_gameplay_state_hash(const TecmoGameplayState *state)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    if (state == NULL) {
        return 0U;
    }

    hash = gameplay_hash_byte(hash, state->initialized ? 1U : 0U);
    hash = gameplay_hash_byte(hash, state->config.regulation_minutes);
    hash = gameplay_hash_byte(hash, (uint8_t)state->phase);
    hash = gameplay_hash_byte(hash, (uint8_t)state->banner);
    hash = gameplay_hash_byte(hash, (uint8_t)state->violation);
    hash = gameplay_hash_byte(hash, (uint8_t)state->possession);
    hash = gameplay_hash_byte(hash, (uint8_t)state->restart_possession);
    hash = gameplay_hash_byte(hash, state->period);
    hash = gameplay_hash_byte(hash, state->overtime_count);
    hash = gameplay_hash_byte(hash, state->clock_minutes);
    hash = gameplay_hash_byte(hash, state->clock_seconds);
    hash = gameplay_hash_byte(hash, state->clock_divider);
    hash = gameplay_hash_byte(hash, state->shot_clock);
    hash = gameplay_hash_u16(hash, state->phase_frame);
    hash = gameplay_hash_byte(hash, state->expiry_wait_frames_remaining);
    hash = gameplay_hash_byte(
        hash, state->period_expiry_zero_action_observed ? 1U : 0U);
    hash = gameplay_hash_u16(hash, state->score[0]);
    hash = gameplay_hash_u16(hash, state->score[1]);

    for (size_t team = 0U; team < TECMO_GAMEPLAY_TEAM_COUNT; ++team) {
        hash = gameplay_hash_byte(hash, state->team_fouls[team]);
        for (size_t player = 0U; player < TECMO_GAMEPLAY_PLAYER_COUNT;
             ++player) {
            hash = gameplay_hash_byte(
                hash, state->individual_fouls[team][player]);
        }
    }

    hash = gameplay_hash_byte(hash, (uint8_t)state->free_throws.scoring_team);
    hash = gameplay_hash_byte(hash, state->free_throws.attempts_remaining);
    hash = gameplay_hash_byte(
        hash, (uint8_t)state->close_shot_subtype01.phase);
    hash = gameplay_hash_byte(
        hash, (uint8_t)state->close_shot_subtype01.observation);
    hash = gameplay_hash_u16(
        hash, state->close_shot_subtype01.observed_actor_pose_index);
    hash = gameplay_hash_u16(
        hash, state->close_shot_subtype01.observed_ball_pose_index);
    hash = gameplay_hash_u32(
        hash, state->close_shot_subtype01.transition_serial);
    hash = gameplay_hash_byte(
        hash, state->close_shot_subtype01.observed_pose_available ? 1U : 0U);
    hash = gameplay_hash_byte(
        hash, state->close_shot_subtype01.active ? 1U : 0U);
    return hash;
}

static void gameplay_self_test_message(char *message,
                                       size_t message_size,
                                       const char *text)
{
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size, "%s", text != NULL ? text : "");
    }
}

static bool gameplay_events_contain(const TecmoGameplayEventBuffer *events,
                                    TecmoGameplayEventKind kind,
                                    uint16_t value)
{
    if (events == NULL) {
        return false;
    }
    for (size_t index = 0U; index < events->count; ++index) {
        if (events->events[index].kind == kind &&
            events->events[index].value == value) {
            return true;
        }
    }
    return false;
}

static bool gameplay_self_test_run_frames(
    TecmoGameplayState *state,
    size_t frame_count,
    const TecmoGameplayFrameInput *input,
    const TecmoGameplayLiveContext *context,
    TecmoGameplayEventBuffer *events)
{
    for (size_t frame = 0U; frame < frame_count; ++frame) {
        if (!tecmo_gameplay_update(state, input, context, events)) {
            return false;
        }
    }
    return true;
}

static bool gameplay_self_test_expire_after_allowed_action(
    TecmoGameplayState *state,
    const TecmoGameplayFrameInput *input,
    TecmoGameplayEventBuffer *events)
{
    TecmoGameplayLiveContext context;

    state->clock_minutes = 0U;
    state->clock_seconds = 1U;
    state->clock_divider = 1U;
    state->shot_clock = 10U;
    tecmo_gameplay_live_context_default(&context);
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
    if (!tecmo_gameplay_update(state, input, &context, events) ||
        state->phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !state->period_expiry_zero_action_observed ||
        state->clock_minutes != 0U || state->clock_seconds != 0U ||
        state->clock_divider != TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES) {
        return false;
    }
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    return tecmo_gameplay_update(state, input, &context, events);
}

static bool gameplay_self_test_release_after_lead_in(
    TecmoGameplayState *state,
    TecmoGameplayLiveContext *context,
    TecmoGameplayEventBuffer *events)
{
    TecmoGameplayFrameInput input;

    tecmo_gameplay_frame_input_clear(&input);
    if (!gameplay_self_test_run_frames(
            state, TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES,
            &input, context, events)) {
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[1].released.nes_a_pass_switch = true;
    return tecmo_gameplay_update(state, &input, context, events);
}

static bool gameplay_self_test_rejects_state(
    const TecmoGameplayState *state,
    char *message,
    size_t message_size,
    const char *failure)
{
    if (tecmo_gameplay_state_valid(state)) {
        gameplay_self_test_message(message, message_size, failure);
        return false;
    }
    return true;
}

static bool gameplay_self_test_config_and_input(char *message,
                                                size_t message_size)
{
    static const uint8_t valid_minutes[] = {2U, 3U, 4U, 8U, 12U};
    static const uint8_t overtime_minutes[] = {1U, 1U, 2U, 3U, 5U};
    static const TecmoGameplayCloseShotPhase expected_phases[] = {
        TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_A,
        TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_B,
        TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH,
        TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE,
        TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY,
        TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL
    };
    static const uint16_t expected_advance_poses[] = {
        254U, 255U, 257U, 258U, 259U, 509U
    };
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayEventBuffer events;

    for (size_t index = 0U;
         index < sizeof(valid_minutes) / sizeof(valid_minutes[0]);
         ++index) {
        if (!tecmo_gameplay_config_init(&config, valid_minutes[index]) ||
            tecmo_gameplay_overtime_minutes(valid_minutes[index]) !=
                overtime_minutes[index]) {
            gameplay_self_test_message(message, message_size,
                                       "VALID PERIOD OPTION REJECTED");
            return false;
        }
    }
    if (tecmo_gameplay_config_init(&config, 0U) ||
        tecmo_gameplay_config_init(&config, 1U) ||
        tecmo_gameplay_config_init(&config, 5U) ||
        tecmo_gameplay_config_init(&config, 13U) ||
        tecmo_gameplay_overtime_minutes(5U) != 0U) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID PERIOD OPTION ACCEPTED");
        return false;
    }

    if (!tecmo_gameplay_config_init(&config, 4U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        tecmo_gameplay_state_init(
            &state, &config, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT STATE INIT FAILED");
        return false;
    }
    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "VALID STATE REINIT FAILED");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_events_clear(&events);
    input.controllers[0].held.nes_start = true;
    if (!tecmo_gameplay_input_nes_start_held(&input, 0U) ||
        tecmo_gameplay_input_nes_a_pass_switch_held(&input, 0U) ||
        tecmo_gameplay_input_nes_b_jump_steal_shot_held(&input, 0U) ||
        tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U,
            TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "START WAS REVERSED INTO SHOT INPUT");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].held.nes_a_pass_switch = true;
    if (!tecmo_gameplay_input_nes_a_pass_switch_held(&input, 0U) ||
        tecmo_gameplay_input_nes_b_jump_steal_shot_held(&input, 0U) ||
        tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U,
            TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "NES A PASS/SWITCH MAPPING FAILED");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].held.nes_b_jump_steal_shot = true;
    if (!tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U,
            TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY, &events) ||
        state.close_shot_subtype01.phase != TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY ||
        state.close_shot_subtype01.observed_pose_available ||
        state.close_shot_subtype01.observed_actor_pose_index != UINT16_MAX ||
        state.close_shot_subtype01.observed_ball_pose_index != UINT16_MAX ||
        !gameplay_events_contain(
            &events, TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
            TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY)) {
        gameplay_self_test_message(message, message_size,
                                   "SEMANTIC CLOSE SHOT ENTRY FAILED");
        return false;
    }

    for (size_t index = 0U;
         index < sizeof(expected_phases) / sizeof(expected_phases[0]);
         ++index) {
        if (!tecmo_gameplay_advance_close_shot_subtype01(&state, &events) ||
            state.close_shot_subtype01.phase != expected_phases[index] ||
            state.close_shot_subtype01.observed_pose_available) {
            gameplay_self_test_message(message, message_size,
                                       "SEMANTIC CLOSE SHOT SEQUENCE FAILED");
            return false;
        }
    }
    if (state.close_shot_subtype01.active ||
        state.close_shot_subtype01.phase !=
            TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL ||
        tecmo_gameplay_advance_close_shot_subtype01(&state, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "NUMERIC CLOSE SHOT RECOVERY FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "OBSERVED SHOT TRACE INIT FAILED");
        return false;
    }
    tecmo_gameplay_events_clear(&events);
    if (tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U,
            TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVATION_COUNT, &events) ||
        state.close_shot_subtype01.active || events.count != 0U ||
        !tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U,
            TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVED_RIGHTWARD_TRACE, &events) ||
        !state.close_shot_subtype01.observed_pose_available ||
        state.close_shot_subtype01.observed_actor_pose_index != 445U ||
        state.close_shot_subtype01.observed_ball_pose_index != 64U) {
        gameplay_self_test_message(message, message_size,
                                   "BOUNDED SHOT TRACE ENTRY FAILED");
        return false;
    }
    for (size_t index = 0U;
         index < sizeof(expected_advance_poses) /
                     sizeof(expected_advance_poses[0]);
         ++index) {
        if (!tecmo_gameplay_advance_close_shot_subtype01(&state, &events) ||
            !state.close_shot_subtype01.observed_pose_available ||
            state.close_shot_subtype01.observed_actor_pose_index !=
                expected_advance_poses[index] ||
            state.close_shot_subtype01.observed_ball_pose_index != 64U) {
            gameplay_self_test_message(message, message_size,
                                       "BOUNDED SHOT TRACE SEQUENCE FAILED");
            return false;
        }
    }

    state.clock_seconds = 60U;
    if (tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "MALFORMED STATE ACCEPTED");
        return false;
    }
    return true;
}

static bool gameplay_self_test_clock_and_periods(char *message,
                                                 size_t message_size)
{
    static const uint8_t valid_minutes[] = {2U, 3U, 4U, 8U, 12U};
    static const uint8_t overtime_minutes[] = {1U, 1U, 2U, 3U, 5U};
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 2U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !gameplay_self_test_run_frames(&state, 44U, &input, &context,
                                       &events) ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.shot_clock != 24U || state.clock_divider != 1U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.clock_minutes != 1U || state.clock_seconds != 59U ||
        state.shot_clock != 23U || state.clock_divider != 45U) {
        gameplay_self_test_message(message, message_size,
                                   "45-FRAME CLOCK ORDER FAILED");
        return false;
    }

    if (!tecmo_gameplay_reset_possession(&state,
                                         TECMO_GAMEPLAY_TEAM_HOME) ||
        state.clock_divider != 50U || state.shot_clock != 24U ||
        !gameplay_self_test_run_frames(&state, 49U, &input, &context,
                                       &events) ||
        state.clock_seconds != 59U || state.shot_clock != 24U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.clock_seconds != 58U || state.shot_clock != 23U) {
        gameplay_self_test_message(message, message_size,
                                   "50-FRAME POSSESSION RESET FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "LATE CLOCK INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 12U;
    state.clock_divider = 1U;
    state.shot_clock = 12U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.clock_seconds != 11U ||
        gameplay_events_contain(&events, TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID) ||
        !gameplay_self_test_run_frames(&state, 44U, &input, &context,
                                       &events) ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.clock_seconds != 10U ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                 TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID)) {
        gameplay_self_test_message(message, message_size,
                                   "LATE CLOCK EVENT BOUNDARY FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "SHOT CLOCK INIT FAILED");
        return false;
    }
    state.clock_minutes = 1U;
    state.clock_seconds = 30U;
    state.clock_divider = 1U;
    state.shot_clock = 1U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        state.violation != TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK ||
        state.clock_seconds != 29U ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_SFX_REQUEST,
                                 TECMO_GAMEPLAY_SFX_EXPIRY_ID) ||
        !gameplay_events_contain(
            &events, TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED,
            TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK)) {
        gameplay_self_test_message(message, message_size,
                                   "AUTOMATIC SHOT CLOCK VIOLATION FAILED");
        return false;
    }
    input.controllers[1].held.nes_start = true;
    if (!gameplay_self_test_run_frames(&state, 4U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        state.phase_frame != 4U || state.clock_divider != 45U) {
        gameplay_self_test_message(message, message_size,
                                   "VIOLATION LEAD-IN/FROZEN CLOCK FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[1].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        state.shot_clock != 24U || state.clock_divider != 50U ||
        !gameplay_events_contain(
            &events, TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST,
            TECMO_GAMEPLAY_RESTART_PLAY_ID)) {
        gameplay_self_test_message(message, message_size,
                                   "SHOT CLOCK RESTART FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "SHOT EXEMPT INIT FAILED");
        return false;
    }
    state.clock_minutes = 1U;
    state.clock_seconds = 30U;
    state.clock_divider = 1U;
    state.shot_clock = 1U;
    context.shot_clock_violation_exempt = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE || state.shot_clock != 0U ||
        !gameplay_events_contain(
            &events, TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED,
            TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK)) {
        gameplay_self_test_message(message, message_size,
                                   "SHOT CLOCK LIVE-STATE EXEMPTION FAILED");
        return false;
    }
    context.shot_clock_violation_exempt = false;

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FIXED EXPIRY INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    state.team_fouls[0] = 4U;
    state.team_fouls[1] = 3U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        state.expiry_wait_frames_remaining != 31U ||
        !gameplay_self_test_run_frames(&state, 30U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        state.expiry_wait_frames_remaining != 1U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.period != 2U ||
        state.banner != TECMO_GAMEPLAY_BANNER_SECOND_PERIOD ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        state.team_fouls[0] != 4U || state.team_fouls[1] != 3U ||
        !gameplay_self_test_run_frames(&state, 59U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        state.team_fouls[0] != 4U || state.team_fouls[1] != 3U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.clock_divider != 50U || state.shot_clock != 24U ||
        state.team_fouls[0] != 0U || state.team_fouls[1] != 0U) {
        gameplay_self_test_message(message, message_size,
                                   "31/60 FRAME PERIOD TRANSITION FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "PRE-EXPIRY ACTION INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 2U;
    state.shot_clock = 10U;
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.period_expiry_zero_action_observed ||
        state.clock_divider != 1U) {
        gameplay_self_test_message(message, message_size,
                                   "PRE-EXPIRY ACTION WAS LATCHED");
        return false;
    }
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        state.expiry_wait_frames_remaining != 31U) {
        gameplay_self_test_message(message, message_size,
                                   "PRE-EXPIRY HISTORY BYPASSED FIXED31");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "INITIAL SETTLED INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        state.expiry_wait_frames_remaining != 31U ||
        !gameplay_self_test_run_frames(&state, 30U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        state.expiry_wait_frames_remaining != 1U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.period != 2U) {
        gameplay_self_test_message(message, message_size,
                                   "INITIAL SETTLED DID NOT TAKE FIXED31");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "LIVE SETTLE INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !gameplay_self_test_run_frames(&state, 20U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE) {
        gameplay_self_test_message(message, message_size,
                                   "ALLOWED LIVE SETTLEMENT WAIT FAILED");
        return false;
    }
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.period != 2U) {
        gameplay_self_test_message(message, message_size,
                                   "ALLOWED LIVE SETTLEMENT EXIT FAILED");
        return false;
    }

    for (size_t index = 0U;
         index < sizeof(valid_minutes) / sizeof(valid_minutes[0]);
         ++index) {
        if (!tecmo_gameplay_config_init(&config, valid_minutes[index]) ||
            !tecmo_gameplay_state_init(&state, &config,
                                       TECMO_GAMEPLAY_TEAM_AWAY)) {
            gameplay_self_test_message(message, message_size,
                                       "OVERTIME MATRIX INIT FAILED");
            return false;
        }
        state.period = 4U;
        if (!gameplay_self_test_expire_after_allowed_action(
                &state, &input, &events) ||
            state.period != 5U || state.overtime_count != 1U ||
            state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
            state.banner != TECMO_GAMEPLAY_BANNER_OVERTIME ||
            state.clock_minutes != overtime_minutes[index] ||
            state.clock_divider != 45U || state.shot_clock != 9U) {
            gameplay_self_test_message(message, message_size,
                                       "OVERTIME MINUTE MATRIX FAILED");
            return false;
        }
    }

    if (!tecmo_gameplay_config_init(&config, 2U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "MAX OVERTIME INIT FAILED");
        return false;
    }
    state.period = 5U;
    state.overtime_count = UINT8_MAX;
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !state.period_expiry_zero_action_observed) {
        gameplay_self_test_message(message, message_size,
                                   "MAX OVERTIME SETTLE ENTRY FAILED");
        return false;
    }
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    {
        const uint64_t before_hash = tecmo_gameplay_state_hash(&state);
        if (tecmo_gameplay_update(&state, &input, &context, &events) ||
            tecmo_gameplay_state_hash(&state) != before_hash ||
            events.count != 0U) {
            gameplay_self_test_message(message, message_size,
                                       "MAX OVERTIME FAILURE MUTATED STATE");
            return false;
        }
    }

    return true;
}

static bool gameplay_self_test_halftime_and_final(char *message,
                                                  size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 2U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME INIT FAILED");
        return false;
    }

    state.period = 2U;
    state.team_fouls[0] = 5U;
    state.team_fouls[1] = 4U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.period != 3U ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        state.banner != TECMO_GAMEPLAY_BANNER_HALFTIME ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        !gameplay_self_test_run_frames(&state, 119U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        state.team_fouls[0] != 5U || state.team_fouls[1] != 4U ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                 TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID)) {
        gameplay_self_test_message(message, message_size,
                                   "120-FRAME HALFTIME BANNER FAILED");
        return false;
    }

    input.controllers[0].held.dpad_down = true;
    input.controllers[0].held.nes_a_pass_switch = true;
    input.controllers[1].released.dpad_left = true;
    if (!gameplay_self_test_run_frames(&state, 500U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME SCORE SCREEN WAS BOUNDED");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[1].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        state.team_fouls[0] != 5U || state.team_fouls[1] != 4U) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME A-RELEASE HANDOFF FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    if (!gameplay_self_test_run_frames(&state, 59U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        state.team_fouls[0] != 5U || state.team_fouls[1] != 4U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.clock_divider != 50U || state.shot_clock != 24U ||
        state.team_fouls[0] != 0U || state.team_fouls[1] != 0U) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME INPUT/THIRD PERIOD HANDOFF FAILED");
        return false;
    }

    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.period != 4U ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.banner != TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD) {
        gameplay_self_test_message(message, message_size,
                                   "FOURTH PERIOD BANNER FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_set_score(&state, TECMO_GAMEPLAY_TEAM_AWAY, 1U)) {
        gameplay_self_test_message(message, message_size,
                                   "REGULATION FINAL INIT FAILED");
        return false;
    }
    state.period = 4U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.period != 5U || state.overtime_count != 0U ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                 TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID)) {
        gameplay_self_test_message(message, message_size,
                                   "REGULATION FINAL HANDOFF FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "OVERTIME REPEAT INIT FAILED");
        return false;
    }
    state.period = 4U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.period != 5U || state.overtime_count != 1U ||
        state.clock_minutes != 1U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        !gameplay_self_test_run_frames(&state, 60U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.clock_divider != 50U || state.shot_clock != 24U) {
        gameplay_self_test_message(message, message_size,
                                   "FIRST OVERTIME HANDOFF FAILED");
        return false;
    }

    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.period != 5U || state.overtime_count != 2U ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.clock_minutes != 1U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        !gameplay_self_test_run_frames(&state, 60U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !tecmo_gameplay_set_score(&state, TECMO_GAMEPLAY_TEAM_AWAY, 1U)) {
        gameplay_self_test_message(message, message_size,
                                   "TIED OVERTIME REPEAT FAILED");
        return false;
    }

    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        state.clock_minutes != 1U || state.clock_seconds != 0U ||
        state.clock_divider != 45U || state.shot_clock != 9U ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                 TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID) ||
        !gameplay_self_test_run_frames(&state, 500U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        state.clock_minutes != 1U || state.clock_seconds != 0U ||
        state.clock_divider != 45U) {
        gameplay_self_test_message(message, message_size,
                                   "NON-TIE FINAL PRESENTATION FAILED");
        return false;
    }

    input.controllers[0].held.nes_a_pass_switch = true;
    input.controllers[1].held.nes_start = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        gameplay_self_test_message(message, message_size,
                                   "HELD INPUT DISMISSED FINAL SCORE");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.dpad_up = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        gameplay_self_test_message(message, message_size,
                                   "DIRECTION RELEASE DISMISSED FINAL SCORE");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_COMPLETE ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_GAME_COMPLETE,
                                 0U)) {
        gameplay_self_test_message(message, message_size,
                                   "FINAL SCORE SCREEN EXIT FAILED");
        return false;
    }
    return true;
}

static bool gameplay_self_test_strict_state_validation(
    char *message,
    size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayState valid;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;
    TecmoGameplayFoulRequest foul;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 4U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT VALIDATION INIT FAILED");
        return false;
    }

    valid = state;
    state.phase_frame = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "LIVE NONZERO PHASE FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    state.period_expiry_zero_action_observed = true;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "LIVE EXPIRY LATCH ACCEPTED")) {
        return false;
    }

    state = valid;
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FIXED-WAIT SETUP FAILED");
        return false;
    }
    valid = state;
    state.phase_frame = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FIXED-WAIT NONZERO PHASE FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    state.expiry_wait_frames_remaining = 0U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FIXED-WAIT ZERO COUNTER ACCEPTED")) {
        return false;
    }
    state = valid;
    state.expiry_wait_frames_remaining =
        TECMO_GAMEPLAY_PERIOD_EXPIRY_WAIT_FRAMES + 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FIXED-WAIT HIGH COUNTER ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_seconds = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "NONZERO FIXED-WAIT CLOCK ACCEPTED")) {
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT SETTLE INIT FAILED");
        return false;
    }
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT LIVE-SETTLE SETUP FAILED");
        return false;
    }
    valid = state;
    state.period_expiry_zero_action_observed = false;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "LIVE-SETTLE WITHOUT ZERO ACTION ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_seconds = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "LIVE-SETTLE NONZERO CLOCK ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES - 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "LIVE-SETTLE NONENTRY DIVIDER ACCEPTED")) {
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT PERIOD-BANNER SETUP FAILED");
        return false;
    }
    valid = state;
    state.banner = (TecmoGameplayBanner)-1;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "NEGATIVE BANNER ENUM ACCEPTED")) {
        return false;
    }
    state = valid;
    state.banner = (TecmoGameplayBanner)6;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "OUT-OF-RANGE BANNER ENUM ACCEPTED")) {
        return false;
    }
    state = valid;
    state.banner = TECMO_GAMEPLAY_BANNER_THIRD_PERIOD;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "MISMATCHED BANNER/PERIOD ACCEPTED")) {
        return false;
    }
    state = valid;
    state.banner = TECMO_GAMEPLAY_BANNER_FIRST_PERIOD;
    state.period = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "UNREACHABLE FIRST-PERIOD BANNER ACCEPTED")) {
        return false;
    }
    state = valid;
    state.phase_frame = TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "PERIOD-BANNER ENDPOINT FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_minutes = 3U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "UNPREPARED PERIOD-BANNER CLOCK ACCEPTED")) {
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT HALFTIME INIT FAILED");
        return false;
    }
    state.period = 2U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT HALFTIME SETUP FAILED");
        return false;
    }
    valid = state;
    state.phase_frame = TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "HALFTIME ENDPOINT FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    state.period = 2U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "HALFTIME BANNER WRONG PERIOD ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_minutes = 3U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "UNPREPARED HALFTIME CLOCK ACCEPTED")) {
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_request_violation(
            &state, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
            TECMO_GAMEPLAY_TEAM_HOME)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT VIOLATION SETUP FAILED");
        return false;
    }
    state.phase_frame = TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "VIOLATION ENDPOINT FRAME ACCEPTED")) {
        return false;
    }

    foul.fouling_team = TECMO_GAMEPLAY_TEAM_AWAY;
    foul.free_throw_team = TECMO_GAMEPLAY_TEAM_HOME;
    foul.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_NONE;
    foul.player_index = 0U;
    foul.free_throw_attempts = 0U;
    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_request_foul(&state, &foul)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FOUL SETUP FAILED");
        return false;
    }
    valid = state;
    state.phase_frame = TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FOUL ENDPOINT FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    tecmo_gameplay_live_context_default(&context);
    if (!gameplay_self_test_run_frames(
            &state, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES,
            &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FOUL-SETTLEMENT SETUP FAILED");
        return false;
    }
    state.phase_frame = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FOUL SETTLEMENT NONZERO FRAME ACCEPTED")) {
        return false;
    }

    foul.free_throw_attempts = 1U;
    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_request_foul(&state, &foul) ||
        !gameplay_self_test_release_after_lead_in(
            &state, &context, &events) ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FREE-THROW SETUP FAILED");
        return false;
    }
    valid = state;
    state.shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS - 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FREE-THROW NONRESET SHOT CLOCK ACCEPTED")) {
        return false;
    }
    state = valid;
    state.clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES + 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FREE-THROW UNKNOWN DIVIDER ACCEPTED")) {
        return false;
    }
    state = valid;
    tecmo_gameplay_events_clear(&events);
    if (!tecmo_gameplay_record_free_throw_result(&state, false, &events) ||
        state.phase !=
            TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FT-SETTLEMENT SETUP FAILED");
        return false;
    }
    valid = state;
    state.phase_frame = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FT SETTLEMENT NONZERO FRAME ACCEPTED")) {
        return false;
    }
    state = valid;
    state.free_throws.attempts_remaining = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "FT SETTLEMENT REMAINING ATTEMPT ACCEPTED")) {
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_set_score(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 1U)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT FINAL INIT FAILED");
        return false;
    }
    state.period = 4U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        state.overtime_count != 0U || state.clock_minutes != 4U ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "REGULATION-PREPARED FINAL INVALID");
        return false;
    }
    valid = state;
    {
        const uint64_t before_hash = tecmo_gameplay_state_hash(&state);
        if (tecmo_gameplay_set_score(
                &state, TECMO_GAMEPLAY_TEAM_HOME,
                state.score[TECMO_GAMEPLAY_TEAM_AWAY]) ||
            tecmo_gameplay_state_hash(&state) != before_hash ||
            tecmo_gameplay_reset_possession(
                &state, TECMO_GAMEPLAY_TEAM_HOME) ||
            tecmo_gameplay_state_hash(&state) != before_hash) {
            gameplay_self_test_message(
                message, message_size,
                "FINAL MUTATOR FAILURE WAS NOT TRANSACTIONAL");
            return false;
        }
    }
    state.clock_minutes = tecmo_gameplay_overtime_minutes(
        state.config.regulation_minutes);
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "REGULATION FINAL ACCEPTED OVERTIME CLOCK")) {
        return false;
    }
    state = valid;
    state.score[TECMO_GAMEPLAY_TEAM_HOME] =
        state.score[TECMO_GAMEPLAY_TEAM_AWAY];
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "TIED FINAL SCREEN ACCEPTED")) {
        return false;
    }
    state = valid;
    state.phase = TECMO_GAMEPLAY_PHASE_LIVE;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "REGULATION PERIOD-FIVE LIVE STATE ACCEPTED")) {
        return false;
    }
    state = valid;
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_COMPLETE ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT COMPLETE SETUP FAILED");
        return false;
    }
    state.phase_frame = 1U;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "COMPLETE NONZERO PHASE FRAME ACCEPTED")) {
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT OVERTIME INIT FAILED");
        return false;
    }
    state.period = 4U;
    if (!gameplay_self_test_expire_after_allowed_action(
            &state, &input, &events) ||
        !gameplay_self_test_run_frames(
            &state, TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES,
            &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.period != 5U || state.overtime_count != 1U ||
        state.clock_minutes != 2U ||
        !tecmo_gameplay_state_valid(&state)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT OVERTIME LIVE SETUP FAILED");
        return false;
    }
    state.clock_minutes = state.config.regulation_minutes;
    if (!gameplay_self_test_rejects_state(
            &state, message, message_size,
            "OVERTIME LIVE ACCEPTED REGULATION CLOCK")) {
        return false;
    }

    return true;
}

static bool gameplay_self_test_violations(char *message,
                                          size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 4U)) {
        gameplay_self_test_message(message, message_size,
                                   "VIOLATION CONFIG FAILED");
        return false;
    }

    for (unsigned value = 1U; value <= 7U; ++value) {
        const TecmoGameplayViolation violation =
            (TecmoGameplayViolation)value;
        if (!tecmo_gameplay_state_init(&state, &config,
                                       TECMO_GAMEPLAY_TEAM_AWAY)) {
            gameplay_self_test_message(message, message_size,
                                       "VIOLATION ENUM INIT FAILED");
            return false;
        }
        state.clock_divider = 17U;
        state.shot_clock = 11U;
        if (!tecmo_gameplay_request_violation(
                &state, violation, TECMO_GAMEPLAY_TEAM_HOME) ||
            state.violation != violation ||
            strcmp(tecmo_gameplay_violation_name(violation), "INVALID") == 0 ||
            !gameplay_self_test_run_frames(&state, 123U, &input, &context,
                                           &events) ||
            state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            state.phase_frame != 123U ||
            state.clock_divider != 17U || state.shot_clock != 11U ||
            !tecmo_gameplay_update(&state, &input, &context, &events) ||
            state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
            state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
            state.shot_clock != 24U || state.clock_divider != 50U ||
            !gameplay_events_contain(
                &events, TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST,
                TECMO_GAMEPLAY_RESTART_PLAY_ID)) {
            gameplay_self_test_message(message, message_size,
                                       "VIOLATION ENUM/TIMING FAILED");
            return false;
        }
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        tecmo_gameplay_request_violation(
            &state, TECMO_GAMEPLAY_VIOLATION_NONE,
            TECMO_GAMEPLAY_TEAM_HOME) ||
        tecmo_gameplay_request_violation(
            &state, (TecmoGameplayViolation)8,
            TECMO_GAMEPLAY_TEAM_HOME) ||
        tecmo_gameplay_request_violation(
            &state, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
            (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT)) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID VIOLATION REQUEST ACCEPTED");
        return false;
    }

    if (!tecmo_gameplay_request_violation(
            &state, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
            TECMO_GAMEPLAY_TEAM_HOME)) {
        gameplay_self_test_message(message, message_size,
                                   "OUT OF BOUNDS TRIGGER FAILED");
        return false;
    }
    state.clock_divider = 19U;
    state.shot_clock = 8U;
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        state.phase_frame != 1U) {
        gameplay_self_test_message(message, message_size,
                                   "EARLY A RELEASE DISMISSED VIOLATION");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].held.nes_a_pass_switch = true;
    input.controllers[0].held.nes_start = true;
    input.controllers[0].held.dpad_right = true;
    input.controllers[1].released.dpad_left = true;
    if (!gameplay_self_test_run_frames(&state, 3U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        state.phase_frame != 4U || state.clock_divider != 19U ||
        state.shot_clock != 8U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        state.phase_frame != 5U) {
        gameplay_self_test_message(message, message_size,
                                   "NON-RELEASE INPUT DISMISSED VIOLATION");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[1].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.phase_frame != 0U ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        state.shot_clock != 24U || state.clock_divider != 50U) {
        gameplay_self_test_message(message, message_size,
                                   "SECOND CONTROLLER A RELEASE FAILED");
        return false;
    }
    return true;
}

static bool gameplay_self_test_fouls_and_free_throws(char *message,
                                                     size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayState before;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;
    TecmoGameplayFoulRequest request;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 4U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL TIMING INIT FAILED");
        return false;
    }
    state.clock_divider = 17U;
    state.shot_clock = 11U;
    request.fouling_team = TECMO_GAMEPLAY_TEAM_AWAY;
    request.free_throw_team = TECMO_GAMEPLAY_TEAM_HOME;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_NONE;
    request.player_index = 0U;
    request.free_throw_attempts = 0U;
    if (!tecmo_gameplay_request_foul(&state, &request) ||
        !gameplay_self_test_run_frames(&state, 163U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        state.phase_frame != 163U || state.clock_divider != 17U ||
        state.shot_clock != 11U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
        state.clock_divider != 17U || state.shot_clock != 11U) {
        gameplay_self_test_message(message, message_size,
                                   "164-FRAME FOUL PRESENTATION FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL SETTLEMENT WAS NOT EXPLICIT");
        return false;
    }
    before = state;
    if (tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_CLOCK_PATH_COUNT) ||
        tecmo_gameplay_state_hash(&state) !=
            tecmo_gameplay_state_hash(&before) ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        state.shot_clock != 24U || state.clock_divider != 45U) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL DIVIDER-45 SETTLEMENT FAILED");
        return false;
    }

    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_NONE;
    if (!tecmo_gameplay_request_foul(&state, &request)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL RELEASE-GATE INIT FAILED");
        return false;
    }
    state.clock_divider = 21U;
    state.shot_clock = 7U;
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        state.phase_frame != 1U) {
        gameplay_self_test_message(message, message_size,
                                   "EARLY A RELEASE DISMISSED FOUL");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].held.nes_a_pass_switch = true;
    input.controllers[0].held.nes_start = true;
    input.controllers[1].released.dpad_up = true;
    if (!gameplay_self_test_run_frames(&state, 4U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        state.phase_frame != 5U || state.clock_divider != 21U ||
        state.shot_clock != 7U) {
        gameplay_self_test_message(message, message_size,
                                   "NON-RELEASE INPUT DISMISSED FOUL");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[1].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_AWAY,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        state.shot_clock != 24U || state.clock_divider != 50U) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL A-RELEASE/DIVIDER-50 FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL CAP INIT FAILED");
        return false;
    }
    request.fouling_team = TECMO_GAMEPLAY_TEAM_AWAY;
    request.free_throw_team = TECMO_GAMEPLAY_TEAM_HOME;
    request.player_index = 2U;
    request.free_throw_attempts = 0U;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_INDIVIDUAL;
    if (!tecmo_gameplay_request_foul(&state, &request) ||
        state.individual_fouls[0][2] != 1U || state.team_fouls[0] != 0U ||
        !gameplay_self_test_release_after_lead_in(
            &state, &context, &events) ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
        gameplay_self_test_message(message, message_size,
                                   "INDIVIDUAL-ONLY FOUL EFFECT FAILED");
        return false;
    }
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_TEAM;
    if (!tecmo_gameplay_request_foul(&state, &request) ||
        state.individual_fouls[0][2] != 1U || state.team_fouls[0] != 1U ||
        !gameplay_self_test_release_after_lead_in(
            &state, &context, &events) ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
        gameplay_self_test_message(message, message_size,
                                   "TEAM-ONLY FOUL EFFECT FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL CAP REINIT FAILED");
        return false;
    }
    request.player_index = 0U;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    for (unsigned foul = 0U; foul < 7U; ++foul) {
        if (!tecmo_gameplay_request_foul(&state, &request) ||
            !gameplay_self_test_release_after_lead_in(
                &state, &context, &events) ||
            !tecmo_gameplay_settle_foul_presentation(
                &state, TECMO_GAMEPLAY_TEAM_HOME,
                TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
            gameplay_self_test_message(message, message_size,
                                       "FOUL CAP REQUEST FAILED");
            return false;
        }
        if (foul == 3U && tecmo_gameplay_team_in_bonus(
                              &state, TECMO_GAMEPLAY_TEAM_AWAY)) {
            gameplay_self_test_message(message, message_size,
                                       "REGULATION BONUS STARTED BEFORE FIVE");
            return false;
        }
    }
    if (state.individual_fouls[0][0] != 6U ||
        state.team_fouls[0] != 5U ||
        !tecmo_gameplay_individual_fouled_out(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        !tecmo_gameplay_team_in_bonus(&state, TECMO_GAMEPLAY_TEAM_AWAY) ||
        tecmo_gameplay_bonus_threshold(&state) != 5U) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL CAP/BONUS CONTRACT FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "OVERTIME BONUS INIT FAILED");
        return false;
    }
    state.period = 5U;
    state.overtime_count = 1U;
    state.clock_minutes = 2U;
    request.fouling_team = TECMO_GAMEPLAY_TEAM_HOME;
    request.free_throw_team = TECMO_GAMEPLAY_TEAM_AWAY;
    request.player_index = 1U;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    for (unsigned foul = 0U; foul < 4U; ++foul) {
        if (!tecmo_gameplay_request_foul(&state, &request) ||
            !gameplay_self_test_release_after_lead_in(
                &state, &context, &events) ||
            !tecmo_gameplay_settle_foul_presentation(
                &state, TECMO_GAMEPLAY_TEAM_AWAY,
                TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
            gameplay_self_test_message(message, message_size,
                                       "OVERTIME FOUL REQUEST FAILED");
            return false;
        }
    }
    if (tecmo_gameplay_bonus_threshold(&state) != 4U ||
        !tecmo_gameplay_team_in_bonus(&state, TECMO_GAMEPLAY_TEAM_HOME)) {
        gameplay_self_test_message(message, message_size,
                                   "OVERTIME BONUS THRESHOLD FAILED");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL VALIDATION INIT FAILED");
        return false;
    }
    request.fouling_team = TECMO_GAMEPLAY_TEAM_AWAY;
    request.free_throw_team = TECMO_GAMEPLAY_TEAM_HOME;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    request.player_index = TECMO_GAMEPLAY_PLAYER_COUNT;
    request.free_throw_attempts = 1U;
    before = state;
    if (tecmo_gameplay_request_foul(&state, &request)) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID FOUL PLAYER ACCEPTED");
        return false;
    }
    request.player_index = 1U;
    request.free_throw_attempts = 4U;
    if (tecmo_gameplay_request_foul(&state, &request)) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID FREE THROW COUNT ACCEPTED");
        return false;
    }
    request.free_throw_attempts = 3U;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_EFFECT_COUNT;
    if (tecmo_gameplay_request_foul(&state, &request) ||
        tecmo_gameplay_request_foul(&state, NULL) ||
        tecmo_gameplay_state_hash(&state) !=
            tecmo_gameplay_state_hash(&before)) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID FOUL REQUEST MUTATED STATE");
        return false;
    }
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    if (!tecmo_gameplay_request_foul(&state, &request) ||
        !gameplay_self_test_release_after_lead_in(
            &state, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        state.free_throws.attempts_remaining != 3U ||
        state.shot_clock != 24U || state.clock_divider != 45U) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL TO FREE THROW HANDOFF FAILED");
        return false;
    }

    if (!tecmo_gameplay_record_free_throw_result(&state, true, &events) ||
        state.score[1] != 1U || state.free_throws.attempts_remaining != 2U ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        !tecmo_gameplay_record_free_throw_result(&state, false, &events) ||
        state.score[1] != 1U || state.free_throws.attempts_remaining != 1U ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        !tecmo_gameplay_record_free_throw_result(&state, true, &events) ||
        state.score[1] != 2U || state.free_throws.attempts_remaining != 0U ||
        state.phase !=
            TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        tecmo_gameplay_record_free_throw_result(&state, true, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "EXPLICIT FREE THROW RESULTS FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].released.nes_a_pass_switch = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase !=
            TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW SETTLEMENT WAS NOT EXPLICIT");
        return false;
    }
    before = state;
    if (tecmo_gameplay_settle_free_throws(
            &state, (TecmoGameplayTeam)TECMO_GAMEPLAY_TEAM_COUNT,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50) ||
        tecmo_gameplay_settle_free_throws(
            &state, TECMO_GAMEPLAY_TEAM_AWAY,
            TECMO_GAMEPLAY_POST_FOUL_CLOCK_PATH_COUNT) ||
        tecmo_gameplay_state_hash(&state) !=
            tecmo_gameplay_state_hash(&before) ||
        !tecmo_gameplay_settle_free_throws(
            &state, TECMO_GAMEPLAY_TEAM_AWAY,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
        state.shot_clock != 24U || state.clock_divider != 50U) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW SETTLEMENT PATH FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_set_score(&state, TECMO_GAMEPLAY_TEAM_HOME,
                                  UINT16_MAX)) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW OVERFLOW INIT FAILED");
        return false;
    }
    request.player_index = 2U;
    request.free_throw_attempts = 1U;
    request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    if (!tecmo_gameplay_request_foul(&state, &request) ||
        !gameplay_self_test_release_after_lead_in(
            &state, &context, &events) ||
        !tecmo_gameplay_settle_foul_presentation(
            &state, TECMO_GAMEPLAY_TEAM_HOME,
            TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW OVERFLOW SETUP FAILED");
        return false;
    }
    before = state;
    if (tecmo_gameplay_record_free_throw_result(&state, true, &events) ||
        state.score[1] != UINT16_MAX ||
        state.free_throws.attempts_remaining != 1U ||
        tecmo_gameplay_state_hash(&state) !=
            tecmo_gameplay_state_hash(&before)) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW SCORE OVERFLOW ACCEPTED");
        return false;
    }
    return true;
}

static uint64_t gameplay_trace_mix_u64(uint64_t hash, uint64_t value)
{
    for (unsigned byte = 0U; byte < 8U; ++byte) {
        hash = gameplay_hash_byte(hash, (uint8_t)(value & 0xFFU));
        value >>= 8U;
    }
    return hash;
}

static bool gameplay_self_test_replay(uint64_t *result)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;
    TecmoGameplayFoulRequest foul_request;
    uint64_t trace_hash = UINT64_C(14695981039346656037);

    if (result == NULL || !tecmo_gameplay_config_init(&config, 3U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        return false;
    }
    foul_request.fouling_team = TECMO_GAMEPLAY_TEAM_HOME;
    foul_request.free_throw_team = TECMO_GAMEPLAY_TEAM_AWAY;
    foul_request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
    foul_request.player_index = 3U;
    foul_request.free_throw_attempts = 2U;

    for (unsigned frame = 0U; frame < 240U; ++frame) {
        tecmo_gameplay_frame_input_clear(&input);
        tecmo_gameplay_live_context_default(&context);

        if (frame == 3U) {
            input.controllers[0].held.nes_b_jump_steal_shot = true;
        }
        if (frame == 45U) {
            input.controllers[1].released.nes_a_pass_switch = true;
        }
        if (frame == 85U) {
            input.controllers[1].released.nes_a_pass_switch = true;
        }

        if (!tecmo_gameplay_update(&state, &input, &context, &events)) {
            return false;
        }

        if (frame == 3U) {
            if (!tecmo_gameplay_nes_b_begin_close_shot_subtype01(
                    &state, &input, 0U,
                    TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY, &events)) {
                return false;
            }
        } else if (frame == 7U || frame == 10U || frame == 13U ||
                   frame == 17U || frame == 22U || frame == 28U) {
            if (!tecmo_gameplay_advance_close_shot_subtype01(&state,
                                                              &events)) {
                return false;
            }
        } else if (frame == 40U) {
            if (!tecmo_gameplay_request_violation(
                    &state, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
                    TECMO_GAMEPLAY_TEAM_HOME)) {
                return false;
            }
        } else if (frame == 80U) {
            if (!tecmo_gameplay_request_foul(&state, &foul_request)) {
                return false;
            }
        } else if (frame == 85U) {
            if (!tecmo_gameplay_settle_foul_presentation(
                    &state, TECMO_GAMEPLAY_TEAM_AWAY,
                    TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45)) {
                return false;
            }
        } else if (frame == 90U) {
            if (!tecmo_gameplay_record_free_throw_result(&state, true,
                                                          &events)) {
                return false;
            }
        } else if (frame == 95U) {
            if (!tecmo_gameplay_record_free_throw_result(&state, false,
                                                          &events) ||
                !tecmo_gameplay_settle_free_throws(
                    &state, TECMO_GAMEPLAY_TEAM_HOME,
                    TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50)) {
                return false;
            }
        } else if (frame == 130U) {
            if (!tecmo_gameplay_reset_possession(
                    &state, TECMO_GAMEPLAY_TEAM_HOME)) {
                return false;
            }
        } else if (frame == 160U) {
            if (!tecmo_gameplay_award_points(
                    &state, TECMO_GAMEPLAY_TEAM_AWAY, 2U)) {
                return false;
            }
        }

        trace_hash = gameplay_hash_u32(trace_hash, frame);
        trace_hash = gameplay_trace_mix_u64(
            trace_hash, tecmo_gameplay_state_hash(&state));
        trace_hash = gameplay_hash_byte(trace_hash, (uint8_t)events.count);
        for (size_t event_index = 0U; event_index < events.count;
             ++event_index) {
            const TecmoGameplayEvent *event = &events.events[event_index];
            trace_hash = gameplay_hash_byte(trace_hash, (uint8_t)event->kind);
            trace_hash = gameplay_hash_u16(trace_hash, event->value);
            trace_hash = gameplay_hash_u16(trace_hash, event->detail);
        }
    }

    *result = trace_hash;
    return true;
}

bool tecmo_gameplay_state_self_test(char *message, size_t message_size)
{
    const uint64_t expected_replay_hash = UINT64_C(0xEAD5CA9E20C3F3C8);
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayState before;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;
    uint64_t replay_a;
    uint64_t replay_b;
    char success[160];

    if (!gameplay_self_test_config_and_input(message, message_size) ||
        !gameplay_self_test_clock_and_periods(message, message_size) ||
        !gameplay_self_test_halftime_and_final(message, message_size) ||
        !gameplay_self_test_strict_state_validation(message, message_size) ||
        !gameplay_self_test_violations(message, message_size) ||
        !gameplay_self_test_fouls_and_free_throws(message, message_size)) {
        return false;
    }

    if (!tecmo_gameplay_config_init(&config, 2U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "STRICT UPDATE INIT FAILED");
        return false;
    }
    before = state;
    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    context.period_expiry = TECMO_GAMEPLAY_EXPIRY_CONTEXT_COUNT;
    if (tecmo_gameplay_update(&state, &input, &context, &events) ||
        tecmo_gameplay_state_hash(&state) !=
            tecmo_gameplay_state_hash(&before) ||
        events.count != 0U) {
        gameplay_self_test_message(message, message_size,
                                   "INVALID FRAME CONTEXT MUTATED STATE");
        return false;
    }

    if (!gameplay_self_test_replay(&replay_a) ||
        !gameplay_self_test_replay(&replay_b) ||
        replay_a != expected_replay_hash || replay_a != replay_b) {
        gameplay_self_test_message(message, message_size,
                                   "DETERMINISTIC REPLAY HASH FAILED");
        return false;
    }

    (void)snprintf(success, sizeof(success),
                   "GAMEPLAY STATE SELF TEST PASS replay=%016llX",
                   (unsigned long long)replay_a);
    gameplay_self_test_message(message, message_size, success);
    return true;
}
