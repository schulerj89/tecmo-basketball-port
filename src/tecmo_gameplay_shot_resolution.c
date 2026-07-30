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
    assets->point_shot_flags_mask = 0U;
    assets->point_y_min_inclusive = 0U;
    assets->point_y_max_exclusive = 0U;
    assets->point_orientation_count = 0U;
    memset(assets->point_arc_boundary, 0,
           sizeof(assets->point_arc_boundary));
    memset(&assets->rim_rattle, 0, sizeof(assets->rim_rattle));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGSR-3 rejected");
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
           read_u32(payload + 68U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_OFFSET &&
           read_u32(payload + 72U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_SIZE &&
           read_u32(payload + 76U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A32 &&
           read_u64(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A64 &&
           read_u16(payload + 88U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_CLASSIFIER_CPU &&
           read_u16(payload + 90U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_CLASSIFIER_END_CPU &&
           payload[92U] == 0x03U &&
           payload[93U] == 0x5BU &&
           payload[94U] == 0xD7U &&
           payload[95U] == 2U &&
           read_u16(payload + 96U) == 0x942DU &&
           read_u16(payload + 98U) == 0x9434U &&
           payload[100U] == 1U &&
           bytes_are_zero(
               payload + 101U,
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_HEADER_SIZE - 101U);
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
    const uint8_t *point_arc = payload +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_OFFSET;
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
        fnv1a32(
            point_arc,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A32 ||
        fnv1a64(
            point_arc,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A64 ||
        point_arc[0U] != 0xF1U ||
        point_arc[5U] != 0xFDU ||
        point_arc[6U] != 0x00U ||
        point_arc[60U] != 0x28U ||
        point_arc[61U] != 0x27U ||
        point_arc[99U] != 0x0EU ||
        point_arc[100U] != 0x0DU ||
        point_arc[123U] != 0xDEU ||
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
        return reject(assets, "TGSR-3 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 ||
        fnv1a64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A64) {
        return reject(assets, "TGSR-3 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload) || !validate_semantics(payload)) {
        return reject(assets, "TGSR-3 source/semantic contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size)) {
        return reject(assets, "TGSR-3 same-pack TGPL-1 dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGSR-3 allocation failed");
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
    assets->point_shot_flags_mask = storage[92U];
    assets->point_y_min_inclusive = storage[93U];
    assets->point_y_max_exclusive = storage[94U];
    assets->point_orientation_count = storage[95U];
    memcpy(
        assets->point_arc_boundary,
        storage + TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_OFFSET,
        sizeof(assets->point_arc_boundary));
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
    assets->rim_rattle.object_state = metadata[29U];
    assets->rim_rattle.orientation_start_x[0U] =
        read_u16(metadata + 30U);
    assets->rim_rattle.orientation_start_x[1U] =
        read_u16(metadata + 32U);
    assets->rim_rattle.start_y = metadata[34U];
    assets->rim_rattle.horizontal_velocity_q6 =
        read_u16(metadata + 35U);
    assets->rim_rattle.altitude = metadata[37U];
    assets->rim_rattle.pass_timer_updates = metadata[38U];
    assets->rim_rattle.pass_source_mask = metadata[39U];
    assets->rim_rattle.pass_source_bias = metadata[40U];
    assets->rim_rattle.pass_animation_shift = metadata[41U];
    assets->rim_rattle.animation_low_mask = metadata[42U];
    assets->rim_rattle.repeat_dmc_length = metadata[43U];
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RIM_RATTLE_RENDER_SCRIPT_COUNT;
         ++index) {
        assets->rim_rattle.render_script_addresses[index] =
            read_u16(metadata + 44U + index * 2U);
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT;
         ++index) {
        assets->rim_rattle.exit_render_script_addresses[index] =
            read_u16(metadata + 60U + index * 2U);
    }
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGSR-3 gameplay shot-resolution assetpack");
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
            "TGSR-3 gameplay/shot-resolution entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets,
            "TGSR-3 gameplay/core dependency missing or wrong-sized");
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

static uint8_t subtract_with_6502_carry(
    uint8_t lhs,
    uint8_t rhs,
    bool *carry)
{
    const uint16_t subtrahend =
        (uint16_t)rhs + (*carry ? 0U : 1U);
    const uint8_t result = (uint8_t)((uint16_t)lhs - subtrahend);
    *carry = (uint16_t)lhs >= subtrahend;
    return result;
}

bool tecmo_gameplay_shot_resolution_classify_point_value(
    const TecmoGameplayShotResolutionAssets *assets,
    uint16_t world_x,
    uint8_t world_y,
    uint8_t orientation,
    uint8_t shot_flags,
    uint8_t *point_value)
{
    uint8_t index;
    uint8_t high_adjust;
    uint8_t ignored;
    bool carry;
    if (assets == NULL || !assets->available || point_value == NULL ||
        orientation >= assets->point_orientation_count ||
        assets->point_orientation_count != 2U ||
        assets->point_shot_flags_mask != 0x03U ||
        assets->point_y_min_inclusive != 0x5BU ||
        assets->point_y_max_exclusive != 0xD7U) {
        return false;
    }
    if ((shot_flags & assets->point_shot_flags_mask) != 0U) {
        *point_value = 1U;
        return true;
    }

    *point_value = 2U;
    if (world_y < assets->point_y_min_inclusive ||
        world_y >= assets->point_y_max_exclusive) {
        *point_value = 3U;
        return true;
    }
    index = (uint8_t)(world_y - assets->point_y_min_inclusive);
    if (index >= TECMO_GAMEPLAY_SHOT_POINT_ARC_COUNT) return false;

    if (orientation == 0U) {
        /* $B9B1-$B9D7: subtract the table byte and then the derived high
           adjustment, preserving the exact 6502 borrow into the high byte. */
        high_adjust =
            index >= 0x06U && index < 0x6DU ? 1U : 0U;
        carry = true;
        ignored = subtract_with_6502_carry(
            (uint8_t)world_x, assets->point_arc_boundary[index], &carry);
        (void)ignored;
        ignored = subtract_with_6502_carry(
            (uint8_t)(world_x >> 8U), high_adjust, &carry);
        (void)ignored;
        if (carry) *point_value = 3U;
    } else {
        uint8_t boundary_low;
        /* $B9D8-$B9FD first forms $FF-table[index], then compares world X
           against the mirrored high-byte adjustment through the same borrow. */
        high_adjust =
            index >= 0x06U && index < 0x6EU ? 1U : 2U;
        carry = true;
        boundary_low = subtract_with_6502_carry(
            0xFFU, assets->point_arc_boundary[index], &carry);
        carry = true;
        ignored = subtract_with_6502_carry(
            (uint8_t)world_x, boundary_low, &carry);
        (void)ignored;
        ignored = subtract_with_6502_carry(
            (uint8_t)(world_x >> 8U), high_adjust, &carry);
        (void)ignored;
        if (!carry) *point_value = 3U;
    }
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

bool tecmo_gameplay_shot_rim_rattle_begin(
    const TecmoGameplayShotResolutionAssets *assets,
    TecmoGameplayShotRimRattle *rattle,
    uint8_t orientation,
    uint8_t pass_source,
    uint8_t animation_phase,
    int16_t incoming_horizontal_velocity_q6,
    int16_t incoming_vertical_velocity_q6)
{
    const TecmoGameplayShotRimRattleContract *contract;
    uint8_t pass_count;
    if (assets == NULL || !assets->available || rattle == NULL ||
        orientation >= TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT) {
        return false;
    }
    contract = &assets->rim_rattle;
    pass_count = (uint8_t)(
        (pass_source & contract->pass_source_mask) +
        contract->pass_source_bias);
    if (contract->object_state != 0x15U ||
        contract->horizontal_velocity_q6 != 0x0040U ||
        contract->pass_timer_updates != 4U ||
        contract->pass_animation_shift != 4U ||
        pass_count == 0U || pass_count > 4U) {
        return false;
    }
    memset(rattle, 0, sizeof(*rattle));
    rattle->active = true;
    rattle->object_state = contract->object_state;
    rattle->orientation = orientation;
    rattle->timer_remaining = contract->pass_timer_updates;
    rattle->passes_remaining = pass_count;
    rattle->animation_phase = (uint8_t)(
        (uint8_t)(pass_count << contract->pass_animation_shift) |
        (animation_phase & contract->animation_low_mask));
    rattle->altitude = contract->altitude;
    rattle->x = (int16_t)contract->orientation_start_x[orientation];
    rattle->y = (int16_t)contract->start_y;
    rattle->saved_horizontal_velocity_q6 =
        incoming_horizontal_velocity_q6;
    rattle->saved_vertical_velocity_q6 =
        incoming_vertical_velocity_q6;
    rattle->horizontal_velocity_q6 =
        incoming_horizontal_velocity_q6 < 0
            ? (int16_t)contract->horizontal_velocity_q6
            : -(int16_t)contract->horizontal_velocity_q6;
    rattle->vertical_velocity_q6 = 0;
    rattle->render_script_address =
        contract->render_script_addresses[(size_t)orientation * 4U];
    return true;
}

bool tecmo_gameplay_shot_rim_rattle_step(
    const TecmoGameplayShotResolutionAssets *assets,
    TecmoGameplayShotRimRattle *rattle,
    bool *repeat_dmc,
    bool *completed)
{
    const TecmoGameplayShotRimRattleContract *contract;
    size_t render_index;
    uint8_t animation_high;
    if (repeat_dmc != NULL) *repeat_dmc = false;
    if (completed != NULL) *completed = false;
    if (assets == NULL || !assets->available || rattle == NULL ||
        repeat_dmc == NULL || completed == NULL || !rattle->active ||
        rattle->complete ||
        rattle->orientation >=
            TECMO_GAMEPLAY_SHOT_RIM_RATTLE_ORIENTATION_COUNT ||
        rattle->object_state != assets->rim_rattle.object_state ||
        rattle->timer_remaining == 0U ||
        rattle->timer_remaining >
            assets->rim_rattle.pass_timer_updates ||
        rattle->passes_remaining == 0U ||
        rattle->passes_remaining > 4U ||
        (uint8_t)(rattle->animation_phase >>
                  assets->rim_rattle.pass_animation_shift) !=
            rattle->passes_remaining ||
        rattle->altitude != assets->rim_rattle.altitude ||
        rattle->y != (int16_t)assets->rim_rattle.start_y ||
        rattle->vertical_velocity_q6 != 0 ||
        rattle->x <
            (int16_t)(assets->rim_rattle.orientation_start_x[
                          rattle->orientation] - 4U) ||
        rattle->x >
            (int16_t)(assets->rim_rattle.orientation_start_x[
                          rattle->orientation] + 4U)) {
        return false;
    }
    contract = &assets->rim_rattle;
    if (rattle->horizontal_velocity_q6 ==
            (int16_t)contract->horizontal_velocity_q6) {
        ++rattle->x;
    } else if (rattle->horizontal_velocity_q6 ==
                   -(int16_t)contract->horizontal_velocity_q6) {
        --rattle->x;
    } else {
        return false;
    }
    --rattle->timer_remaining;
    if (rattle->timer_remaining == 0U) {
        animation_high =
            (uint8_t)(rattle->animation_phase & 0xF0U);
        if (animation_high < 0x10U) return false;
        animation_high = (uint8_t)(animation_high - 0x10U);
        rattle->animation_phase = (uint8_t)(
            animation_high |
            (rattle->animation_phase & contract->animation_low_mask));
        --rattle->passes_remaining;
        if (rattle->passes_remaining == 0U) {
            rattle->active = false;
            rattle->complete = true;
            rattle->object_state = 0U;
            rattle->horizontal_velocity_q6 =
                rattle->saved_horizontal_velocity_q6;
            rattle->vertical_velocity_q6 =
                rattle->saved_vertical_velocity_q6;
            rattle->render_script_address =
                contract->exit_render_script_addresses[
                    rattle->orientation];
            *completed = true;
            return true;
        }
        rattle->timer_remaining = contract->pass_timer_updates;
        rattle->horizontal_velocity_q6 =
            (int16_t)-rattle->horizontal_velocity_q6;
        rattle->vertical_velocity_q6 = 0;
        rattle->render_script_address =
            contract->render_script_addresses[
                (size_t)rattle->orientation * 4U];
        *repeat_dmc = true;
        return true;
    }

    if (rattle->orientation == 0U) {
        render_index = rattle->horizontal_velocity_q6 < 0
            ? rattle->timer_remaining
            : (size_t)(3U - rattle->timer_remaining);
    } else {
        render_index = rattle->horizontal_velocity_q6 < 0
            ? (size_t)(7U - rattle->timer_remaining)
            : (size_t)(rattle->timer_remaining + 4U);
    }
    if (render_index >=
            TECMO_GAMEPLAY_SHOT_RIM_RATTLE_RENDER_SCRIPT_COUNT) {
        return false;
    }
    rattle->render_script_address =
        contract->render_script_addresses[render_index];
    return true;
}
