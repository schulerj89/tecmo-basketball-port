#ifndef TECMO_GAMEPLAY_STATE_H
#define TECMO_GAMEPLAY_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CONTROLLER_COUNT 2U
#define TECMO_GAMEPLAY_PLAYER_COUNT 12U
#define TECMO_GAMEPLAY_EVENT_CAPACITY 8U

#define TECMO_GAMEPLAY_CLOCK_DIVIDER_FRAMES 45U
#define TECMO_GAMEPLAY_POSSESSION_DIVIDER_FRAMES 50U
#define TECMO_GAMEPLAY_SHOT_CLOCK_SECONDS 24U
#define TECMO_GAMEPLAY_PERIOD_BANNER_FRAMES 60U
#define TECMO_GAMEPLAY_HALFTIME_BANNER_FRAMES 120U
#define TECMO_GAMEPLAY_PERIOD_EXPIRY_WAIT_FRAMES 31U
#define TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES 4U
#define TECMO_GAMEPLAY_VIOLATION_WAIT_FRAMES 120U
#define TECMO_GAMEPLAY_FOUL_WAIT_FRAMES 160U
#define TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES 124U
#define TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES 164U

/* ROM request values. The state layer reports them; it never synthesizes audio. */
#define TECMO_GAMEPLAY_SFX_EXPIRY_ID 3U
#define TECMO_GAMEPLAY_SFX_LATE_CLOCK_ID 14U
#define TECMO_GAMEPLAY_PRESENTATION_MUSIC_ID 6U
#define TECMO_GAMEPLAY_RESTART_PLAY_ID 5U

typedef enum TecmoGameplayTeam {
    TECMO_GAMEPLAY_TEAM_AWAY = 0,
    TECMO_GAMEPLAY_TEAM_HOME = 1,
    TECMO_GAMEPLAY_TEAM_COUNT = 2
} TecmoGameplayTeam;

typedef enum TecmoGameplayPhase {
    TECMO_GAMEPLAY_PHASE_LIVE = 0,
    TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_LIVE_SETTLE,
    TECMO_GAMEPLAY_PHASE_PERIOD_EXPIRY_FIXED_WAIT,
    TECMO_GAMEPLAY_PHASE_PERIOD_BANNER,
    TECMO_GAMEPLAY_PHASE_HALFTIME_BANNER,
    TECMO_GAMEPLAY_PHASE_HALFTIME_SCORE_SCREEN,
    TECMO_GAMEPLAY_PHASE_FINAL_SCORE_SCREEN,
    TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION,
    TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION,
    TECMO_GAMEPLAY_PHASE_FOUL_SETTLEMENT_REQUIRED,
    TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE,
    TECMO_GAMEPLAY_PHASE_FREE_THROW_SETTLEMENT_REQUIRED,
    TECMO_GAMEPLAY_PHASE_COMPLETE,
    TECMO_GAMEPLAY_PHASE_COUNT
} TecmoGameplayPhase;

typedef enum TecmoGameplayBanner {
    TECMO_GAMEPLAY_BANNER_FIRST_PERIOD = 0,
    TECMO_GAMEPLAY_BANNER_SECOND_PERIOD = 1,
    TECMO_GAMEPLAY_BANNER_THIRD_PERIOD = 2,
    TECMO_GAMEPLAY_BANNER_FOURTH_PERIOD = 3,
    TECMO_GAMEPLAY_BANNER_OVERTIME = 4,
    TECMO_GAMEPLAY_BANNER_HALFTIME = 5,
    TECMO_GAMEPLAY_BANNER_NONE = 255
} TecmoGameplayBanner;

typedef enum TecmoGameplayViolation {
    TECMO_GAMEPLAY_VIOLATION_NONE = 0,
    TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS = 1,
    TECMO_GAMEPLAY_VIOLATION_BACKCOURT = 2,
    TECMO_GAMEPLAY_VIOLATION_FIVE_SECONDS = 3,
    TECMO_GAMEPLAY_VIOLATION_TEN_SECONDS = 4,
    TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK = 5,
    TECMO_GAMEPLAY_VIOLATION_TRAVELING = 6,
    TECMO_GAMEPLAY_VIOLATION_GOALTENDING = 7
} TecmoGameplayViolation;

