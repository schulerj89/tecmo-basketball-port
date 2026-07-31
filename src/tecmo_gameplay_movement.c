#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_movement.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define TECMO_GAMEPLAY_MOVEMENT_LIFECYCLE_TAG 0x4D475431U
#define MOVEMENT_REV1_ROM_SIZE 393232U
#define MOVEMENT_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t movement_rev1_sha256[32] = {
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

static bool reject(TecmoGameplayMovementAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->speed_adjustment, 0,
           sizeof(assets->speed_adjustment));
    memset(assets->direction_map, 0, sizeof(assets->direction_map));
    assets->available = false;
    assets->gameplay_core_fingerprint = 0U;
    assets->gameplay_camera_fingerprint = 0U;
    assets->team_data_fingerprint = 0U;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGMO-1 rejected");
    return false;
}

void tecmo_gameplay_movement_assets_init(
    TecmoGameplayMovementAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_MOVEMENT_LIFECYCLE_TAG;
}

void tecmo_gameplay_movement_assets_destroy(
    TecmoGameplayMovementAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_MOVEMENT_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_movement_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    static const uint8_t speed_adjustments[3] = {0x05U,0xFFU,0xFAU};
    static const uint8_t direction_map[16] = {
        0U,0U,1U,0U,2U,3U,4U,0U,5U,6U,7U,0U,1U,2U,4U,5U
    };
    if (payload_size != TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE ||
        memcmp(payload, "TGMO", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCES_OFFSET ||
        read_u32(payload + 20U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(payload + 24U) != TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        read_u32(payload + 28U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        read_u32(payload + 32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32 ||
        read_u32(payload + 36U) != TECMO_ASSET_PACK_TEAM_DATA_SIZE ||
        read_u32(payload + 40U) != TECMO_ASSET_PACK_TEAM_DATA_FNV1A32 ||
        read_u32(payload + 44U) != MOVEMENT_REV1_ROM_SIZE ||
        read_u32(payload + 48U) != MOVEMENT_REV1_ROM_FNV1A32 ||
        memcmp(payload + 52U, movement_rev1_sha256,
               sizeof(movement_rev1_sha256)) != 0 ||
        !bytes_are_zero(payload + 84U, 4U) ||
        memcmp(payload +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SPEEDS_OFFSET,
               speed_adjustments, sizeof(speed_adjustments)) != 0 ||
        payload[179U] != 6U || payload[180U] != 8U ||
        payload[181U] != 8U || payload[182U] != 3U ||
        payload[183U] != 5U || payload[184U] != 0x4AU ||
        payload[185U] != 0xECU || payload[186U] != 3U ||
        payload[187U] != 4U || read_u16(payload + 188U) != 0x00DFU ||
        read_u16(payload + 190U) != 0x0220U ||
        read_u16(payload + 192U) != 0x00EFU ||
        read_u16(payload + 194U) != 0x0028U ||
        payload[196U] != 6U ||
        !bytes_are_zero(payload + 197U, 3U) ||
        memcmp(payload +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DIRECTION_MAP_OFFSET,
               direction_map, sizeof(direction_map)) != 0 ||
        !bytes_are_zero(payload + 216U,
                        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HEADER_SIZE -
                            216U)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *expected =
            &tecmo_gameplay_movement_expected_sources[index];
        const uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DESCRIPTOR_STRIDE;
        if (read_u32(descriptor) != expected->payload_offset ||
            read_u32(descriptor + 4U) != expected->byte_count ||
            read_u32(descriptor + 8U) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *expected =
            &tecmo_gameplay_movement_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCE_STRIDE;
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u32(record + 16U) != expected->payload_offset ||
            !bytes_are_zero(record + 20U, 12U) ||
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
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_SIZE)) &&
           bytes_are_zero(
               payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_SIZE)) &&
           bytes_are_zero(
               payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_SIZE)) &&
           bytes_are_zero(
               payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE));
}

static bool validate_dependencies(const uint8_t *gameplay_core,
                                  size_t gameplay_core_size,
                                  const uint8_t *gameplay_camera,
                                  size_t gameplay_camera_size,
                                  const uint8_t *team_data,
                                  size_t team_data_size,
                                  const uint8_t *movement_payload)
{
    return gameplay_core != NULL &&
           gameplay_core_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(gameplay_core, "TGPL", 4U) == 0 &&
           read_u16(gameplay_core + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           read_u32(gameplay_core + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           fnv1a32(gameplay_core, gameplay_core_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           gameplay_camera != NULL &&
           gameplay_camera_size ==
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE &&
           memcmp(gameplay_camera, "TGCP", 4U) == 0 &&
           read_u16(gameplay_camera + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_VERSION &&
           fnv1a32(gameplay_camera, gameplay_camera_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32 &&
           team_data != NULL &&
           team_data_size == TECMO_ASSET_PACK_TEAM_DATA_SIZE &&
           memcmp(team_data, "TTDT", 4U) == 0 &&
           read_u16(team_data + 4U) == 1U &&
           read_u16(team_data + 6U) ==
               TECMO_ASSET_PACK_TEAM_DATA_HEADER_SIZE &&
           read_u32(team_data + 56U) ==
               TECMO_ASSET_PACK_TEAM_DATA_SIZE &&
           fnv1a32(team_data, team_data_size) ==
               TECMO_ASSET_PACK_TEAM_DATA_FNV1A32 &&
           memcmp(
               gameplay_camera +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET,
               movement_payload +
                   TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET,
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE) == 0;
}

bool tecmo_gameplay_movement_assets_parse(
    TecmoGameplayMovementAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_camera,
    size_t gameplay_camera_size,
    const uint8_t *team_data,
    size_t team_data_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_MOVEMENT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_movement_assets_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(
            assets, "TGMO-1 header/size/reserved contract rejected");
    }
    if (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 == 0U ||
        fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32) {
        return reject(
            assets, "TGMO-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload)) {
        return reject(assets, "TGMO-1 source/padding contract rejected");
    }
    if (!validate_dependencies(
            gameplay_core, gameplay_core_size,
            gameplay_camera, gameplay_camera_size,
            team_data, team_data_size, payload)) {
        return reject(
            assets,
            "TGMO-1 TGPL-1/TGCP-2/TTDT-1 dependency contract rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGMO-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *expected =
            &tecmo_gameplay_movement_expected_sources[index];
        TecmoGameplayMovementSourceSpan *source = &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)(
            (uint32_t)expected->cpu_start + expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT; ++index) {
        assets->speed_adjustment[index] = (int8_t)storage[
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SPEEDS_OFFSET + index];
    }
    memcpy(assets->direction_map,
           storage +
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DIRECTION_MAP_OFFSET,
           sizeof(assets->direction_map));
    assets->condition_fresh_high_nibble = storage[179U];
    assets->minimum_movement_amount = storage[180U];
    assets->animation_period = storage[181U];
    assets->animation_delay_high_nibble = storage[182U];
    assets->animation_transition_high_nibble = storage[183U];
    assets->upper_y_gate = storage[184U];
    assets->lower_y_gate = storage[185U];
    assets->diagonal_numerator = storage[186U];
    assets->diagonal_denominator = storage[187U];
    assets->left_boundary_base = read_u16(storage + 188U);
    assets->right_boundary_base = read_u16(storage + 190U);
    assets->gameplay_core_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->gameplay_camera_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32;
    assets->team_data_fingerprint = TECMO_ASSET_PACK_TEAM_DATA_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGMO-1 controlled-player movement assetpack");
    return true;
}

bool tecmo_gameplay_movement_assets_load(
    TecmoGameplayMovementAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *gameplay_camera = NULL;
    uint8_t *team_data = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t gameplay_camera_size = 0U;
    uint64_t team_data_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_MOVEMENT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_movement_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGMO-1 gameplay/movement entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ID,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE,
            &gameplay_camera, &gameplay_camera_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_TEAM_DATA_ID,
            TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            &team_data, &team_data_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        tecmo_asset_pack_free(gameplay_camera);
        tecmo_asset_pack_free(team_data);
        return reject(
            assets,
            "TGMO-1 same-pack dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_movement_assets_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        gameplay_camera, (size_t)gameplay_camera_size,
        team_data, (size_t)team_data_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(gameplay_camera);
    tecmo_asset_pack_free(team_data);
    return loaded;
}

const TecmoGameplayMovementSourceSpan *
tecmo_gameplay_movement_find_source(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_movement_input_valid(uint8_t direction_bits)
{
    switch (direction_bits) {
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT:
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT:
        return true;
    default:
        return false;
    }
}

static bool animation_phase_valid(
    const TecmoGameplayMovementAssets *assets,
    uint8_t phase,
    uint8_t action_state)
{
    uint8_t high = (uint8_t)(phase >> 4U);
    uint8_t low = (uint8_t)(phase & 0x0FU);
    if (low >= assets->animation_period ||
        high > assets->animation_transition_high_nibble) {
        return false;
    }
    /* The direction-transition countdown starts at $50 with low phase zero.
       Once it reaches the ordinary $3x countdown, the low phase may advance.
       Neutral transitions start directly at $30. */
    if (high > assets->animation_delay_high_nibble && low != 0U) {
        return false;
    }
    return action_state != TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL ||
           high <= assets->animation_delay_high_nibble;
}

bool tecmo_gameplay_movement_state_valid(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementState *state)
{
    if (assets == NULL || !assets->available || state == NULL ||
        state->contract_tag != TECMO_GAMEPLAY_MOVEMENT_STATE_TAG ||
        !tecmo_gameplay_court_coordinate_valid(&state->position) ||
        !tecmo_gameplay_movement_input_valid(state->action_state) ||
        state->direction >= 8U ||
        state->fractional_accumulator >= 16U ||
        !animation_phase_valid(
            assets, state->animation_phase, state->action_state)) {
        return false;
    }
    return state->action_state == TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL ||
           state->direction == assets->direction_map[state->action_state];
}

bool tecmo_gameplay_movement_state_initialize(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    const TecmoGameplayCourtCoordinate *position,
    uint8_t initial_direction)
{
    TecmoGameplayMovementState initial;
    if (assets == NULL || !assets->available || state == NULL ||
        position == NULL ||
        !tecmo_gameplay_court_coordinate_valid(position) ||
        initial_direction >= 8U) {
        return false;
    }
    memset(&initial, 0, sizeof(initial));
    initial.contract_tag = TECMO_GAMEPLAY_MOVEMENT_STATE_TAG;
    initial.position = *position;
    initial.direction = initial_direction;
    initial.animation_phase = (uint8_t)(
        assets->animation_delay_high_nibble << 4U);
    if (!tecmo_gameplay_movement_state_valid(assets, &initial)) {
        return false;
    }
    *state = initial;
    return true;
}

bool tecmo_gameplay_movement_clamp(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementClampInput *input,
    TecmoGameplayMovementClampResult *result_out)
{
    TecmoGameplayMovementClampResult result;
    int32_t boundary;
    uint16_t page;
    if (assets == NULL || !assets->available || input == NULL ||
        result_out == NULL ||
        !tecmo_gameplay_court_coordinate_valid(&input->position) ||
        input->actor_direction >= 8U) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.position = input->position;
    result.violation_latched = input->violation_latched;

    /* Fixed $F10C dispatcher exclusions for the primary selected actor. */
    if (input->primary_selected_actor &&
        (input->global_object_state == 4U ||
         input->actor_action_state == 0x0FU ||
         input->actor_action_state == 0x10U)) {
        result.skipped = true;
        *result_out = result;
        return true;
    }
    /* Selected-actor exemption inside $F127/$F16D. Secondary actors do not
       take this branch. */
    if (input->primary_selected_actor &&
        input->global_object_state != 7U &&
        input->global_object_state != 8U &&
        (input->movement_flags & 0x08U) != 0U) {
        result.skipped = true;
        *result_out = result;
        return true;
    }

    page = (uint16_t)result.position.x >> 8U;
    if (page == 0U) {
        boundary = (int32_t)assets->left_boundary_base -
                   (uint16_t)result.position.y / 2U;
        if (result.position.x < boundary) {
            result.position.x = (int16_t)boundary;
            result.clamped = true;
        }
    } else if (page == 2U) {
        boundary = (int32_t)assets->right_boundary_base +
                   (uint16_t)result.position.y / 2U;
        if (result.position.x > boundary) {
            result.position.x = (int16_t)boundary;
            result.clamped = true;
        }
    }
    if (result.clamped && input->primary_selected_actor &&
        input->actor_direction != 5U &&
        input->global_object_state == 0U &&
        (input->movement_flags & 0x08U) == 0U) {
        result.violation_latched = true;
    }
    if (!tecmo_gameplay_court_coordinate_valid(&result.position)) {
        return false;
    }
    *result_out = result;
    return true;
}

static uint8_t animation_advance(
    const TecmoGameplayMovementAssets *assets,
    uint8_t phase)
{
    if (phase >= 0x10U) {
        return (uint8_t)((((phase & 0xF0U) - 0x10U) & 0xF0U) |
                         (phase & 0x0FU));
    }
    phase = (uint8_t)((phase & 0x0FU) + 1U);
    if (phase >= assets->animation_period) phase = 0U;
    return (uint8_t)(
        (assets->animation_delay_high_nibble << 4U) | phase);
}

static bool movement_amount(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementStepInput *input,
    uint8_t *amount_out)
{
    int adjusted;
    int amount;
    if (input->speed_value >= TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT ||
        input->condition > 0x64U || amount_out == NULL) {
        return false;
    }
    adjusted = (int)input->player_movement_rating +
               assets->speed_adjustment[input->speed_value];
    if (adjusted < assets->minimum_movement_amount || adjusted > 0xFF) {
        return false;
    }
    amount = adjusted + (int)(input->condition >> 4U) -
             assets->condition_fresh_high_nibble;
    if (amount < assets->minimum_movement_amount) {
        amount = assets->minimum_movement_amount;
    }
    /* The original loop at $87BA is reachable-safe only while the
       condition-adjusted amount does not exceed the stored rating. */
    if (amount > adjusted || amount > 0xFF) return false;
    *amount_out = (uint8_t)amount;
    return true;
}

static bool action_is_diagonal(uint8_t action)
{
    return action == TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT ||
           action == TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT ||
           action == TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT ||
           action == TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT;
}

static bool movement_apply_delta(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    uint8_t amount)
{
    uint16_t accumulated;
    uint8_t delta;
    int32_t next_x = state->position.x;
    int32_t next_y = state->position.y;
    if (action_is_diagonal(state->action_state)) {
        /* $87D7 computes A - floor(A/4), not a rounded C expression. */
        amount = (uint8_t)(amount - (amount >> 2U));
    }
    accumulated = (uint16_t)amount + state->fractional_accumulator;
    state->fractional_accumulator = (uint8_t)(accumulated & 0x0FU);
    delta = (uint8_t)(accumulated >> 4U);

    switch (state->action_state) {
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT:
        next_x += delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT:
        next_x -= delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN:
        if (next_y < assets->lower_y_gate) next_y += delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT:
        if (next_y < assets->lower_y_gate) next_y += delta;
        next_x += delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT:
        if (next_y < assets->lower_y_gate) next_y += delta;
        next_x -= delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP:
        if (next_y >= assets->upper_y_gate) next_y -= delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT:
        if (next_y >= assets->upper_y_gate) next_y -= delta;
        next_x += delta;
        break;
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT:
        if (next_y >= assets->upper_y_gate) next_y -= delta;
        next_x -= delta;
        break;
    default:
        return false;
    }
    if (next_x < INT16_MIN || next_x > INT16_MAX ||
        next_y < INT16_MIN || next_y > INT16_MAX) {
        return false;
    }
    state->position.x = (int16_t)next_x;
    state->position.y = (int16_t)next_y;
    state->animation_phase = animation_advance(
        assets, state->animation_phase);
    return true;
}

bool tecmo_gameplay_movement_step(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    const TecmoGameplayMovementStepInput *input)
{
    TecmoGameplayMovementState next;
    TecmoGameplayMovementClampInput clamp_input;
    TecmoGameplayMovementClampResult clamp_result;
    uint8_t amount;
    if (!tecmo_gameplay_movement_state_valid(assets, state) ||
        input == NULL ||
        !tecmo_gameplay_movement_input_valid(input->held_direction_bits) ||
        !movement_amount(assets, input, &amount)) {
        return false;
    }
    next = *state;

    if (next.action_state == TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
        next.animation_phase = animation_advance(
            assets, next.animation_phase);
    } else if (input->held_direction_bits == next.action_state &&
               !movement_apply_delta(assets, &next, amount)) {
        return false;
    }

    /* $8E58 updates the action after the current action handler, producing
       the original one-update latency when direction changes. */
    if (input->held_direction_bits != next.action_state) {
        next.action_state = input->held_direction_bits;
        if (next.action_state == TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
            next.animation_phase = (uint8_t)(
                assets->animation_delay_high_nibble << 4U);
        } else {
            next.direction = assets->direction_map[next.action_state];
            next.animation_phase = (uint8_t)(
                assets->animation_transition_high_nibble << 4U);
        }
    }

    memset(&clamp_input, 0, sizeof(clamp_input));
    clamp_input.position = next.position;
    clamp_input.global_object_state = input->global_object_state;
    clamp_input.actor_action_state = next.action_state;
    clamp_input.movement_flags = input->movement_flags;
    clamp_input.actor_direction = next.direction;
    clamp_input.primary_selected_actor = input->primary_selected_actor;
    clamp_input.violation_latched = next.boundary_violation_latched;
    if (!tecmo_gameplay_movement_clamp(
            assets, &clamp_input, &clamp_result)) {
        return false;
    }
    next.position = clamp_result.position;
    next.boundary_violation_latched = clamp_result.violation_latched;
    if (!tecmo_gameplay_movement_state_valid(assets, &next)) return false;
    *state = next;
    return true;
}

bool tecmo_gameplay_movement_pose_index(
    const TecmoGameplayMovementAssets *assets,
    const TecmoGameplayMovementState *state,
    bool alternate_pose_half,
    uint16_t *pose_index_out)
{
    const TecmoGameplayMovementSourceSpan *source;
    size_t table_index;
    uint16_t offset;
    uint16_t pose_index;
    if (!tecmo_gameplay_movement_state_valid(assets, state) ||
        pose_index_out == NULL) {
        return false;
    }
    source = tecmo_gameplay_movement_find_source(
        assets, TECMO_GAMEPLAY_MOVEMENT_SOURCE_INPUT_AND_POSE);
    if (source == NULL || source->byte_count < 271U) return false;
    table_index = state->direction + (alternate_pose_half ? 8U : 0U);
    offset = (uint16_t)(
        source->bytes[(0x8F47U - 0x8E58U) + table_index] |
        ((uint16_t)source->bytes[
             (0x8F57U - 0x8E58U) + table_index] << 8U));
    if ((offset & 1U) != 0U) return false;
    pose_index = (uint16_t)(offset / 2U +
                            (state->animation_phase & 0x0FU));
    if (pose_index >= 1179U) return false;
    *pose_index_out = pose_index;
    return true;
}

static bool movement_states_equal(const TecmoGameplayMovementState *left,
                                  const TecmoGameplayMovementState *right)
{
    return memcmp(left, right, sizeof(*left)) == 0;
}

static bool malformed_state_rejected_unchanged(
    const TecmoGameplayMovementAssets *assets,
    TecmoGameplayMovementState *state,
    const TecmoGameplayMovementStepInput *input)
{
    TecmoGameplayMovementState before = *state;
    return !tecmo_gameplay_movement_state_valid(assets, state) &&
           !tecmo_gameplay_movement_step(assets, state, input) &&
           movement_states_equal(state, &before);
}

bool tecmo_gameplay_movement_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayMovementAssets assets;
    TecmoGameplayCourtCoordinate start = {384, 148};
    TecmoGameplayMovementState state;
    TecmoGameplayMovementState before;
    TecmoGameplayMovementStepInput input;
    TecmoGameplayMovementClampInput clamp_input;
    TecmoGameplayMovementClampResult clamp_result;
    TecmoGameplayMovementClampResult clamp_before;
    TecmoGameplayMovementState malformed;
    TecmoGameplayMovementState edge;
    uint16_t pose_index;
    tecmo_gameplay_movement_assets_init(&assets);
    if (!tecmo_gameplay_movement_assets_load(&assets, asset_pack_path) ||
        !tecmo_gameplay_movement_state_initialize(
            &assets, &state, &start, 0U)) {
        (void)snprintf(message, message_size, "%s", assets.status);
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.held_direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
    input.player_movement_rating = 20U;
    input.condition = 100U;
    input.speed_value = 1U;
    input.primary_selected_actor = true;
    if (!tecmo_gameplay_movement_step(&assets, &state, &input) ||
        state.position.x != 384 || state.position.y != 148 ||
        state.action_state != TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT ||
        state.direction != 0U || state.fractional_accumulator != 0U ||
        state.animation_phase != 0x50U ||
        !tecmo_gameplay_movement_step(&assets, &state, &input) ||
        state.position.x != 385 || state.fractional_accumulator != 3U ||
        state.animation_phase != 0x40U ||
        !tecmo_gameplay_movement_step(&assets, &state, &input) ||
        state.position.x != 386 || state.fractional_accumulator != 6U ||
        state.animation_phase != 0x30U ||
        !tecmo_gameplay_movement_pose_index(
            &assets, &state, false, &pose_index) ||
        pose_index != 181U) {
        (void)snprintf(message, message_size,
                       "TGMO-1 cardinal/latency vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }

    before = state;
    input.held_direction_bits = 3U;
    if (tecmo_gameplay_movement_step(&assets, &state, &input) ||
        !movement_states_equal(&state, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 transactional input rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    input.held_direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
    input.condition = 101U;
    if (tecmo_gameplay_movement_step(&assets, &state, &input) ||
        !movement_states_equal(&state, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 condition rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    input.condition = 100U;
    input.speed_value = TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT;
    if (tecmo_gameplay_movement_step(&assets, &state, &input) ||
        !movement_states_equal(&state, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 speed-range rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    input.speed_value = 0U;
    input.player_movement_rating = 0xFFU;
    if (tecmo_gameplay_movement_step(&assets, &state, &input) ||
        !movement_states_equal(&state, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 adjusted-rating overflow rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    input.speed_value = 2U;
    input.player_movement_rating = 0U;
    if (tecmo_gameplay_movement_step(&assets, &state, &input) ||
        !movement_states_equal(&state, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 unreachable rating rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    input.speed_value = 1U;
    input.player_movement_rating = 20U;

    malformed = state;
    malformed.contract_tag ^= 1U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.position.x =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1);
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.action_state = 3U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.direction = 8U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.direction = 1U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.fractional_accumulator = 16U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.animation_phase = 0x38U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.animation_phase = 0x60U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.animation_phase = 0x41U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;
    malformed = state;
    malformed.action_state = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    malformed.animation_phase = 0x40U;
    if (!malformed_state_rejected_unchanged(
            &assets, &malformed, &input)) goto malformed_state_failure;

    start.x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X;
    start.y = 148;
    if (!tecmo_gameplay_movement_state_initialize(
            &assets, &edge, &start, 0U)) {
        goto malformed_state_failure;
    }
    edge.action_state = TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT;
    edge.animation_phase = 0x30U;
    edge.fractional_accumulator = 15U;
    before = edge;
    if (tecmo_gameplay_movement_step(&assets, &edge, &input) ||
        !movement_states_equal(&edge, &before)) {
        (void)snprintf(message, message_size,
                       "TGMO-1 coordinate-overflow rollback failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }

    memset(&clamp_input, 0, sizeof(clamp_input));
    clamp_input.position.x = 100;
    clamp_input.position.y = 148;
    clamp_input.actor_action_state = 1U;
    clamp_input.actor_direction = 0U;
    clamp_input.primary_selected_actor = true;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        !clamp_result.clamped || clamp_result.skipped ||
        clamp_result.position.x != 149 ||
        !clamp_result.violation_latched) {
        (void)snprintf(message, message_size,
                       "TGMO-1 ordinary clamp vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 4U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        !clamp_result.skipped || clamp_result.clamped ||
        clamp_result.position.x != 100) {
        (void)snprintf(message, message_size,
                       "TGMO-1 dispatcher exception vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 0U;
    clamp_input.actor_action_state = 0x0FU;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        !clamp_result.skipped || clamp_result.clamped ||
        clamp_result.position.x != 100) {
        (void)snprintf(message, message_size,
                       "TGMO-1 action-$0F dispatcher vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.actor_action_state = 0x10U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        !clamp_result.skipped || clamp_result.clamped ||
        clamp_result.position.x != 100) {
        (void)snprintf(message, message_size,
                       "TGMO-1 action-$10 dispatcher vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.actor_action_state = 1U;
    clamp_input.movement_flags = 0x08U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        !clamp_result.skipped || clamp_result.clamped ||
        clamp_result.position.x != 100) {
        (void)snprintf(message, message_size,
                       "TGMO-1 flags-bit-3 exemption vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 7U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        clamp_result.skipped || !clamp_result.clamped ||
        clamp_result.position.x != 149 ||
        clamp_result.violation_latched) {
        (void)snprintf(message, message_size,
                       "TGMO-1 state-7 clamp override vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 8U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        clamp_result.skipped || !clamp_result.clamped ||
        clamp_result.position.x != 149 ||
        clamp_result.violation_latched) {
        (void)snprintf(message, message_size,
                       "TGMO-1 state-8 clamp override vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 4U;
    clamp_input.primary_selected_actor = false;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        clamp_result.skipped || !clamp_result.clamped ||
        clamp_result.position.x != 149 ||
        clamp_result.violation_latched) {
        (void)snprintf(message, message_size,
                       "TGMO-1 secondary-actor dispatcher vector failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    clamp_input.global_object_state = 0U;
    clamp_input.movement_flags = 0U;
    clamp_input.primary_selected_actor = true;
    clamp_input.actor_direction = 5U;
    if (!tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        clamp_result.skipped || !clamp_result.clamped ||
        clamp_result.position.x != 149 ||
        clamp_result.violation_latched) {
        (void)snprintf(message, message_size,
                       "TGMO-1 direction-5 latch exclusion failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }
    memset(&clamp_before, 0xA5, sizeof(clamp_before));
    clamp_result = clamp_before;
    clamp_input.position.x = -1;
    if (tecmo_gameplay_movement_clamp(
            &assets, &clamp_input, &clamp_result) ||
        memcmp(&clamp_result, &clamp_before, sizeof(clamp_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGMO-1 transactional clamp rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&assets);
        return false;
    }

    tecmo_gameplay_movement_assets_destroy(&assets);
    (void)snprintf(
        message, message_size,
        "TGMO-1 movement passed: latency=1 normal-rating20-q4=19 clamp-x=149");
    return true;

malformed_state_failure:
    (void)snprintf(message, message_size,
                   "TGMO-1 malformed-state transactional rejection failed.");
    tecmo_gameplay_movement_assets_destroy(&assets);
    return false;
}
