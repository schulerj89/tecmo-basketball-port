#ifndef TECMO_GAMEPLAY_JUMP_SHOTS_H
#define TECMO_GAMEPLAY_JUMP_SHOTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT 12U
#define TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT 2U
#define TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT 2U
#define TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT 8U
#define TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT 32U

typedef enum TecmoGameplayJumpShotSourceKind {
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_SIGNED_MATH = 1,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_FAMILY_BASES = 2,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ANIMATION_COUNTER = 3,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_INITIAL_VELOCITY = 4,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_PHASE_DECREMENT = 5,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_MADE_STATE08 = 6,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ROUTE1_FOLLOW_RELEASE = 7,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ROUTE10 = 8,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_BOUNCE_MOTION_COLLISION = 9,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_POST_SHOT_SETTLEMENT = 10,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_DISTANCE_HELPERS = 11,
    TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_DISTANCE_TABLE = 12
} TecmoGameplayJumpShotSourceKind;

typedef enum TecmoGameplayJumpShotFamily {
    TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0 = 0,
    TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_1 = 1
} TecmoGameplayJumpShotFamily;

typedef enum TecmoGameplayJumpShotProfile {
    TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0 = 0,
    TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_1 = 1
} TecmoGameplayJumpShotProfile;

typedef enum TecmoGameplayJumpShotDirection {
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_0 = 0,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1 = 1,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_2 = 2,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_3 = 3,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_4 = 4,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_5 = 5,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_6 = 6,
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_7 = 7
} TecmoGameplayJumpShotDirection;

/* The original captured ordinary-shot fixture.  It remains the shooting-lab
   default and a legacy diagnostic identity; bound gameplay instead uses the
   retained Bank02 profile bit and Bank05 eight-way $05A0 direction.  Family
   selection remains bounded to family 0 until the full $8B83-$8BC8 gate is
   represented. */
#define TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_FAMILY \
    TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_0
#define TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_PROFILE \
    TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_0
#define TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_DIRECTION \
    TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_1
#define TECMO_GAMEPLAY_JUMP_SHOT_CAPTURED_SELECTION 1U

typedef struct TecmoGameplayJumpShotConstants {
    uint8_t nes_b_mask;
    uint8_t actor_state_gather;
    uint8_t actor_state_prepared;
    uint8_t actor_state_held;
    uint8_t actor_state_airborne;
    uint8_t actor_state_recovery;
    uint8_t actor_state_neutral;
    uint8_t phase_seed_gather;
    uint8_t phase_seed_prepared;
    uint8_t phase_seed_airborne;
    uint8_t phase_seed_recovery;
    uint8_t phase_seed_recovery_counter;
    uint8_t ball_state_launch;
    uint8_t ball_state_route1;
    uint8_t ball_state_route5;
    uint8_t ball_state_route17;
    uint8_t ball_state_route10;
    uint8_t ball_state_neutral;
    uint8_t outcome_flag_mask;
    uint8_t crowd_sfx;
    uint8_t side_result_base;
    uint16_t gravity_q8;
    uint8_t floor_wrap_clamp;
    uint16_t bounce_decay_q8;
    uint8_t made_state;
    uint8_t made_initial_timer;
    uint8_t made_repeat_timer;
    uint8_t made_terminal_stage;
    uint8_t made_update_count;
    uint8_t flight_state;
    uint8_t result_state;
    uint8_t flight_count_limit;
    uint8_t flight_result_altitude_threshold;
    uint8_t made_complete_state;
} TecmoGameplayJumpShotConstants;

typedef struct TecmoGameplayJumpShotFlightState {
    uint16_t world_x_q6;
    uint16_t world_y_q6;
    uint16_t velocity_x_q6;
    uint16_t velocity_y_q6;
    uint16_t altitude_q8;
    uint16_t altitude_velocity_q8;
    uint16_t remaining_updates;
    uint8_t object_state;
} TecmoGameplayJumpShotFlightState;

typedef struct TecmoGameplayJumpShotMadeSettlement {
    uint8_t state;
    uint8_t timer;
    uint8_t stage;
    uint8_t updates;
    bool complete;
} TecmoGameplayJumpShotMadeSettlement;

typedef struct TecmoGameplayJumpShotSourceSpan {
    TecmoGameplayJumpShotSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint64_t fingerprint_fnv1a64;
    const uint8_t *bytes;
} TecmoGameplayJumpShotSourceSpan;

typedef struct TecmoGameplayJumpShotAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayJumpShotSourceSpan
        sources[TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT];
    TecmoGameplayJumpShotConstants constants;
    const uint8_t *pose_indices;
    uint32_t gameplay_core_fingerprint;
    uint32_t close_shots_fingerprint;
} TecmoGameplayJumpShotAssets;

void tecmo_gameplay_jump_shots_init(TecmoGameplayJumpShotAssets *assets);
void tecmo_gameplay_jump_shots_destroy(TecmoGameplayJumpShotAssets *assets);

bool tecmo_gameplay_jump_shots_parse(
    TecmoGameplayJumpShotAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *close_shots,
    size_t close_shots_size);

bool tecmo_gameplay_jump_shots_load(TecmoGameplayJumpShotAssets *assets,
                                    const char *asset_pack_path);

const TecmoGameplayJumpShotSourceSpan *tecmo_gameplay_jump_shots_find_source(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotSourceKind kind);

bool tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFamily family,
    TecmoGameplayJumpShotProfile profile,
    TecmoGameplayJumpShotDirection direction,
    uint16_t *pointer_index);

/* Bank07 renders a committed Bank05 sequence by adding twice the low nibble
   of $0458 to the selected $0442/$044D byte offset.  pose_indices stores that
   selected sequence base as a TGPL pointer index, so the native equivalent is
   base + phase. */
bool tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFamily family,
    TecmoGameplayJumpShotProfile profile,
    TecmoGameplayJumpShotDirection direction,
    uint8_t animation_byte,
    uint16_t *pointer_index);

/* Bank05's native unsigned Q8.8 update. Velocity subtracts gravity first,
   then altitude adds the wrapped 16-bit velocity. A zero or wrapped-negative
   high byte clamps altitude and velocity to zero. */
bool tecmo_gameplay_jump_shots_step_q8(
    const TecmoGameplayJumpShotAssets *assets,
    uint16_t *altitude_q8,
    uint16_t *velocity_q8,
    bool *landed);

/* Test-only translation of Bank05 $B100 using explicit raw state. It does not
   derive the still-unproven $AD6E launch inputs. */
bool tecmo_gameplay_jump_shots_step_distance_flight(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFlightState *state);
bool tecmo_gameplay_jump_shots_begin_distance_flight(
    const TecmoGameplayJumpShotAssets *assets,
    uint16_t start_x,
    uint8_t start_y,
    uint16_t target_x,
    uint8_t target_y,
    uint16_t start_altitude_q8,
    uint8_t context,
    TecmoGameplayJumpShotFlightState *state);

bool tecmo_gameplay_jump_shots_made_settlement_begin(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotMadeSettlement *settlement);
bool tecmo_gameplay_jump_shots_made_settlement_step(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotMadeSettlement *settlement,
    bool stalled);

#endif
