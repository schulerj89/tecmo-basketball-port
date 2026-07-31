#ifndef TECMO_GAMEPLAY_MOVEMENT_H
#define TECMO_GAMEPLAY_MOVEMENT_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT 7U
#define TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT 3U
#define TECMO_GAMEPLAY_MOVEMENT_DIRECTION_TABLE_COUNT 16U
#define TECMO_GAMEPLAY_MOVEMENT_STATE_TAG 0x4F4D4754U

/* NES controller direction bits consumed by Bank05 $8E58. */
typedef enum TecmoGameplayMovementInput {
    TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL = 0x00,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT = 0x01,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT = 0x02,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN = 0x04,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT = 0x05,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT = 0x06,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_UP = 0x08,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT = 0x09,
    TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT = 0x0A
} TecmoGameplayMovementInput;

typedef enum TecmoGameplayMovementSourceKind {
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_PROFILE_AND_SPEED = 1,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_ANIMATION_CONFIG = 2,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_DELTA_HELPERS = 3,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_DIRECTION_HANDLERS = 4,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_INPUT_AND_POSE = 5,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_DIRECTION_MAP = 6,
    TECMO_GAMEPLAY_MOVEMENT_SOURCE_ACTOR_CLAMP = 7
} TecmoGameplayMovementSourceKind;

typedef struct TecmoGameplayMovementSourceSpan {
    TecmoGameplayMovementSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayMovementSourceSpan;

typedef struct TecmoGameplayMovementAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[176];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayMovementSourceSpan
        sources[TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT];
    int8_t speed_adjustment[TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT];
    uint8_t direction_map[TECMO_GAMEPLAY_MOVEMENT_DIRECTION_TABLE_COUNT];
    uint8_t condition_fresh_high_nibble;
    uint8_t minimum_movement_amount;
    uint8_t animation_period;
    uint8_t animation_delay_high_nibble;
    uint8_t animation_transition_high_nibble;
    uint8_t upper_y_gate;
    uint8_t lower_y_gate;
    uint8_t diagonal_numerator;
    uint8_t diagonal_denominator;
    uint16_t left_boundary_base;
    uint16_t right_boundary_base;
    uint32_t gameplay_core_fingerprint;
    uint32_t gameplay_camera_fingerprint;
    uint32_t team_data_fingerprint;
} TecmoGameplayMovementAssets;

/* Native names for the bounded actor-local state used by the ordinary
   Bank05 movement route. The subpixel accumulator is Q4, matching $06F1. */
typedef struct TecmoGameplayMovementState {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate position;
    uint8_t action_state;
    uint8_t direction;
    uint8_t fractional_accumulator;
    uint8_t animation_phase;
    bool boundary_violation_latched;
} TecmoGameplayMovementState;

typedef struct TecmoGameplayMovementStepInput {
    uint8_t held_direction_bits;
    uint8_t player_movement_rating;
    uint8_t condition;
    uint8_t speed_value;
    uint8_t global_object_state;
    uint8_t movement_flags;
    bool primary_selected_actor;
} TecmoGameplayMovementStepInput;

typedef struct TecmoGameplayMovementClampInput {
    TecmoGameplayCourtCoordinate position;
    uint8_t global_object_state;
    uint8_t actor_action_state;
    uint8_t movement_flags;
    uint8_t actor_direction;
    bool primary_selected_actor;
    bool violation_latched;
} TecmoGameplayMovementClampInput;

typedef struct TecmoGameplayMovementClampResult {
    TecmoGameplayCourtCoordinate position;
    bool violation_latched;
    bool clamped;
    bool skipped;
} TecmoGameplayMovementClampResult;

void tecmo_gameplay_movement_assets_init(
    TecmoGameplayMovementAssets *assets);
void tecmo_gameplay_movement_assets_destroy(
    TecmoGameplayMovementAssets *assets);

bool tecmo_gameplay_movement_assets_parse(
    TecmoGameplayMovementAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_camera,
    size_t gameplay_camera_size,
    const uint8_t *team_data,
    size_t team_data_size);

bool tecmo_gameplay_movement_assets_load(
    TecmoGameplayMovementAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayMovementSourceSpan *
tecmo_gameplay_movement_find_source(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementSourceKind kind);

bool tecmo_gameplay_movement_input_valid(uint8_t direction_bits);
bool tecmo_gameplay_movement_state_initialize(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    const TecmoGameplayCourtCoordinate *position,
    uint8_t initial_direction);
bool tecmo_gameplay_movement_state_valid(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementState *state);

/* Pure, transactional ports. Failed validation leaves caller output/state
   untouched. The step includes Bank05's one-update action-state latency. */
bool tecmo_gameplay_movement_clamp(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementClampInput *input,
    TecmoGameplayMovementClampResult *result_out);
bool tecmo_gameplay_movement_step(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    const TecmoGameplayMovementStepInput *input);

/* Exact base-pointer plus animation-low-nibble resolution from $8F47-$8F66.
   `alternate_pose_half` is the $8F02 table-half decision; matching an actor
   to its opponent remains the caller's responsibility. */
bool tecmo_gameplay_movement_pose_index(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementState *state,
    bool alternate_pose_half,
    uint16_t *pose_index_out);

bool tecmo_gameplay_movement_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
