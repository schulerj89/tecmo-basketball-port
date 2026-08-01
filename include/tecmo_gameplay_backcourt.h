#ifndef TECMO_GAMEPLAY_BACKCOURT_H
#define TECMO_GAMEPLAY_BACKCOURT_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT 1U
#define TECMO_GAMEPLAY_BACKCOURT_ORIENTATION_COUNT 2U
#define TECMO_GAMEPLAY_BACKCOURT_STATE_TAG 0x43424754U

typedef enum TecmoGameplayBackcourtSourceKind {
    TECMO_GAMEPLAY_BACKCOURT_SOURCE_DETECTOR = 1
} TecmoGameplayBackcourtSourceKind;

typedef struct TecmoGameplayBackcourtSourceSpan {
    TecmoGameplayBackcourtSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayBackcourtSourceSpan;

typedef struct TecmoGameplayBackcourtAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[176];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayBackcourtSourceSpan
        sources[TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT];
    uint8_t required_global_object_state;
    uint8_t frontcourt_progress_mask;
    uint8_t violation_selector;
    uint8_t orientation_count;
    uint16_t orientation_zero_frontcourt_x;
    uint8_t orientation_zero_return_low;
    uint16_t orientation_one_frontcourt_x;
    uint8_t orientation_one_return_low;
    uint16_t backcourt_code_offset;
    uint16_t backcourt_code_size;
    uint32_t court_orientation_fingerprint;
    uint32_t penalties_fingerprint;
} TecmoGameplayBackcourtAssets;

typedef struct TecmoGameplayBackcourtState {
    uint32_t contract_tag;
    uint8_t frontcourt_established;
    uint8_t reserved[3];
} TecmoGameplayBackcourtState;

typedef struct TecmoGameplayBackcourtStepInput {
    TecmoGameplayCourtCoordinate ball_position;
    uint8_t orientation;
    uint8_t global_object_state;
} TecmoGameplayBackcourtStepInput;

void tecmo_gameplay_backcourt_assets_init(
    TecmoGameplayBackcourtAssets *assets);
void tecmo_gameplay_backcourt_assets_destroy(
    TecmoGameplayBackcourtAssets *assets);
bool tecmo_gameplay_backcourt_assets_parse(
    TecmoGameplayBackcourtAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *court_orientation,
    size_t court_orientation_size,
    const uint8_t *penalties,
    size_t penalties_size);
bool tecmo_gameplay_backcourt_assets_load(
    TecmoGameplayBackcourtAssets *assets,
    const char *asset_pack_path);
const TecmoGameplayBackcourtSourceSpan *tecmo_gameplay_backcourt_find_source(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtSourceKind kind);

bool tecmo_gameplay_backcourt_state_initialize(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtState *state_out);
bool tecmo_gameplay_backcourt_state_valid(
    const TecmoGameplayBackcourtAssets *assets,
    const TecmoGameplayBackcourtState *state);

/* Exact Bank05 $971F-$9786 detector semantics for the ordinary live-ball
   route. The preceding selector-4 ten-second test at $970B-$971E is retained
   as source evidence but is deliberately outside this API. Failed input or
   state validation leaves both caller outputs unchanged. */
bool tecmo_gameplay_backcourt_step(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtState *state,
    const TecmoGameplayBackcourtStepInput *input,
    bool *violation_out);

bool tecmo_gameplay_backcourt_self_test(const char *asset_pack_path,
                                        char *message,
                                        size_t message_size);

#endif
