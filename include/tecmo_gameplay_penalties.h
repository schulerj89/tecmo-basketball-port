#ifndef TECMO_GAMEPLAY_PENALTIES_H
#define TECMO_GAMEPLAY_PENALTIES_H

#include "tecmo_gameplay_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT 8U
#define TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT 3U
#define TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT 7U
#define TECMO_GAMEPLAY_PENALTY_ACTOR_SLOT_COUNT 10U

typedef enum TecmoGameplayPenaltySourceKind {
    TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_COMMIT = 1,
    TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_RULES_PRESENTATION = 2,
    TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_PRESENTATION_SCRIPT = 3,
    TECMO_GAMEPLAY_PENALTY_SOURCE_RELEASE_GATE = 4,
    TECMO_GAMEPLAY_PENALTY_SOURCE_VIOLATION_SCRIPT = 5,
    TECMO_GAMEPLAY_PENALTY_SOURCE_RELEASE_INPUT_HELPER = 6,
    TECMO_GAMEPLAY_PENALTY_SOURCE_VIOLATION_SELECTOR_PRESENTATION = 7,
    TECMO_GAMEPLAY_PENALTY_SOURCE_PRESENTATION_CUE_REQUEST = 8
} TecmoGameplayPenaltySourceKind;

typedef enum TecmoGameplayFoulClass {
    TECMO_GAMEPLAY_FOUL_CLASS_INVALID = 0,
    TECMO_GAMEPLAY_FOUL_CLASS_CHARGING = 1,
    TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING = 2,
    TECMO_GAMEPLAY_FOUL_CLASS_PUSHING = 3
} TecmoGameplayFoulClass;

typedef enum TecmoGameplayPenaltyPeriodKind {
    TECMO_GAMEPLAY_PENALTY_PERIOD_REGULATION = 0,
    TECMO_GAMEPLAY_PENALTY_PERIOD_OVERTIME = 1,
    TECMO_GAMEPLAY_PENALTY_PERIOD_KIND_COUNT
} TecmoGameplayPenaltyPeriodKind;

typedef enum TecmoGameplayPenaltyPresentationKind {
    TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL = 1,
    TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION = 2
} TecmoGameplayPenaltyPresentationKind;

typedef struct TecmoGameplayPenaltySourceSpan {
    TecmoGameplayPenaltySourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
} TecmoGameplayPenaltySourceSpan;

typedef struct TecmoGameplayPenaltyPresentation {
    TecmoGameplayPenaltyPresentationKind kind;
    uint8_t selector_min;
    uint8_t selector_max;
    /* Delay is internal to the shared screen-$22 routine before its C009 request. */
    uint8_t presentation_sfx_id;
    uint8_t live_restart_sfx_id;
    uint8_t live_restart_music_id;
    uint16_t lead_in_frames;
    uint16_t maximum_wait_frames;
    uint16_t presentation_sfx_delay_frames;
    uint8_t release_button_mask;
    uint8_t controller_count;
} TecmoGameplayPenaltyPresentation;

/*
 * Every input is an explicit ROM-semantic byte. `saved_route` is the route
 * copied before foul commit. `current_route` and `contact_selector` are the
 * two bytes used by the defensive BLOCKING/PUSHING selector. The API does not
 * infer contact, collision, possession, or shooting state.
 */
typedef struct TecmoGameplayPenaltyContext {
    uint8_t foul_actor;
    uint8_t offensive_primary_actor;
    uint8_t saved_route;
    uint8_t current_route;
    uint8_t contact_selector;
    uint8_t individual_fouls;
    uint8_t team_fouls;
    TecmoGameplayPenaltyPeriodKind period_kind;
} TecmoGameplayPenaltyContext;

typedef struct TecmoGameplayPenaltyResult {
    TecmoGameplayFoulClass foul_class;
    bool offensive_foul;
    bool turnover;
    bool team_in_bonus;
    bool fouled_out;
    bool attempts_from_bonus;
    uint8_t individual_foul_delta;
    uint8_t team_foul_delta;
    uint8_t individual_fouls_after;
    uint8_t team_fouls_after;
    uint8_t free_throw_attempts;
} TecmoGameplayPenaltyResult;

typedef struct TecmoGameplayPenaltyAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayPenaltySourceSpan
        sources[TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT];
    const uint8_t *rules;
    const uint8_t *foul_classes;
    const uint8_t *attempt_selectors;
    const uint8_t *violations;
    const uint8_t *presentations;
    uint32_t gameplay_core_fingerprint;
    uint32_t gameplay_sfx_fingerprint;
} TecmoGameplayPenaltyAssets;

void tecmo_gameplay_penalties_init(TecmoGameplayPenaltyAssets *assets);
void tecmo_gameplay_penalties_destroy(TecmoGameplayPenaltyAssets *assets);

bool tecmo_gameplay_penalties_parse(
    TecmoGameplayPenaltyAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_sfx,
    size_t gameplay_sfx_size);

bool tecmo_gameplay_penalties_load(TecmoGameplayPenaltyAssets *assets,
                                   const char *asset_pack_path);

const TecmoGameplayPenaltySourceSpan *tecmo_gameplay_penalties_find_source(
    const TecmoGameplayPenaltyAssets *assets,
    TecmoGameplayPenaltySourceKind kind);

bool tecmo_gameplay_penalties_classify(
    const TecmoGameplayPenaltyAssets *assets,
    const TecmoGameplayPenaltyContext *context,
    TecmoGameplayPenaltyResult *result);

bool tecmo_gameplay_penalties_get_presentation(
    const TecmoGameplayPenaltyAssets *assets,
    TecmoGameplayPenaltyPresentationKind kind,
    uint8_t selector,
    TecmoGameplayPenaltyPresentation *presentation);

bool tecmo_gameplay_penalties_get_violation(
    const TecmoGameplayPenaltyAssets *assets,
    uint8_t selector,
    TecmoGameplayViolation *violation,
    TecmoGameplayPenaltyPresentation *presentation);

bool tecmo_gameplay_penalties_self_test(const char *asset_pack_path,
                                        char *message,
                                        size_t message_size);

#endif
