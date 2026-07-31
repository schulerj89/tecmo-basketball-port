#ifndef TECMO_GAMEPLAY_CPU_STEERING_H
#define TECMO_GAMEPLAY_CPU_STEERING_H

#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_movement.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT 10U
#define TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT 24U
#define TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT 8U
#define TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE 5U
#define TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT 680U
#define TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT 5U
#define TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT 2U
#define TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT 3U
#define TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR 0xFFU
#define TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION 0xFFU
#define TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG 0x48414754U
#define TECMO_GAMEPLAY_CPU_STEERING_HARNESS_RESULT_TAG 0x52414754U
#define TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG 0x494D4754U
#define TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_RESULT_TAG 0x524D4754U

typedef enum TecmoGameplayCpuSteeringSourceKind {
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ACTOR_DISPATCH = 1,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_REFERENCE_DIRECTION = 2,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_DIRECTION = 3,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_DISPATCH = 4,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_HANDLERS = 5,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_APPLY = 6,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_FORMATION_STREAM_SELECT = 7,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_TRAMPOLINE = 8,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_READER = 9,
    TECMO_GAMEPLAY_CPU_STEERING_SOURCE_PLAY_COMMANDS = 10
} TecmoGameplayCpuSteeringSourceKind;

/* This classification names only the bounded effect visible at each exact
   Bank06 handler entry. It is not a complete play-policy or decision model. */
typedef enum TecmoGameplayCpuSteeringCommandKind {
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_CONTROL = 0,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_RELATIVE_TARGET = 1,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ABSOLUTE_TARGET = 2,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ACTOR_TARGET = 3,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_DIRECT_DIRECTION = 4,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_LINKED_TARGET = 5,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_GLOBAL_TARGET = 6,
    TECMO_GAMEPLAY_CPU_STEERING_COMMAND_POINTER_ACTOR_TARGET = 7
} TecmoGameplayCpuSteeringCommandKind;