/*
 * The fixed expiry routine treats a bounded set of live action states
 * differently from all other states. High-level gameplay supplies that
 * classification without exposing the original numeric actor-state values.
 */
typedef enum TecmoGameplayPeriodExpiryContext {
    TECMO_GAMEPLAY_EXPIRY_OTHER = 0,
    TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION,
    TECMO_GAMEPLAY_EXPIRY_ALLOWED_LIVE_ACTION_SETTLED,
    TECMO_GAMEPLAY_EXPIRY_CONTEXT_COUNT
} TecmoGameplayPeriodExpiryContext;

/*
 * ALLOWED_LIVE_ACTION must be observed before ALLOWED_LIVE_ACTION_SETTLED can
 * bypass the fixed wait. A zero-clock frame with an initially settled context
 * still takes the 31-frame OTHER path. Once an allowed action was observed,
 * the zero-clock update enters an unbounded settlement state until a later
 * frame supplies SETTLED; the next 31 OTHER updates are inclusive wait frames.
 */

typedef struct TecmoGameplayLiveContext {
    TecmoGameplayPeriodExpiryContext period_expiry;
    /* Mirrors the proven live-state exemption at the shot-clock-zero gate. */
    bool shot_clock_violation_exempt;
} TecmoGameplayLiveContext;

/*
 * Gameplay names the NES buttons by their original responsibilities. This
 * deliberately avoids the frontend aliases where "confirm" is START,
 * "cancel" is NES B, and "shoot" is NES A.
 */
typedef struct TecmoGameplayPadButtons {
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    bool nes_a_pass_switch;
    bool nes_b_jump_steal_shot;
    bool nes_select;
    bool nes_start;
} TecmoGameplayPadButtons;

typedef struct TecmoGameplayPadInput {
    /*
     * `$EA14-$EA2A` calls `$D2B9-$D2CE`: presentations consume only a
     * transition in released.nes_a_pass_switch, from either controller,
     * after the fixed four-frame lead-in.
     */
    TecmoGameplayPadButtons held;
    TecmoGameplayPadButtons released;
} TecmoGameplayPadInput;

typedef struct TecmoGameplayFrameInput {
    TecmoGameplayPadInput controllers[TECMO_GAMEPLAY_CONTROLLER_COUNT];
} TecmoGameplayFrameInput;

typedef enum TecmoGameplayEventKind {
    TECMO_GAMEPLAY_EVENT_SFX_REQUEST = 0,
    TECMO_GAMEPLAY_EVENT_MUSIC_REQUEST,
    TECMO_GAMEPLAY_EVENT_PLAY_RESTART_REQUEST,
    TECMO_GAMEPLAY_EVENT_SHOT_CLOCK_EXPIRED,
    TECMO_GAMEPLAY_EVENT_CLOSE_SHOT_PHASE_CHANGED,
    TECMO_GAMEPLAY_EVENT_FREE_THROW_RESULT,
    TECMO_GAMEPLAY_EVENT_GAME_COMPLETE,
    TECMO_GAMEPLAY_EVENT_KIND_COUNT
} TecmoGameplayEventKind;

typedef struct TecmoGameplayEvent {
    TecmoGameplayEventKind kind;
    uint16_t value;
    uint16_t detail;
} TecmoGameplayEvent;

typedef struct TecmoGameplayEventBuffer {
    TecmoGameplayEvent events[TECMO_GAMEPLAY_EVENT_CAPACITY];
    size_t count;
} TecmoGameplayEventBuffer;

typedef enum TecmoGameplayCloseShotPhase {
    TECMO_GAMEPLAY_CLOSE_SHOT_NEUTRAL = 0,
    TECMO_GAMEPLAY_CLOSE_SHOT_ENTRY,
    TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_A,
    TECMO_GAMEPLAY_CLOSE_SHOT_GATHER_B,
    TECMO_GAMEPLAY_CLOSE_SHOT_LAUNCH,
    TECMO_GAMEPLAY_CLOSE_SHOT_AIRBORNE,
    TECMO_GAMEPLAY_CLOSE_SHOT_RECOVERY,
    TECMO_GAMEPLAY_CLOSE_SHOT_PHASE_COUNT
} TecmoGameplayCloseShotPhase;

