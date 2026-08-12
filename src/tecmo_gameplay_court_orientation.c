#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_court_orientation.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_shot_resolution.h"
#include "tecmo_asset_pack.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG 0x524F4754U

static const uint8_t court_orientation_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    return (uint64_t)read_u32(bytes) |
           ((uint64_t)read_u32(bytes + 4U) << 32U);
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

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayCourtOrientationAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->hoops, 0, sizeof(assets->hoops));
    assets->actor_role_bit = 0U;
    assets->transition_queue_id = 0U;
    memset(assets->screen_id, 0, sizeof(assets->screen_id));
    assets->gameplay_core_fingerprint = 0U;
    assets->shot_resolution_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGOR-1 rejected");
    return false;
}

void tecmo_gameplay_court_orientation_init(
    TecmoGameplayCourtOrientationAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag =
        TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG;
}

void tecmo_gameplay_court_orientation_destroy(
    TecmoGameplayCourtOrientationAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_court_orientation_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    if (payload_size !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE ||
        memcmp(payload, "TGOR", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET ||
        read_u16(payload + 20U) !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT ||
        read_u16(payload + 22U) !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT ||
        read_u32(payload + 24U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(payload + 28U) != TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        read_u32(payload + 32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE ||
        read_u32(payload + 36U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 ||
        read_u32(payload + 40U) != 393232U ||
        read_u32(payload + 44U) != 0x0650F5B0U ||
        memcmp(payload + 48U, court_orientation_rev1_sha256,
               sizeof(court_orientation_rev1_sha256)) != 0 ||
        read_u16(payload + 128U) != 0x00A0U ||
        read_u16(payload + 130U) != 0x0260U ||
        payload[132U] != 0x94U || payload[133U] != 0U ||
        payload[134U] != TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        payload[135U] != 0x10U || payload[136U] != 0x17U ||
        payload[137U] != 0x1BU || payload[138U] != 0x2EU ||
        payload[139U] != 0x07U ||
        !bytes_are_zero(
            payload + 140U,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_HEADER_SIZE -
                140U)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtOrientationExpectedSource *expected =
            &tecmo_gameplay_court_orientation_expected_sources[index];
        const uint8_t *descriptor = payload + 80U + index * 12U;
        if (read_u32(descriptor) != expected->payload_offset ||
            read_u32(descriptor + 4U) != expected->byte_count ||
            read_u32(descriptor + 8U) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtOrientationExpectedSource *expected =
            &tecmo_gameplay_court_orientation_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE;
        uint32_t end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank || record[3U] != 0U ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u32(record + 16U) != expected->payload_offset ||
            !bytes_are_zero(record + 20U, 12U) ||
            end >= 0xC000U ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_padding(const uint8_t *payload)
{
    return bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_SIZE));
}

static bool validate_gameplay_core(const uint8_t *gameplay_core,
                                   size_t gameplay_core_size)
{
    const TecmoGameplayExpectedSource *selector = NULL;
    const TecmoGameplayExpectedSource *ids = NULL;
    static const uint8_t selector_prefix[] = {
        0xADU,0xFCU,0x04U,0x29U,0x80U,0x18U,
        0x2AU,0x2AU,0x8DU,0x58U,0x07U
    };
    static const uint8_t screen_ids[] = {0x1BU,0x2EU};
    if (gameplay_core == NULL ||
        gameplay_core_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        memcmp(gameplay_core, "TGPL", 4U) != 0 ||
        read_u16(gameplay_core + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_VERSION ||
        read_u32(gameplay_core + 8U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        fnv1a32(gameplay_core, gameplay_core_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        if (tecmo_gameplay_expected_sources[index].kind ==
            TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_SELECT) {
            selector = &tecmo_gameplay_expected_sources[index];
        } else if (tecmo_gameplay_expected_sources[index].kind ==
                   TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_IDS) {
            ids = &tecmo_gameplay_expected_sources[index];
        }
    }
    return selector != NULL && ids != NULL &&
           selector->fixed_bank != 0U &&
           selector->cpu_start == 0xE537U &&
           selector->byte_count == 18U &&
           selector->fingerprint == 0xDB9972CEU &&
           ids->fixed_bank != 0U && ids->cpu_start == 0xE699U &&
           ids->byte_count == 2U && ids->fingerprint == 0xA1B4503CU &&
           memcmp(gameplay_core + selector->payload_offset,
                  selector_prefix, sizeof(selector_prefix)) == 0 &&
           memcmp(gameplay_core + ids->payload_offset,
                  screen_ids, sizeof(screen_ids)) == 0;
}

static bool validate_shot_resolution(const uint8_t *shot_resolution,
                                     size_t shot_resolution_size)
{
    const TecmoGameplayShotResolutionExpectedSource *settlement =
        &tecmo_gameplay_shot_resolution_expected_sources[
            TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT - 1U];
    size_t source_index =
        TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT - 1U;
    const uint8_t *record;
    if (shot_resolution == NULL ||
        shot_resolution_size !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE ||
        memcmp(shot_resolution, "TGSR", 4U) != 0 ||
        read_u16(shot_resolution + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_VERSION ||
        read_u32(shot_resolution + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE ||
        fnv1a32(shot_resolution, shot_resolution_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 ||
        settlement->kind !=
            TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT ||
        settlement->cpu_start != 0xB87CU ||
        settlement->byte_count != 122U ||
        settlement->fingerprint_fnv1a32 != 0x9E2F1F28U ||
        settlement->fingerprint_fnv1a64 != 0xC4F3A0BCC17BFCA8ULL) {
        return false;
    }
    record = shot_resolution +
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET +
        source_index *
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE;
    return read_u16(record) ==
               TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT &&
           read_u16(record + 4U) == 0xB87CU &&
           read_u16(record + 6U) == 0xB8F5U &&
           read_u32(record + 8U) == 122U &&
           read_u32(record + 12U) == 0x9E2F1F28U &&
           read_u64(record + 16U) == 0xC4F3A0BCC17BFCA8ULL;
}

bool tecmo_gameplay_court_orientation_parse(
    TecmoGameplayCourtOrientationAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *shot_resolution,
    size_t shot_resolution_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_court_orientation_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets, "TGOR-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 ||
        !validate_sources(payload, payload_size) ||
        !validate_padding(payload)) {
        return reject(assets, "TGOR-1 canonical source contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size) ||
        !validate_shot_resolution(shot_resolution,
                                  shot_resolution_size)) {
        return reject(
            assets,
            "TGOR-1 same-pack TGPL-1/TGSR-4 dependency contract rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) {
        return reject(assets, "TGOR-1 allocation failed");
    }
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtOrientationExpectedSource *expected =
            &tecmo_gameplay_court_orientation_expected_sources[index];
        TecmoGameplayCourtOrientationSourceSpan *source =
            &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = false;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)(
            (uint32_t)expected->cpu_start + expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    assets->hoops[0U].x = (int16_t)read_u16(storage + 128U);
    assets->hoops[1U].x = (int16_t)read_u16(storage + 130U);
    assets->hoops[0U].y = (int16_t)storage[132U];
    assets->hoops[1U].y = (int16_t)storage[132U];
    assets->actor_role_bit = storage[135U];
    assets->transition_queue_id = storage[136U];
    assets->screen_id[0U] = storage[137U];
    assets->screen_id[1U] = storage[138U];
    assets->gameplay_core_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->shot_resolution_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGOR-1 court-orientation assetpack");
    return true;
}

bool tecmo_gameplay_court_orientation_load(
    TecmoGameplayCourtOrientationAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *shot_resolution = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t shot_resolution_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_court_orientation_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ID,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGOR-1 gameplay/court-orientation entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets, "TGOR-1 gameplay/core dependency missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE,
            &shot_resolution, &shot_resolution_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        return reject(
            assets,
            "TGOR-1 gameplay/shot-resolution dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_court_orientation_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        shot_resolution, (size_t)shot_resolution_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(shot_resolution);
    return loaded;
}

static bool assets_valid(
    const TecmoGameplayCourtOrientationAssets *assets)
{
    return assets != NULL &&
           assets->lifecycle_tag ==
               TECMO_GAMEPLAY_COURT_ORIENTATION_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 &&
           tecmo_gameplay_court_coordinate_valid(&assets->hoops[0U]) &&
           tecmo_gameplay_court_coordinate_valid(&assets->hoops[1U]) &&
           assets->hoops[0U].x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X &&
           assets->hoops[1U].x == TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X &&
           assets->hoops[0U].y == TECMO_GAMEPLAY_COURT_HOOP_Y &&
           assets->hoops[1U].y == TECMO_GAMEPLAY_COURT_HOOP_Y &&
           assets->actor_role_bit == 0x10U &&
           assets->transition_queue_id == 0x17U &&
           assets->screen_id[0U] == 0x1BU &&
           assets->screen_id[1U] == 0x2EU &&
           assets->gameplay_core_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           assets->shot_resolution_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32;
}

const TecmoGameplayCourtOrientationSourceSpan *
tecmo_gameplay_court_orientation_find_source(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationSourceKind kind)
{
    if (!assets_valid(assets)) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_court_orientation_target_x(
    const TecmoGameplayCourtOrientationAssets *assets,
    uint8_t direction,
    uint16_t *target_x_out)
{
    if (!assets_valid(assets) ||
        direction >= TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT ||
        target_x_out == NULL) {
        return false;
    }
    *target_x_out = (uint16_t)assets->hoops[direction].x;
    return true;
}

bool tecmo_gameplay_court_orientation_hoop(
    const TecmoGameplayCourtOrientationAssets *assets,
    uint8_t direction,
    TecmoGameplayCourtCoordinate *hoop_out)
{
    if (!assets_valid(assets) ||
        direction >= TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT ||
        hoop_out == NULL) {
        return false;
    }
    *hoop_out = assets->hoops[direction];
    return true;
}

bool tecmo_gameplay_court_orientation_team_hoop(
    const TecmoGameplayCourtOrientationAssets *assets,
    const TecmoGameplayCourtOrientationState *state,
    uint8_t team,
    TecmoGameplayCourtCoordinate *hoop_out)
{
    TecmoGameplayCourtCoordinate hoop;
    uint8_t direction;
    if (!tecmo_gameplay_court_orientation_state_valid(assets, state) ||
        team >= TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT ||
        hoop_out == NULL) {
        return false;
    }
    direction = state->attack_direction;
    if (team != state->tracked_possession_team) direction ^= 1U;
    if (!tecmo_gameplay_court_orientation_hoop(
            assets, direction, &hoop)) {
        return false;
    }
    *hoop_out = hoop;
    return true;
}

bool tecmo_gameplay_court_orientation_state_initialize(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationState *state_out)
{
    TecmoGameplayCourtOrientationState state;
    if (!assets_valid(assets) || state_out == NULL) return false;
    memset(&state, 0, sizeof(state));
    state.contract_tag = TECMO_GAMEPLAY_COURT_ORIENTATION_STATE_TAG;
    state.offensive_hoop = assets->hoops[0U];
    state.attack_direction = 0U;
    state.previous_attack_direction = 0U;
    state.tracked_possession_team =
        TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY;
    *state_out = state;
    return true;
}

bool tecmo_gameplay_court_orientation_state_valid(
    const TecmoGameplayCourtOrientationAssets *assets,
    const TecmoGameplayCourtOrientationState *state)
{
    return assets_valid(assets) && state != NULL &&
           state->contract_tag ==
               TECMO_GAMEPLAY_COURT_ORIENTATION_STATE_TAG &&
           state->attack_direction <
               TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT &&
           state->previous_attack_direction <
               TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT &&
           state->tracked_possession_team <
               TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT &&
           state->reserved == 0U && state->reserved_padding == 0U &&
           tecmo_gameplay_court_coordinate_valid(
               &state->offensive_hoop) &&
           ((state->transition_serial == 0U &&
             state->previous_attack_direction == state->attack_direction) ||
            (state->transition_serial != 0U &&
             state->previous_attack_direction ==
                 (uint8_t)(state->attack_direction ^ 1U))) &&
           state->offensive_hoop.x ==
               assets->hoops[state->attack_direction].x &&
           state->offensive_hoop.y ==
               assets->hoops[state->attack_direction].y;
}

bool tecmo_gameplay_court_orientation_synchronize(
    const TecmoGameplayCourtOrientationAssets *assets,
    TecmoGameplayCourtOrientationState *state,
    uint8_t possession_team)
{
    TecmoGameplayCourtOrientationState candidate;
    if (!tecmo_gameplay_court_orientation_state_valid(assets, state) ||
        possession_team >= TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT) {
        return false;
    }
    if (state->tracked_possession_team == possession_team) return true;
    if (state->transition_serial == UINT32_MAX) return false;
    candidate = *state;
    candidate.previous_attack_direction = candidate.attack_direction;
    candidate.attack_direction ^= 1U;
    candidate.tracked_possession_team = possession_team;
    ++candidate.transition_serial;
    candidate.offensive_hoop =
        assets->hoops[candidate.attack_direction];
    *state = candidate;
    return true;
}

bool tecmo_gameplay_court_orientation_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayCourtOrientationAssets assets;
    TecmoGameplayCourtOrientationState state;
    TecmoGameplayCourtOrientationState unchanged;
    TecmoGameplayCourtOrientationState crossed;
    const TecmoGameplayCourtOrientationSourceSpan *gate;
    const TecmoGameplayCourtOrientationSourceSpan *role;
    TecmoGameplayCourtCoordinate hoop = {(int16_t)0x5A5A,
                                         (int16_t)0x5A5A};
    TecmoGameplayCourtCoordinate unchanged_hoop;
    TecmoGameplayCourtCoordinate team_hoop = {(int16_t)0x5A5A,
                                              (int16_t)0x5A5A};
    TecmoGameplayCourtCoordinate unchanged_team_hoop;
    uint16_t target = 0xA5A5U;
    uint32_t storage_hash;
    bool passed = false;
    tecmo_gameplay_court_orientation_init(&assets);
    memset(&state, 0xA5, sizeof(state));
    unchanged = state;
    if (tecmo_gameplay_court_orientation_state_initialize(
            &assets, &state) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0 ||
        asset_pack_path == NULL ||
        !tecmo_gameplay_court_orientation_load(&assets, asset_pack_path) ||
        !tecmo_gameplay_court_orientation_load(&assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s",
                       asset_pack_path != NULL ? assets.status
                                               : "PACK path required");
        goto cleanup;
    }
    gate = tecmo_gameplay_court_orientation_find_source(
        &assets,
        TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_POSSESSION_GATE_AND_SWAP);
    role = tecmo_gameplay_court_orientation_find_source(
        &assets,
        TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_ACTOR_ROLE_TOGGLE);
    if (gate == NULL || gate->bank != 5U || gate->fixed_bank ||
        gate->cpu_start != 0x8FADU || gate->cpu_end != 0x8FE7U ||
        gate->byte_count != 59U ||
        gate->fingerprint != 0x7C94E5EAU ||
        role == NULL || role->cpu_start != 0x9042U ||
        role->cpu_end != 0x9053U || role->byte_count != 18U ||
        role->fingerprint != 0xCE6C9466U ||
        tecmo_gameplay_court_orientation_find_source(
            &assets,
            (TecmoGameplayCourtOrientationSourceKind)0) != NULL ||
        tecmo_gameplay_court_orientation_find_source(
            &assets,
            (TecmoGameplayCourtOrientationSourceKind)5) != NULL ||
        !tecmo_gameplay_court_orientation_target_x(
            &assets, 0U, &target) || target != 0x00A0U ||
        !tecmo_gameplay_court_orientation_target_x(
            &assets, 1U, &target) || target != 0x0260U ||
        !tecmo_gameplay_court_orientation_hoop(
            &assets, 0U, &hoop) ||
        hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_court_orientation_hoop(
            &assets, 1U, &hoop) ||
        hoop.x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        (void)snprintf(message, message_size,
                       "TGOR-1 source/target contract failed");
        goto cleanup;
    }
    storage_hash = fnv1a32(assets.storage, assets.storage_size);
    if (!tecmo_gameplay_court_orientation_state_initialize(
            &assets, &state) ||
        !tecmo_gameplay_court_orientation_state_valid(&assets, &state) ||
        state.attack_direction != 0U ||
        state.previous_attack_direction != 0U ||
        state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY ||
        state.transition_serial != 0U ||
        state.offensive_hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        state.offensive_hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        team_hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        team_hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        (void)snprintf(message, message_size,
                       "TGOR-1 initial state contract failed");
        goto cleanup;
    }
    unchanged = state;
    if (!tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0 ||
        !tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME) ||
        state.attack_direction != 1U ||
        state.previous_attack_direction != 0U ||
        state.tracked_possession_team !=
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME ||
        state.transition_serial != 1U ||
        state.offensive_hoop.x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        state.offensive_hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        !tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY) ||
        state.attack_direction != 0U ||
        state.previous_attack_direction != 1U ||
        state.transition_serial != 2U ||
        state.offensive_hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        state.offensive_hoop.y != TECMO_GAMEPLAY_COURT_HOOP_Y) {
        (void)snprintf(message, message_size,
                       "TGOR-1 transition contract failed");
        goto cleanup;
    }
    /* The native state contract does not encode a team enum into a direction.
       Exercise the crossed case explicitly: Away may own direction 1 and
       Home then owns direction 0. */
    crossed = state;
    crossed.attack_direction = 1U;
    crossed.previous_attack_direction = 0U;
    crossed.transition_serial = 1U;
    crossed.tracked_possession_team =
        TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY;
    crossed.offensive_hoop = assets.hoops[1U];
    if (!tecmo_gameplay_court_orientation_state_valid(
            &assets, &crossed) ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &crossed,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &assets, &crossed,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME, &team_hoop) ||
        team_hoop.x != TECMO_GAMEPLAY_COURT_LEFT_HOOP_X) {
        (void)snprintf(message, message_size,
                       "TGOR-1 crossed team/direction matrix failed");
        goto cleanup;
    }
    unchanged = state;
    target = 0xA5A5U;
    hoop.x = (int16_t)0x5A5A;
    hoop.y = (int16_t)0x5A5A;
    unchanged_hoop = hoop;
    team_hoop.x = (int16_t)0x6B6B;
    team_hoop.y = (int16_t)0x6B6B;
    unchanged_team_hoop = team_hoop;
    if (tecmo_gameplay_court_orientation_synchronize(
            &assets, &state, 2U) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0 ||
        tecmo_gameplay_court_orientation_target_x(
            &assets, 2U, &target) || target != 0xA5A5U ||
        tecmo_gameplay_court_orientation_target_x(
            &assets, 0U, NULL) ||
        tecmo_gameplay_court_orientation_hoop(
            &assets, 2U, &hoop) ||
        memcmp(&hoop, &unchanged_hoop, sizeof(hoop)) != 0 ||
        tecmo_gameplay_court_orientation_hoop(
            &assets, 0U, NULL) ||
        tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state, 2U, &team_hoop) ||
        memcmp(&team_hoop, &unchanged_team_hoop,
               sizeof(team_hoop)) != 0 ||
        tecmo_gameplay_court_orientation_team_hoop(
            &assets, &state, 0U, NULL) ||
        tecmo_gameplay_court_orientation_state_initialize(
            &assets, NULL)) {
        (void)snprintf(message, message_size,
                       "TGOR-1 invalid input mutated output");
        goto cleanup;
    }
    state.offensive_hoop.y =
        (int16_t)(TECMO_GAMEPLAY_COURT_HOOP_Y - 1);
    unchanged = state;
    if (tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGOR-1 corrupt hoop state contract failed");
        goto cleanup;
    }
    state.offensive_hoop = assets.hoops[0U];
    state.transition_serial = UINT32_MAX;
    unchanged = state;
    if (tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGOR-1 serial-overflow contract failed");
        goto cleanup;
    }
    state.transition_serial = 2U;
    state.reserved_padding = 1U;
    unchanged = state;
    if (tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGOR-1 reserved state contract failed");
        goto cleanup;
    }
    state.reserved_padding = 0U;
    state.contract_tag ^= 1U;
    unchanged = state;
    if (tecmo_gameplay_court_orientation_synchronize(
            &assets, &state,
            TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_HOME) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0 ||
        fnv1a32(assets.storage, assets.storage_size) != storage_hash) {
        (void)snprintf(message, message_size,
                       "TGOR-1 corrupt state or storage contract failed");
        goto cleanup;
    }
    (void)snprintf(
        message, message_size,
        "TGOR-1 court orientation passed: targets=00A0/0260 y=94 "
        "transitions=2 direction=0");
    passed = true;

cleanup:
    tecmo_gameplay_court_orientation_destroy(&assets);
    tecmo_gameplay_court_orientation_destroy(&assets);
    return passed;
}
