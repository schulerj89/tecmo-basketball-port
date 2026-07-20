#ifndef TECMO_GAMEPLAY_SHOT_RESOLUTION_H
#define TECMO_GAMEPLAY_SHOT_RESOLUTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT 4U
#define TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT 4U

typedef enum TecmoGameplayShotResolutionSourceKind {
    TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_OUTCOME_CALCULATION = 1,
    TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_RIM_ROUTE_DISPATCH = 2,
    TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SCAN = 3,
    TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT = 4
} TecmoGameplayShotResolutionSourceKind;

typedef enum TecmoGameplayShotOutcome {
    TECMO_GAMEPLAY_SHOT_OUTCOME_UNKNOWN = 0,
    TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE = 1,
    TECMO_GAMEPLAY_SHOT_OUTCOME_MISS = 2
} TecmoGameplayShotOutcome;

/* These numeric route identities are intentionally address-bound. The
   imported dispatch proves their selection, but not a more specific semantic
   name for every route. */
typedef enum TecmoGameplayShotRimRouteKind {
    TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708 = 1,
    TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9 = 2,
    TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9 = 3
} TecmoGameplayShotRimRouteKind;

typedef enum TecmoGameplayShotClaimantTeamRelation {
    TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM = 0,
    TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM = 1
} TecmoGameplayShotClaimantTeamRelation;

typedef struct TecmoGameplayShotResolutionSourceSpan {
    TecmoGameplayShotResolutionSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
} TecmoGameplayShotResolutionSourceSpan;

typedef struct TecmoGameplayShotRimRoute {
    uint8_t selector;
    TecmoGameplayShotRimRouteKind kind;
    uint16_t source_target_cpu;
} TecmoGameplayShotRimRoute;

typedef struct TecmoGameplayShotClaimantThresholds {
    int8_t horizontal_min_inclusive;
    int8_t horizontal_max_inclusive;
    int8_t depth_min_inclusive;
    int8_t depth_max_inclusive;
    uint8_t grounded_ball_altitude_max_inclusive;
    uint8_t airborne_ball_above_claimant_max_inclusive;
} TecmoGameplayShotClaimantThresholds;

typedef struct TecmoGameplayShotSettlementDecision {
    bool select_claimant;
    bool replace_other_handler_with_previous;
    bool change_possession;
} TecmoGameplayShotSettlementDecision;

typedef struct TecmoGameplayShotResolutionAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayShotResolutionSourceSpan
        sources[TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT];
    TecmoGameplayShotRimRoute
        routes[TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT];
    TecmoGameplayShotClaimantThresholds claimant_thresholds;
    uint8_t outcome_flag_mask;
    uint8_t route_selector_mask;
    uint8_t claimant_other_team_flag_mask;
    uint8_t claimant_count;
    uint32_t gameplay_core_fingerprint;
} TecmoGameplayShotResolutionAssets;

void tecmo_gameplay_shot_resolution_init(
    TecmoGameplayShotResolutionAssets *assets);
void tecmo_gameplay_shot_resolution_destroy(
    TecmoGameplayShotResolutionAssets *assets);

bool tecmo_gameplay_shot_resolution_parse(
    TecmoGameplayShotResolutionAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size);
bool tecmo_gameplay_shot_resolution_load(
    TecmoGameplayShotResolutionAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayShotResolutionSourceSpan *
tecmo_gameplay_shot_resolution_find_source(
    const TecmoGameplayShotResolutionAssets *assets,
    TecmoGameplayShotResolutionSourceKind kind);

/* $942D clears bit 7 and $9434 sets it, but either helper may be reached from
   a nonterminal animation path. Classification is therefore deliberately
   unavailable unless the caller has already established terminal context. */
bool tecmo_gameplay_shot_resolution_classify_terminal_outcome(
    const TecmoGameplayShotResolutionAssets *assets,
    bool terminal_context,
    uint8_t result_flags,
    TecmoGameplayShotOutcome *outcome);

bool tecmo_gameplay_shot_resolution_resolve_rim_route(
    const TecmoGameplayShotResolutionAssets *assets,
    uint8_t raw_selector,
    TecmoGameplayShotRimRoute *route);

bool tecmo_gameplay_shot_resolution_claimant_is_eligible(
    const TecmoGameplayShotResolutionAssets *assets,
    int16_t horizontal_delta,
    int16_t depth_delta,
    uint8_t claimant_altitude,
    uint8_t ball_altitude,
    bool *eligible);

/* This reports only the proven handler/possession decision. It does not name
   the claimant action as a rebound, steal, block, or recovery. */
bool tecmo_gameplay_shot_resolution_decide_claimant_settlement(
    const TecmoGameplayShotResolutionAssets *assets,
    bool claimant_is_current_handler,
    TecmoGameplayShotClaimantTeamRelation relation,
    TecmoGameplayShotSettlementDecision *decision);

#endif