typedef enum TecmoGameplayCloseShotObservation {
    TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_ONLY = 0,
    TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVED_RIGHTWARD_TRACE,
    TECMO_GAMEPLAY_CLOSE_SHOT_OBSERVATION_COUNT
} TecmoGameplayCloseShotObservation;

typedef struct TecmoGameplayCloseShotState {
    TecmoGameplayCloseShotPhase phase;
    TecmoGameplayCloseShotObservation observation;
    uint16_t observed_actor_pose_index;
    uint16_t observed_ball_pose_index;
    uint32_t transition_serial;
    bool observed_pose_available;
    bool active;
} TecmoGameplayCloseShotState;

typedef enum TecmoGameplayFoulCounterEffect {
    TECMO_GAMEPLAY_FOUL_COUNTER_NONE = 0,
    TECMO_GAMEPLAY_FOUL_COUNTER_INDIVIDUAL = 1,
    TECMO_GAMEPLAY_FOUL_COUNTER_TEAM = 2,
    TECMO_GAMEPLAY_FOUL_COUNTER_BOTH = 3,
    TECMO_GAMEPLAY_FOUL_COUNTER_EFFECT_COUNT
} TecmoGameplayFoulCounterEffect;

typedef enum TecmoGameplayPostFoulClockPath {
    TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_45 = 0,
    TECMO_GAMEPLAY_POST_FOUL_SHOT_24_DIVIDER_50,
    TECMO_GAMEPLAY_POST_FOUL_CLOCK_PATH_COUNT
} TecmoGameplayPostFoulClockPath;

typedef struct TecmoGameplayFoulRequest {
    TecmoGameplayTeam fouling_team;
    TecmoGameplayTeam free_throw_team;
    TecmoGameplayFoulCounterEffect counter_effect;
    uint8_t player_index;
    uint8_t free_throw_attempts;
} TecmoGameplayFoulRequest;

typedef struct TecmoGameplayConfig {
    uint8_t regulation_minutes;
} TecmoGameplayConfig;

typedef struct TecmoGameplayFreeThrowState {
    TecmoGameplayTeam scoring_team;
    uint8_t attempts_remaining;
} TecmoGameplayFreeThrowState;

