#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_shot_resolution.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_shot_resolution.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_SHOT_RESOLUTION_LIFECYCLE_TAG 0x52534754U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    for (unsigned index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool reject(TecmoGameplayShotResolutionAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->routes, 0, sizeof(assets->routes));
    memset(&assets->claimant_thresholds, 0,
           sizeof(assets->claimant_thresholds));
    assets->outcome_flag_mask = 0U;
    assets->route_selector_mask = 0U;
    assets->claimant_other_team_flag_mask = 0U;
    assets->claimant_count = 0U;
    assets->gameplay_core_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGSR-1 rejected");
    return false;
}

void tecmo_gameplay_shot_resolution_init(
    TecmoGameplayShotResolutionAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag =
        TECMO_GAMEPLAY_SHOT_RESOLUTION_LIFECYCLE_TAG;
}

void tecmo_gameplay_shot_resolution_destroy(
    TecmoGameplayShotResolutionAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_SHOT_RESOLUTION_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_shot_resolution_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE &&
           memcmp(payload, "TGSR", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET &&
           read_u16(payload + 32U) ==
               TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT &&
           read_u16(payload + 34U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE &&
           read_u32(payload + 36U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u32(payload + 40U) == TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A32 &&
           read_u64(payload + 48U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A64 &&
           read_u32(payload + 56U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A32 &&
           read_u64(payload + 60U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A64 &&
           bytes_are_zero(payload + 68U, 12U) &&
           read_u16(payload + 80U) == 0x942DU &&
           read_u16(payload + 82U) == 0x9434U &&
           payload[84U] == 1U &&
           bytes_are_zero(
               payload + 85U,
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_HEADER_SIZE - 85U);
}

static bool validate_sources(const uint8_t *payload)
{
    uint32_t prior_end = 0U;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        const TecmoGameplayShotResolutionExpectedSource *expected =
            &tecmo_gameplay_shot_resolution_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE;
        uint32_t end = (uint32_t)expected->cpu_start +
                       expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] !=
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_BANK ||
            record[3U] != 0U ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint_fnv1a32 ||
            read_u64(record + 16U) != expected->fingerprint_fnv1a64 ||
            read_u16(record + 24U) != (uint16_t)index ||
            !bytes_are_zero(record + 26U, 6U) ||
            (index != 0U && expected->cpu_start <= prior_end)) {
            return false;
        }
        prior_end = end;
    }
    return true;
}

static bool validate_semantics(const uint8_t *payload)
{
    const uint8_t *metadata = payload +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET;
    const uint8_t *routes = payload +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET;
    if (memcmp(metadata,
               tecmo_gameplay_shot_resolution_expected_metadata,
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE) != 0 ||
        fnv1a32(metadata,
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A32 ||
        fnv1a64(metadata,
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A64 ||
        memcmp(routes,
               tecmo_gameplay_shot_resolution_expected_routes,
               TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
                   TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE) !=
            0 ||
        fnv1a32(routes,
                TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A32 ||
        fnv1a64(routes,
                TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A64 ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_PADDING_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_PADDING_SIZE)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT; ++index) {
        const uint8_t *record = routes +
            index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE;
        if (record[0U] != index || record[1U] == 0U ||
            record[1U] > TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9 ||
            read_u16(record + 2U) != 0U ||
            read_u16(record + 4U) < 0xA6EEU ||
            read_u16(record + 4U) > 0xA9D9U ||
            read_u16(record + 6U) != index) {
            return false;
        }
    }
    return true;
}

static bool validate_gameplay_core(const uint8_t *gameplay_core,
                                   size_t gameplay_core_size)
{
    return gameplay_core != NULL &&
           gameplay_core_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(gameplay_core, "TGPL", 4U) == 0 &&
           read_u16(gameplay_core + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           read_u16(gameplay_core + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE &&
           read_u32(gameplay_core + 8U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           fnv1a32(gameplay_core, gameplay_core_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
}

bool tecmo_gameplay_shot_resolution_parse(
    TecmoGameplayShotResolutionAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size)
{
    const uint8_t *metadata;
    const uint8_t *route_bytes;
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_SHOT_RESOLUTION_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_shot_resolution_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets, "TGSR-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 ||
        fnv1a64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A64) {
        return reject(assets, "TGSR-1 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload) || !validate_semantics(payload)) {
        return reject(assets, "TGSR-1 source/semantic contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size)) {
        return reject(assets, "TGSR-1 same-pack TGPL-1 dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGSR-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        const TecmoGameplayShotResolutionExpectedSource *expected =
            &tecmo_gameplay_shot_resolution_expected_sources[index];
        TecmoGameplayShotResolutionSourceSpan *source =
            &assets->sources[index];
        source->kind = expected->kind;
        source->bank = TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_BANK;
        source->fixed_bank = false;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint_fnv1a32 = expected->fingerprint_fnv1a32;
        source->fingerprint_fnv1a64 = expected->fingerprint_fnv1a64;
    }
    route_bytes = storage +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT; ++index) {
        const uint8_t *record = route_bytes +
            index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE;
        assets->routes[index].selector = record[0U];
        assets->routes[index].kind =
            (TecmoGameplayShotRimRouteKind)record[1U];
        assets->routes[index].source_target_cpu = read_u16(record + 4U);
    }
    metadata = storage +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET;
    assets->outcome_flag_mask = metadata[0U];
    assets->route_selector_mask = metadata[4U];
    assets->claimant_thresholds.horizontal_min_inclusive =
        (int8_t)metadata[6U];
    assets->claimant_thresholds.horizontal_max_inclusive =
        (int8_t)metadata[7U];
    assets->claimant_thresholds.depth_min_inclusive =
        (int8_t)metadata[8U];
    assets->claimant_thresholds.depth_max_inclusive =
        (int8_t)metadata[9U];
    assets->claimant_thresholds.grounded_ball_altitude_max_inclusive =
        metadata[10U];
    assets->claimant_thresholds.
        airborne_ball_above_claimant_max_inclusive = metadata[11U];
    assets->claimant_other_team_flag_mask = metadata[12U];
    assets->claimant_count = metadata[13U];
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGSR-1 gameplay shot-resolution assetpack");
    return true;
}

bool tecmo_gameplay_shot_resolution_load(
    TecmoGameplayShotResolutionAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_SHOT_RESOLUTION_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_shot_resolution_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGSR-1 gameplay/shot-resolution entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets,
            "TGSR-1 gameplay/core dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_shot_resolution_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    return loaded;
}

const TecmoGameplayShotResolutionSourceSpan *
tecmo_gameplay_shot_resolution_find_source(
    const TecmoGameplayShotResolutionAssets *assets,
    TecmoGameplayShotResolutionSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_shot_resolution_classify_terminal_outcome(
    const TecmoGameplayShotResolutionAssets *assets,
    bool terminal_context,
    uint8_t result_flags,
    TecmoGameplayShotOutcome *outcome)
{
    if (assets == NULL || !assets->available || !terminal_context ||
        outcome == NULL || assets->outcome_flag_mask != 0x80U) {
        return false;
    }
    *outcome = (result_flags & assets->outcome_flag_mask) == 0U
        ? TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE
        : TECMO_GAMEPLAY_SHOT_OUTCOME_MISS;
    return true;
}

bool tecmo_gameplay_shot_resolution_resolve_rim_route(
    const TecmoGameplayShotResolutionAssets *assets,
    uint8_t raw_selector,
    TecmoGameplayShotRimRoute *route)
{
    uint8_t selector;
    if (assets == NULL || !assets->available || route == NULL ||
        assets->route_selector_mask != 0x03U) {
        return false;
    }
    selector = (uint8_t)(raw_selector & assets->route_selector_mask);
    if (selector >= TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT ||
        assets->routes[selector].selector != selector) {
        return false;
    }
    *route = assets->routes[selector];
    return true;
}

bool tecmo_gameplay_shot_resolution_claimant_is_eligible(
    const TecmoGameplayShotResolutionAssets *assets,
    int16_t horizontal_delta,
    int16_t depth_delta,
    uint8_t claimant_altitude,
    uint8_t ball_altitude,
    bool *eligible)
{
    const TecmoGameplayShotClaimantThresholds *thresholds;
    bool altitude_ok;
    if (assets == NULL || !assets->available || eligible == NULL) {
        return false;
    }
    thresholds = &assets->claimant_thresholds;
    if (claimant_altitude == 0U) {
        altitude_ok = ball_altitude <=
            thresholds->grounded_ball_altitude_max_inclusive;
    } else if (ball_altitude < claimant_altitude) {
        altitude_ok = true;
    } else {
        altitude_ok =
            (unsigned)(ball_altitude - claimant_altitude) <=
            thresholds->airborne_ball_above_claimant_max_inclusive;
    }
    *eligible =
        horizontal_delta >= thresholds->horizontal_min_inclusive &&
        horizontal_delta <= thresholds->horizontal_max_inclusive &&
        depth_delta >= thresholds->depth_min_inclusive &&
        depth_delta <= thresholds->depth_max_inclusive &&
        altitude_ok;
    return true;
}

bool tecmo_gameplay_shot_resolution_decide_claimant_settlement(
    const TecmoGameplayShotResolutionAssets *assets,
    bool claimant_is_current_handler,
    TecmoGameplayShotClaimantTeamRelation relation,
    TecmoGameplayShotSettlementDecision *decision)
{
    if (assets == NULL || !assets->available || decision == NULL ||
        (relation != TECMO_GAMEPLAY_SHOT_CLAIMANT_SAME_TEAM &&
         relation != TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM) ||
        (claimant_is_current_handler &&
         relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM) ||
        assets->claimant_other_team_flag_mask != 0x10U) {
        return false;
    }
    memset(decision, 0, sizeof(*decision));
    if (claimant_is_current_handler) return true;
    decision->select_claimant = true;
    if (relation == TECMO_GAMEPLAY_SHOT_CLAIMANT_OTHER_TEAM) {
        decision->replace_other_handler_with_previous = true;
        decision->change_possession = true;
    }
    return true;
}