typedef struct TecmoGameplayCpuSteeringSourceSpan {
    TecmoGameplayCpuSteeringSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayCpuSteeringSourceSpan;

typedef struct TecmoGameplayCpuSteeringCommand {
    uint16_t stream_offset;
    uint16_t cpu_address;
    uint8_t opcode;
    uint8_t arguments[4];
    uint16_t handler_cpu;
    TecmoGameplayCpuSteeringCommandKind kind;
} TecmoGameplayCpuSteeringCommand;

typedef struct TecmoGameplayCpuSteeringAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[192];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayCpuSteeringSourceSpan
        sources[TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT];
    uint16_t command_base_cpu;
    uint16_t command_record_count;
    uint16_t handler_cpu[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint8_t direction_map[TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT];
    uint32_t movement_fingerprint;
} TecmoGameplayCpuSteeringAssets;

/* Bounded native composition policy for exercising the isolated TGAI
   direction boundary with a complete canonical court snapshot. The CLI and
   live scene share it. A linked actor is the port's explicit stand-in for the
   still-unreconstructed ROM $06CB,X assignment. Slots 0..4 are away/team 0
   and slots 5..9 are home/team 1, matching TecmoGameplayScene. This is not a
   reconstructed ROM play selector. */
typedef enum TecmoGameplayCpuSteeringHarnessTargetKind {
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR = 0,
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH,
    TECMO_GAMEPLAY_CPU_STEERING_HARNESS_TARGET_KIND_COUNT
} TecmoGameplayCpuSteeringHarnessTargetKind;

typedef struct TecmoGameplayCpuSteeringHarnessInput {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t actor;
    uint8_t possession;
    uint8_t orientation;
    uint8_t ball_holder;
    uint8_t matchup_actor;
    uint8_t difficulty;
} TecmoGameplayCpuSteeringHarnessInput;

typedef struct TecmoGameplayCpuSteeringHarnessResult {
    uint32_t contract_tag;
    uint32_t input_fingerprint;
    TecmoGameplayCourtCoordinate actor_position;
    TecmoGameplayCourtCoordinate target_position;
    int16_t horizontal_delta;
    int16_t depth_delta;
    uint8_t actor;
    uint8_t actor_team;
    uint8_t possession;
    uint8_t orientation;
    uint8_t ball_holder;
    uint8_t matchup_actor;
    uint8_t difficulty;
    uint8_t target_actor;
    TecmoGameplayCpuSteeringHarnessTargetKind target_kind;
    uint8_t direction;
    bool writes_direction;
} TecmoGameplayCpuSteeringHarnessResult;

/* Transactional CLI/live composition input. The selected actor's canonical
   coordinate must exactly match movement.position. TGAI selects the target
   and direction; the remaining fields are passed to the exact TGMO movement
   kernel as a secondary (non-player-selected) actor. */
typedef struct TecmoGameplayCpuSteeringMovementInput {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringHarnessInput steering;
    TecmoGameplayMovementState movement;
    uint8_t player_movement_rating;
    uint8_t condition;
    uint8_t speed_value;
    uint8_t global_object_state;
    uint8_t movement_flags;
} TecmoGameplayCpuSteeringMovementInput;

typedef struct TecmoGameplayCpuSteeringMovementResult {
    uint32_t contract_tag;
    TecmoGameplayCpuSteeringHarnessResult steering;
    TecmoGameplayMovementState movement;
    uint8_t held_direction_bits;
} TecmoGameplayCpuSteeringMovementResult;

void tecmo_gameplay_cpu_steering_assets_init(
    TecmoGameplayCpuSteeringAssets *assets);
void tecmo_gameplay_cpu_steering_assets_destroy(
    TecmoGameplayCpuSteeringAssets *assets);

bool tecmo_gameplay_cpu_steering_assets_parse(
    TecmoGameplayCpuSteeringAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *movement,
    size_t movement_size);
bool tecmo_gameplay_cpu_steering_assets_load(
    TecmoGameplayCpuSteeringAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayCpuSteeringSourceSpan *
tecmo_gameplay_cpu_steering_find_source(
    const TecmoGameplayCpuSteeringAssets *assets,
    TecmoGameplayCpuSteeringSourceKind kind);

/* `stream_offset` is the exact actor-local $0547/$0551 offset added to
   Bank04 CPU base $9F2E. It must be aligned to a five-byte command record. */
bool tecmo_gameplay_cpu_steering_decode_command(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint16_t stream_offset,
    TecmoGameplayCpuSteeringCommand *command_out);

/* Exact $92D4-$92DD target gate plus $88DA-$899D octant decision, including
   the 6502 routine's unsigned 16-bit magnitude/doubling behavior. Deltas are
   target minus actor in the ROM's horizontal and court-depth axes. The gate
   skips a zero vector, represented here by a transactional false return. */
bool tecmo_gameplay_cpu_steering_direction_for_delta(
    const TecmoGameplayCpuSteeringAssets *assets,
    int16_t horizontal_delta,
    int16_t depth_delta,
    uint8_t *direction_out);

const char *tecmo_gameplay_cpu_steering_direction_name(uint8_t direction);
const char *tecmo_gameplay_cpu_steering_command_kind_name(
    TecmoGameplayCpuSteeringCommandKind kind);

/* Pure and transactional. Every one of the ten coordinates is validated and
   included in input_fingerprint. The selected target is implementation-owned:
   the holder uses the current native hoop-approach policy; every other actor
   uses its explicit opposing linked/matchup actor. Only the final octant is
   ROM-exact. A zero delta succeeds with writes_direction=false, mirroring the
   ROM gate that preserves the caller's prior direction. */
bool tecmo_gameplay_cpu_steering_harness_evaluate(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringHarnessInput *input,
    TecmoGameplayCpuSteeringHarnessResult *result_out);
const char *tecmo_gameplay_cpu_steering_harness_target_kind_name(
    TecmoGameplayCpuSteeringHarnessTargetKind kind);

/* Pure and transactional TGAI -> TGMO composition. A nonzero TGAI
   result is inverted through TGMO's validated direction table and supplied
   as NES held-direction bits. TGAI's zero-vector no-write case has no NES
   input equivalent, so this adapter-owned boundary supplies neutral while
   preserving TGMO's exact one-update action-state latency. The live scene
   uses this API with scene-owned fixed opposing links. It still does not own
   or claim the ROM command/link lifecycle. */
bool tecmo_gameplay_cpu_steering_movement_step(
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayCpuSteeringMovementInput *input,
    TecmoGameplayCpuSteeringMovementResult *result_out);

bool tecmo_gameplay_cpu_steering_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