typedef struct TecmoGameplayState {
    TecmoGameplayConfig config;
    TecmoGameplayPhase phase;
    TecmoGameplayBanner banner;
    TecmoGameplayViolation violation;
    TecmoGameplayTeam possession;
    TecmoGameplayTeam restart_possession;
    TecmoGameplayFreeThrowState free_throws;
    TecmoGameplayCloseShotState close_shot_subtype01;
    uint16_t score[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t individual_fouls[TECMO_GAMEPLAY_TEAM_COUNT]
                             [TECMO_GAMEPLAY_PLAYER_COUNT];
    uint8_t team_fouls[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t period;
    uint8_t overtime_count;
    uint8_t clock_minutes;
    uint8_t clock_seconds;
    uint8_t clock_divider;
    uint8_t shot_clock;
    uint16_t phase_frame;
    uint8_t expiry_wait_frames_remaining;
    bool period_expiry_allowed_live_observed;
    bool initialized;
} TecmoGameplayState;

bool tecmo_gameplay_config_init(TecmoGameplayConfig *config,
                                uint8_t regulation_minutes);
bool tecmo_gameplay_config_valid(const TecmoGameplayConfig *config);
uint8_t tecmo_gameplay_overtime_minutes(uint8_t regulation_minutes);

void tecmo_gameplay_frame_input_clear(TecmoGameplayFrameInput *input);
void tecmo_gameplay_live_context_default(TecmoGameplayLiveContext *context);
bool tecmo_gameplay_input_any(const TecmoGameplayFrameInput *input);
bool tecmo_gameplay_input_nes_a_pass_switch_held(
    const TecmoGameplayFrameInput *input,
    size_t controller);
bool tecmo_gameplay_input_nes_a_pass_switch_released(
    const TecmoGameplayFrameInput *input,
    size_t controller);
bool tecmo_gameplay_input_nes_b_jump_steal_shot_held(
    const TecmoGameplayFrameInput *input,
    size_t controller);
bool tecmo_gameplay_input_nes_start_held(
    const TecmoGameplayFrameInput *input,
    size_t controller);

void tecmo_gameplay_events_clear(TecmoGameplayEventBuffer *events);

bool tecmo_gameplay_state_init(TecmoGameplayState *state,
                               const TecmoGameplayConfig *config,
                               TecmoGameplayTeam initial_possession);
bool tecmo_gameplay_state_valid(const TecmoGameplayState *state);

/*
 * One call advances exactly one native video frame and first clears `events`.
 * GAME SPEED is purposely absent: render pacing must not alter this update or
 * the emitted event cadence. Explicit result/shot APIs append to an existing
 * buffer so the caller can retain all requests produced during the same frame.
 */
bool tecmo_gameplay_update(TecmoGameplayState *state,
                           const TecmoGameplayFrameInput *input,
                           const TecmoGameplayLiveContext *live_context,
                           TecmoGameplayEventBuffer *events);

bool tecmo_gameplay_reset_possession(TecmoGameplayState *state,
                                     TecmoGameplayTeam possession);
bool tecmo_gameplay_set_score(TecmoGameplayState *state,
                              TecmoGameplayTeam team,
                              uint16_t score);
bool tecmo_gameplay_award_points(TecmoGameplayState *state,
                                 TecmoGameplayTeam team,
                                 uint8_t points);

/* All non-shot-clock violations enter only through this explicit trigger. */
bool tecmo_gameplay_request_violation(TecmoGameplayState *state,
                                      TecmoGameplayViolation violation,
                                      TecmoGameplayTeam restart_possession);

/*
 * Foul detection/subtype stays outside this evidence-bounded state module. The
 * request must explicitly identify which proven counters change; this module
 * does not apply a universal team-foul delta. Presentation completion stops in
 * a settlement-required phase until the caller selects one of the two proven
 * shot-24/divider paths and an explicit possession.
 */
bool tecmo_gameplay_request_foul(TecmoGameplayState *state,
                                 const TecmoGameplayFoulRequest *request);
bool tecmo_gameplay_settle_foul_presentation(
    TecmoGameplayState *state,
    TecmoGameplayTeam next_possession,
    TecmoGameplayPostFoulClockPath clock_path);
bool tecmo_gameplay_individual_fouled_out(const TecmoGameplayState *state,
                                          TecmoGameplayTeam team,
                                          uint8_t player_index);
uint8_t tecmo_gameplay_bonus_threshold(const TecmoGameplayState *state);
bool tecmo_gameplay_team_in_bonus(const TecmoGameplayState *state,
                                  TecmoGameplayTeam team);

/*
 * Attempts are the proven two-bit `$BA & 3` value (strictly 0..3). The caller
 * supplies only the observed made/missed result; there is no invented aim
 * physics.
 */
bool tecmo_gameplay_record_free_throw_result(
    TecmoGameplayState *state,
    bool made,
    TecmoGameplayEventBuffer *events);
bool tecmo_gameplay_settle_free_throws(
    TecmoGameplayState *state,
    TecmoGameplayTeam next_possession,
    TecmoGameplayPostFoulClockPath clock_path);

/*
 * This is numeric ROM subtype 01 only. Runtime phases are semantic and their
 * advancement is explicit, so this layer invents neither duration nor ball
 * trajectory. Pose indexes are exposed only when the caller selects the one
 * bounded rightward observation documented in gameplay-state-foundation.md;
 * they are not universal subtype assets or a claim that this is a dunk/layup.
 */
bool tecmo_gameplay_nes_b_begin_close_shot_subtype01(
    TecmoGameplayState *state,
    const TecmoGameplayFrameInput *input,
    size_t controller,
    TecmoGameplayCloseShotObservation observation,
    TecmoGameplayEventBuffer *events);
bool tecmo_gameplay_advance_close_shot_subtype01(
    TecmoGameplayState *state,
    TecmoGameplayEventBuffer *events);

const char *tecmo_gameplay_phase_name(TecmoGameplayPhase phase);
const char *tecmo_gameplay_violation_name(TecmoGameplayViolation violation);
uint64_t tecmo_gameplay_state_hash(const TecmoGameplayState *state);
bool tecmo_gameplay_state_self_test(char *message, size_t message_size);

#endif
