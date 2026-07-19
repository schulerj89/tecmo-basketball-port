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

static uint16_t gameplay_close_shot_pose(TecmoGameplayCloseShotPhase phase)
{
    switch (phase) {
    case TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL:
        return 509U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY_POSE_445:
        return 445U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_254:
        return 254U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_255:
        return 255U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH_POSE_257:
        return 257U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE_POSE_258:
        return 258U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY_POSE_259:
        return 259U;
    case TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT:
    default:
        return UINT16_MAX;
    }
}

static void gameplay_close_shot_set_phase(TecmoGameplayCloseShotState *shot,
                                          TecmoGameplayCloseShotPhase phase)
{
    shot->phase = phase;
    shot->actor_pose_index = gameplay_close_shot_pose(phase);
    shot->ball_pose_index = 64U;
    shot->active = phase != TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL;
    ++shot->transition_serial;
}

static void gameplay_clock_reset(TecmoGameplayState *state, uint8_t minutes)
{
    state->clock_minutes = minutes;
    state->clock_seconds = 0U;
    state->clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
}

static void gameplay_period_team_fouls_reset(TecmoGameplayState *state)
{
    memset(state->team_fouls, 0, sizeof(state->team_fouls));
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
               banner == TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD ||
               banner == TECMO_GAMEPLAY_BANNER_FIRST_PERIOD) {
        minutes = state->config.regulation_minutes;
    } else {
        return false;
    }

    if (minutes == 0U) {
        return false;
    }

    gameplay_clock_reset(state, minutes);
    gameplay_period_team_fouls_reset(state);
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
    const bool tied = state->score[TECMO_GAMEPLAY_TEAM_AWAY] ==
                      state->score[TECMO_GAMEPLAY_TEAM_HOME];

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
        if (state->overtime_count == UINT8_MAX) {
            return false;
        }
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
    const TecmoGameplayLiveContext *live_context,
    TecmoGameplayEventBuffer *events)
{
    state->phase_frame = 0U;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
    state->violation = TECMO_GAMEPLAY_VIOLATION_NONE;

    if (live_context->period_expiry ==
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED) {
        return gameplay_finish_period(state, events);
    }
    if (live_context->period_expiry ==
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION) {
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

static void gameplay_advance_paused_clock_divider(TecmoGameplayState *state)
{
    if (state->clock_divider > 1U) {
        --state->clock_divider;
    } else {
        state->clock_divider = TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES;
    }
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
        return gameplay_enter_period_expiry(state, live_context, events);
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

static bool gameplay_pad_any(const TecmoGameplayPadInput *pad)
{
    return pad != NULL &&
           (pad->dpad_up || pad->dpad_down ||
            pad->dpad_left || pad->dpad_right ||
            pad->nes_a_pass_switch || pad->nes_b_jump_steal_shot ||
            pad->nes_select || pad->nes_start);
}

bool tecmo_gameplay_input_any(const TecmoGameplayFrameInput *input)
{
    if (input == NULL) {
        return false;
    }
    return gameplay_pad_any(&input->controllers[0]) ||
           gameplay_pad_any(&input->controllers[1]);
}

bool tecmo_gameplay_input_nes_a_pass_switch(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].nes_a_pass_switch;
}

bool tecmo_gameplay_input_nes_b_jump_steal_shot(
    const TecmoGameplayFrameInput *input,
    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].nes_b_jump_steal_shot;
}

bool tecmo_gameplay_input_nes_start(const TecmoGameplayFrameInput *input,
                                    size_t controller)
{
    return input != NULL && controller < TECMO_GAMEPLAY_CONTROLLER_COUNT &&
           input->controllers[controller].nes_start;
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
    gameplay_close_shot_set_phase(&state->close_shot_subtype01,
                                  TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL);
    state->close_shot_subtype01.transition_serial = 0U;
    state->initialized = true;
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_state_valid(const TecmoGameplayState *state)
{
    uint8_t maximum_minutes;
    uint16_t expected_pose;

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
            TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT) {
        return false;
    }

    maximum_minutes = state->period == 5U
                          ? tecmo_gameplay_overtime_minutes(
                                state->config.regulation_minutes)
                          : state->config.regulation_minutes;
    if (state->clock_minutes > maximum_minutes ||
        (state->period == 5U && state->overtime_count == 0U &&
         state->phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN &&
         state->phase != TECMO_GAMEPLAY_PHASE_COMPLETE) ||
        (state->period < 5U && state->overtime_count != 0U)) {
        return false;
    }

    expected_pose = gameplay_close_shot_pose(
        state->close_shot_subtype01.phase);
    if (expected_pose == UINT16_MAX ||
        state->close_shot_subtype01.actor_pose_index != expected_pose ||
        state->close_shot_subtype01.ball_pose_index != 64U ||
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
                TECMO_GAMEPLAY_PERIOD_EXPIRY_WAIT_FRAMES) {
            return false;
        }
    } else if (state->expiry_wait_frames_remaining != 0U) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_PERIOD_BANNER) {
        if (state->banner > TECMO_GAMEPLAY_BANNER_OVERTIME ||
            state->phase_frame > TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES) {
            return false;
        }
    } else if (state->phase == TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER) {
        if (state->banner != TECMO_GAMEPLAY_BANNER_HALFTIME ||
            state->phase_frame > TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES) {
            return false;
        }
    } else if (state->banner != TECMO_GAMEPLAY_BANNER_NONE) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION) {
        if (!gameplay_violation_valid(state->violation) ||
            state->phase_frame >
                TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES) {
            return false;
        }
    } else if (state->violation != TECMO_GAMEPLAY_VIOLATION_NONE) {
        return false;
    }

    if (state->phase == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION &&
        state->phase_frame > TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES) {
        return false;
    }
    if (state->phase == TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE) {
        if (state->free_throws.attempts_remaining == 0U) {
            return false;
        }
    } else if (state->phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION &&
               state->free_throws.attempts_remaining != 0U) {
        return false;
    }

    return true;
}

