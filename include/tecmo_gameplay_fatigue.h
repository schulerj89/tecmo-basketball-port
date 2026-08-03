#ifndef TECMO_GAMEPLAY_FATIGUE_H
#define TECMO_GAMEPLAY_FATIGUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT 2U
#define TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT 3U
#define TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT 2U
#define TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT 12U
#define TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT 5U
#define TECMO_GAMEPLAY_FATIGUE_STATE_TAG 0x54464754U

typedef enum TecmoGameplayFatigueSourceKind {
    TECMO_GAMEPLAY_FATIGUE_SOURCE_EVOLUTION = 1,
    TECMO_GAMEPLAY_FATIGUE_SOURCE_LIVE_CALLER = 2
} TecmoGameplayFatigueSourceKind;

typedef struct TecmoGameplayFatigueSourceSpan {
    TecmoGameplayFatigueSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayFatigueSourceSpan;

typedef struct TecmoGameplayFatigueAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[176];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayFatigueSourceSpan
        sources[TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT];
    uint8_t cadence_reload[TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT];
    uint8_t capacity_profile_index;
    uint8_t condition_maximum;
    uint8_t recovery_increment;
    uint8_t recovery_reload;
    uint32_t team_data_fingerprint;
} TecmoGameplayFatigueAssets;

typedef struct TecmoGameplayFatigueRosterSeed {
    uint8_t condition;
    uint8_t maximum_capacity;
} TecmoGameplayFatigueRosterSeed;

typedef struct TecmoGameplayFatigueState {
    uint32_t contract_tag;
    uint8_t cadence_counter;
    uint8_t condition[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
                     [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
    uint8_t capacity[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
                    [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
    uint8_t countdown[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
                     [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
    uint8_t maximum_capacity[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
                            [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
} TecmoGameplayFatigueState;

typedef struct TecmoGameplayFatigueStepInput {
    uint8_t difficulty;
    uint8_t active_roster[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
                         [TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT];
} TecmoGameplayFatigueStepInput;

void tecmo_gameplay_fatigue_assets_init(TecmoGameplayFatigueAssets *assets);
void tecmo_gameplay_fatigue_assets_destroy(
    TecmoGameplayFatigueAssets *assets);
bool tecmo_gameplay_fatigue_assets_parse(
    TecmoGameplayFatigueAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *team_data,
    size_t team_data_size);
bool tecmo_gameplay_fatigue_assets_load(
    TecmoGameplayFatigueAssets *assets,
    const char *asset_pack_path);
const TecmoGameplayFatigueSourceSpan *tecmo_gameplay_fatigue_find_source(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueSourceKind kind);

bool tecmo_gameplay_fatigue_state_initialize(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueState *state,
    const TecmoGameplayFatigueRosterSeed
        seeds[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
             [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT]);
bool tecmo_gameplay_fatigue_state_valid(
    const TecmoGameplayFatigueAssets *assets,
    const TecmoGameplayFatigueState *state);
/* Exact Bank02 $B4E9 active decay and bench recovery, including the original
   second-team countdown-store asymmetry. A cadence-triggering active step
   intentionally preserves the byte wrap 0 -> 255. Failed validation is
   transactional; the public post-step cadence boundary rejects only the
   unreachable reload value 6. */
bool tecmo_gameplay_fatigue_step(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueState *state,
    const TecmoGameplayFatigueStepInput *input);

bool tecmo_gameplay_fatigue_self_test(const char *asset_pack_path,
                                      char *message,
                                      size_t message_size);

#endif