bool tecmo_gameplay_update(TecmoGameplayState *state,
                           const TecmoGameplayFrameInput *input,
                           const TecmoGameplayLiveContext *live_context,
                           TecmoGameplayEventBuffer *events)
{
    const bool any_input = tecmo_gameplay_input_any(input);

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
        if (live_context->period_expiry ==
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
        if (any_input) {
            if (!gameplay_start_period_banner(
                    state, TECMO_GAMEPLAY_BANNER_THIRD_PERIOD)) {
                return false;
            }
        } else if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN:
        if (any_input) {
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
        gameplay_advance_paused_clock_divider(state);
        ++state->phase_frame;
        if (any_input ||
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
        gameplay_advance_paused_clock_divider(state);
        ++state->phase_frame;
        if (any_input ||
            state->phase_frame >= TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES) {
            state->phase_frame = 0U;
            if (state->free_throws.attempts_remaining > 0U) {
                state->possession = state->free_throws.scoring_team;
                state->phase = TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE;
            } else {
                state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
            }
        }
        break;

    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE:
        /* Aim, release timing, rebound, and ball motion remain evidence-gated. */
        gameplay_advance_paused_clock_divider(state);
        if (state->phase_frame < UINT16_MAX) {
            ++state->phase_frame;
        }
        break;

    case TECMO_GAMEPLAY_PHASE_COMPLETE:
        break;

    case TECMO_GAMEPLAY_PHASE_COUNT:
    default:
        return false;
    }

    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_reset_possession(TecmoGameplayState *state,
                                     TecmoGameplayTeam possession)
{
    if (!tecmo_gameplay_state_valid(state) ||
        !gameplay_team_valid(possession)) {
        return false;
    }

    state->possession = possession;
    state->shot_clock = TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS;
    state->clock_divider = TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES;
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_set_score(TecmoGameplayState *state,
                              TecmoGameplayTeam team,
                              uint16_t score)
{
    if (!tecmo_gameplay_state_valid(state) || !gameplay_team_valid(team)) {
        return false;
    }
    state->score[(size_t)team] = score;
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

bool tecmo_gameplay_request_foul(TecmoGameplayState *state,
                                 TecmoGameplayTeam fouling_team,
                                 uint8_t player_index,
                                 TecmoGameplayTeam free_throw_team,
                                 uint8_t free_throw_attempts)
{
    uint8_t *individual;
    uint8_t *team;

    if (!tecmo_gameplay_state_valid(state) ||
        state->phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !gameplay_team_valid(fouling_team) ||
        !gameplay_team_valid(free_throw_team) ||
        fouling_team == free_throw_team ||
        player_index >= TECMO_GAMEPLAY_PLAYER_COUNT ||
        free_throw_attempts > 3U) {
        return false;
    }

    individual = &state->individual_fouls[(size_t)fouling_team][player_index];
    team = &state->team_fouls[(size_t)fouling_team];
    if (*individual < 6U) {
        ++*individual;
    }
    if (*team < 5U) {
        ++*team;
    }

    state->free_throws.scoring_team = free_throw_team;
    state->free_throws.attempts_remaining = free_throw_attempts;
    state->phase = TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION;
    state->phase_frame = 0U;
    state->banner = TECMO_GAMEPLAY_BANNER_NONE;
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
        /* Possession deliberately stays with the shooting team at this boundary. */
        state->phase = TECMO_GAMEPLAY_PHASE_LIVE;
    }
    return tecmo_gameplay_state_valid(state);
}

bool tecmo_gameplay_nes_b_begin_close_shot_subtype01(
    TecmoGameplayState *state,
    const TecmoGameplayFrameInput *input,
    size_t controller,
    TecmoGameplayEventBuffer *events)
{
    if (!tecmo_gameplay_state_valid(state) || events == NULL ||
        events->count >= TECMO_GAMEPLAY_EVENT_CAPACITY ||
        state->phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state->close_shot_subtype01.active ||
        !tecmo_gameplay_input_nes_b_jump_steal_shot(input, controller)) {
        return false;
    }

    gameplay_close_shot_set_phase(
        &state->close_shot_subtype01,
        TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY_POSE_445);
    if (!gameplay_event_append(
            events,
            TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
            (uint16_t)state->close_shot_subtype01.phase,
            state->close_shot_subtype01.actor_pose_index)) {
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
    case TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY_POSE_445:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_254;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_254:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_255;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_POSE_255:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH_POSE_257;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH_POSE_257:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE_POSE_258;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE_POSE_258:
        next = TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY_POSE_259;
        break;
    case TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY_POSE_259:
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
            state->close_shot_subtype01.actor_pose_index)) {
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
    case TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE: return "free-throw-sequence";
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
    hash = gameplay_hash_u16(
        hash, state->close_shot_subtype01.actor_pose_index);
    hash = gameplay_hash_u16(
        hash, state->close_shot_subtype01.ball_pose_index);
    hash = gameplay_hash_u32(
        hash, state->close_shot_subtype01.transition_serial);
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

static bool gameplay_self_test_config_and_input(char *message,
                                                size_t message_size)
{
    static const uint8_t valid_minutes[] = {2U, 3U, 4U, 8U, 12U};
    static const uint8_t overtime_minutes[] = {1U, 1U, 2U, 3U, 5U};
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
    input.controllers[0].nes_start = true;
    if (!tecmo_gameplay_input_nes_start(&input, 0U) ||
        tecmo_gameplay_input_nes_a_pass_switch(&input, 0U) ||
        tecmo_gameplay_input_nes_b_jump_steal_shot(&input, 0U) ||
        tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "START WAS REVERSED INTO SHOT INPUT");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].nes_a_pass_switch = true;
    if (!tecmo_gameplay_input_nes_a_pass_switch(&input, 0U) ||
        tecmo_gameplay_input_nes_b_jump_steal_shot(&input, 0U) ||
        tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "NES A PASS/SWITCH MAPPING FAILED");
        return false;
    }

    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].nes_b_jump_steal_shot = true;
    if (!tecmo_gameplay_nes_b_begin_close_shot_subtype01(
            &state, &input, 0U, &events) ||
        state.close_shot_subtype01.actor_pose_index != 445U ||
        state.close_shot_subtype01.ball_pose_index != 64U ||
        !gameplay_events_contain(
            &events, TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
            TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY_POSE_445)) {
        gameplay_self_test_message(message, message_size,
                                   "NUMERIC CLOSE SHOT ENTRY FAILED");
        return false;
    }

    for (size_t index = 0U;
         index < sizeof(expected_advance_poses) /
                     sizeof(expected_advance_poses[0]);
         ++index) {
        if (!tecmo_gameplay_advance_close_shot_subtype01(&state, &events) ||
            state.close_shot_subtype01.actor_pose_index !=
                expected_advance_poses[index] ||
            state.close_shot_subtype01.ball_pose_index != 64U) {
            gameplay_self_test_message(message, message_size,
                                       "NUMERIC CLOSE SHOT SEQUENCE FAILED");
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
    input.controllers[1].nes_start = true;
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
        !gameplay_self_test_run_frames(&state, 59U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        gameplay_self_test_message(message, message_size,
                                   "31/60 FRAME PERIOD TRANSITION FAILED");
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
        state.clock_minutes = 0U;
        state.clock_seconds = 1U;
        state.clock_divider = 1U;
        state.shot_clock = 10U;
        context.period_expiry =
            TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
        if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
            state.period != 5U || state.overtime_count != 1U ||
            state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
            state.banner != TECMO_GAMEPLAY_BANNER_OVERTIME ||
            state.clock_minutes != overtime_minutes[index]) {
            gameplay_self_test_message(message, message_size,
                                       "OVERTIME MINUTE MATRIX FAILED");
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
    context.period_expiry =
        TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED;
    if (!tecmo_gameplay_config_init(&config, 2U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME INIT FAILED");
        return false;
    }

    state.period = 2U;
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    state.team_fouls[0] = 5U;
    state.team_fouls[1] = 4U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.period != 3U ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        state.banner != TECMO_GAMEPLAY_BANNER_HALFTIME ||
        !gameplay_self_test_run_frames(&state, 119U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                 TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID)) {
        gameplay_self_test_message(message, message_size,
                                   "120-FRAME HALFTIME BANNER FAILED");
        return false;
    }

    if (!gameplay_self_test_run_frames(&state, 500U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME SCORE SCREEN WAS BOUNDED");
        return false;
    }

    input.controllers[1].dpad_left = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        state.banner != TECMO_GAMEPLAY_BANNER_THIRD_PERIOD ||
        state.clock_minutes != 2U || state.clock_seconds != 0U ||
        state.team_fouls[0] != 0U || state.team_fouls[1] != 0U ||
        !gameplay_self_test_run_frames(&state, 60U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        gameplay_self_test_message(message, message_size,
                                   "HALFTIME INPUT/THIRD PERIOD HANDOFF FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);

    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
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
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.period != 5U || state.overtime_count != 0U ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
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
    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.period != 5U || state.overtime_count != 1U ||
        !gameplay_self_test_run_frames(&state, 60U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        gameplay_self_test_message(message, message_size,
                                   "FIRST OVERTIME HANDOFF FAILED");
        return false;
    }

    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.period != 5U || state.overtime_count != 2U ||
        state.phase != TECMO_GAMEPLAY_PHASE_PERIOD_BANNER ||
        !gameplay_self_test_run_frames(&state, 60U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        !tecmo_gameplay_set_score(&state, TECMO_GAMEPLAY_TEAM_AWAY, 1U)) {
        gameplay_self_test_message(message, message_size,
                                   "TIED OVERTIME REPEAT FAILED");
        return false;
    }

    state.clock_minutes = 0U;
    state.clock_seconds = 1U;
    state.clock_divider = 1U;
    state.shot_clock = 10U;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN ||
        !gameplay_events_contain(&events,
                                 TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
                                 TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID) ||
        !gameplay_self_test_run_frames(&state, 500U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN) {
        gameplay_self_test_message(message, message_size,
                                   "NON-TIE FINAL PRESENTATION FAILED");
        return false;
    }

    input.controllers[0].nes_a_pass_switch = true;
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
                                       TECMO_GAMEPLAY_TEAM_AWAY) ||
            !tecmo_gameplay_request_violation(
                &state, violation, TECMO_GAMEPLAY_TEAM_HOME) ||
            state.violation != violation ||
            strcmp(tecmo_gameplay_violation_name(violation), "INVALID") == 0 ||
            !gameplay_self_test_run_frames(&state, 120U, &input, &context,
                                           &events) ||
            state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            state.phase_frame != 120U ||
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
    input.controllers[1].dpad_right = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.phase_frame != 0U) {
        gameplay_self_test_message(message, message_size,
                                   "SECOND CONTROLLER VIOLATION SKIP FAILED");
        return false;
    }
    return true;
}

static bool gameplay_self_test_fouls_and_free_throws(char *message,
                                                     size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState state;
    TecmoGameplayFrameInput input;
    TecmoGameplayLiveContext context;
    TecmoGameplayEventBuffer events;

    tecmo_gameplay_frame_input_clear(&input);
    tecmo_gameplay_live_context_default(&context);
    if (!tecmo_gameplay_config_init(&config, 4U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_request_foul(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
            TECMO_GAMEPLAY_TEAM_HOME, 0U) ||
        !gameplay_self_test_run_frames(&state, 160U, &input, &context,
                                       &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        state.phase_frame != 160U ||
        !tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        gameplay_self_test_message(message, message_size,
                                   "161-FRAME FOUL PRESENTATION FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL CAP INIT FAILED");
        return false;
    }
    input.controllers[0].nes_start = true;
    for (unsigned foul = 0U; foul < 7U; ++foul) {
        if (!tecmo_gameplay_request_foul(
                &state, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
                TECMO_GAMEPLAY_TEAM_HOME, 0U) ||
            !tecmo_gameplay_update(&state, &input, &context, &events)) {
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
    for (unsigned foul = 0U; foul < 4U; ++foul) {
        if (!tecmo_gameplay_request_foul(
                &state, TECMO_GAMEPLAY_TEAM_HOME, 1U,
                TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !tecmo_gameplay_update(&state, &input, &context, &events)) {
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
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        tecmo_gameplay_request_foul(
            &state, TECMO_GAMEPLAY_TEAM_AWAY,
            TECMO_GAMEPLAY_PLAYER_COUNT, TECMO_GAMEPLAY_TEAM_HOME, 1U) ||
        tecmo_gameplay_request_foul(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 0U,
            TECMO_GAMEPLAY_TEAM_HOME, 4U) ||
        !tecmo_gameplay_request_foul(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 1U,
            TECMO_GAMEPLAY_TEAM_HOME, 3U)) {
        gameplay_self_test_message(message, message_size,
                                   "FOUL/FREE THROW VALIDATION FAILED");
        return false;
    }

    input.controllers[1].nes_select = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        state.phase != TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        state.free_throws.attempts_remaining != 3U) {
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
        state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        state.possession != TECMO_GAMEPLAY_TEAM_HOME ||
        tecmo_gameplay_record_free_throw_result(&state, true, &events)) {
        gameplay_self_test_message(message, message_size,
                                   "EXPLICIT FREE THROW RESULTS FAILED");
        return false;
    }

    if (!tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY) ||
        !tecmo_gameplay_set_score(&state, TECMO_GAMEPLAY_TEAM_HOME,
                                  UINT16_MAX) ||
        !tecmo_gameplay_request_foul(
            &state, TECMO_GAMEPLAY_TEAM_AWAY, 2U,
            TECMO_GAMEPLAY_TEAM_HOME, 1U)) {
        gameplay_self_test_message(message, message_size,
                                   "FREE THROW OVERFLOW INIT FAILED");
        return false;
    }
    tecmo_gameplay_frame_input_clear(&input);
    input.controllers[0].nes_start = true;
    if (!tecmo_gameplay_update(&state, &input, &context, &events) ||
        tecmo_gameplay_record_free_throw_result(&state, true, &events) ||
        state.score[1] != UINT16_MAX ||
        state.free_throws.attempts_remaining != 1U) {
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
    uint64_t trace_hash = UINT64_C(14695981039346656037);

    if (result == NULL || !tecmo_gameplay_config_init(&config, 3U) ||
        !tecmo_gameplay_state_init(&state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        return false;
    }

    for (unsigned frame = 0U; frame < 240U; ++frame) {
        tecmo_gameplay_frame_input_clear(&input);
        tecmo_gameplay_live_context_default(&context);

        if (frame == 3U) {
            input.controllers[0].nes_b_jump_steal_shot = true;
        }
        if (frame == 45U) {
            input.controllers[1].nes_start = true;
        }
        if (frame == 85U) {
            input.controllers[1].dpad_up = true;
        }

        if (!tecmo_gameplay_update(&state, &input, &context, &events)) {
            return false;
        }

        if (frame == 3U) {
            if (!tecmo_gameplay_nes_b_begin_close_shot_subtype01(
                    &state, &input, 0U, &events)) {
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
            if (!tecmo_gameplay_request_foul(
                    &state, TECMO_GAMEPLAY_TEAM_HOME, 3U,
                    TECMO_GAMEPLAY_TEAM_AWAY, 2U)) {
                return false;
            }
        } else if (frame == 90U) {
            if (!tecmo_gameplay_record_free_throw_result(&state, true,
                                                          &events)) {
                return false;
            }
        } else if (frame == 95U) {
            if (!tecmo_gameplay_record_free_throw_result(&state, false,
                                                          &events)) {
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
    const uint64_t expected_replay_hash = UINT64_C(0x4DC1884C638DF5C1);
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
