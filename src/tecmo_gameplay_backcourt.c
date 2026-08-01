#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_backcourt.h"

#include "asset_pack/tecmo_asset_pack_gameplay_backcourt.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_penalties.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG 0x41424754U
#define BACKCOURT_REV1_ROM_SIZE 393232U
#define BACKCOURT_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t backcourt_rev1_sha256[32] = {
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

static bool reject(TecmoGameplayBackcourtAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->required_global_object_state = 0U;
    assets->frontcourt_progress_mask = 0U;
    assets->violation_selector = 0U;
    assets->orientation_count = 0U;
    assets->orientation_zero_frontcourt_x = 0U;
    assets->orientation_zero_return_low = 0U;
    assets->orientation_one_frontcourt_x = 0U;
    assets->orientation_one_return_low = 0U;
    assets->backcourt_code_offset = 0U;
    assets->backcourt_code_size = 0U;
    assets->court_orientation_fingerprint = 0U;
    assets->penalties_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGBC-1 rejected");
    return false;
}

void tecmo_gameplay_backcourt_assets_init(
    TecmoGameplayBackcourtAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG;
}

void tecmo_gameplay_backcourt_assets_destroy(
    TecmoGameplayBackcourtAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_backcourt_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    const TecmoGameplayBackcourtExpectedSource *source =
        &tecmo_gameplay_backcourt_expected_sources[0U];
    return payload != NULL &&
           payload_size == TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE &&
           memcmp(payload, "TGBC", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           read_u32(payload + 32U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32 &&
           read_u32(payload + 36U) == BACKCOURT_REV1_ROM_SIZE &&
           read_u32(payload + 40U) == BACKCOURT_REV1_ROM_FNV1A32 &&
           memcmp(payload + 44U, backcourt_rev1_sha256,
                  sizeof(backcourt_rev1_sha256)) == 0 &&
           payload[76U] == 0U && payload[77U] == 0x10U &&
           payload[78U] == 2U &&
           payload[79U] == TECMO_GAMEPLAY_BACKCOURT_ORIENTATION_COUNT &&
           read_u16(payload + 80U) == 0x0178U &&
           payload[82U] == 0x0AU && payload[83U] == 0U &&
           read_u16(payload + 84U) == 0x0188U &&
           payload[86U] == 0xF8U && payload[87U] == 0U &&
           read_u16(payload + 88U) == 20U &&
           read_u16(payload + 90U) == 104U &&
           read_u32(payload + 92U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET &&
           read_u32(payload + 96U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE &&
           read_u32(payload + 100U) == source->payload_offset &&
           read_u32(payload + 104U) == source->byte_count &&
           read_u32(payload + 108U) == source->fingerprint &&
           bytes_are_zero(
               payload + 112U,
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_HEADER_SIZE - 112U);
}

static bool validate_source(const uint8_t *payload, size_t payload_size)
{
    const TecmoGameplayBackcourtExpectedSource *source =
        &tecmo_gameplay_backcourt_expected_sources[0U];
    const uint8_t *record = payload +
        TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET;
    uint32_t cpu_end =
        (uint32_t)source->cpu_start + source->byte_count - 1U;
    return read_u16(record) == (uint16_t)source->kind &&
           record[2U] == source->bank &&
           record[3U] == source->fixed_bank &&
           read_u16(record + 4U) == source->cpu_start &&
           read_u16(record + 6U) == (uint16_t)cpu_end &&
           read_u32(record + 8U) == source->byte_count &&
           read_u32(record + 12U) == source->fingerprint &&
           read_u32(record + 16U) == source->payload_offset &&
           bytes_are_zero(record + 20U, 12U) &&
           range_ok(source->payload_offset, source->byte_count,
                    payload_size) &&
           fnv1a32(payload + source->payload_offset,
                   source->byte_count) == source->fingerprint;
}

static bool validate_dependencies(const uint8_t *court_orientation,
                                  size_t court_orientation_size,
                                  const uint8_t *penalties,
                                  size_t penalties_size)
{
    return court_orientation != NULL && penalties != NULL &&
           court_orientation_size ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE &&
           memcmp(court_orientation, "TGOR", 4U) == 0 &&
           read_u16(court_orientation + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_VERSION &&
           read_u16(court_orientation + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_HEADER_SIZE &&
           read_u32(court_orientation + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE &&
           fnv1a32(court_orientation, court_orientation_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 &&
           penalties_size == TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           memcmp(penalties, "TPNL", 4U) == 0 &&
           read_u16(penalties + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VERSION &&
           read_u16(penalties + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_HEADER_SIZE &&
           read_u32(penalties + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           fnv1a32(penalties, penalties_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
}

bool tecmo_gameplay_backcourt_assets_parse(
    TecmoGameplayBackcourtAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *court_orientation,
    size_t court_orientation_size,
    const uint8_t *penalties,
    size_t penalties_size)
{
    const TecmoGameplayBackcourtExpectedSource *expected =
        &tecmo_gameplay_backcourt_expected_sources[0U];
    TecmoGameplayBackcourtSourceSpan *source;
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_backcourt_assets_destroy(assets);
    if (!validate_header(payload, payload_size)) {
        return reject(
            assets, "TGBC-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_FNV1A32 ||
        !validate_source(payload, payload_size) ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_SIZE,
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET -
                (TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET +
                 TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_SIZE)) ||
        memcmp(payload + TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET,
               tecmo_gameplay_backcourt_expected_rules,
               sizeof(tecmo_gameplay_backcourt_expected_rules)) != 0 ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE,
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE -
                (TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET +
                 TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE))) {
        return reject(
            assets, "TGBC-1 canonical source/rules contract rejected");
    }
    if (!validate_dependencies(court_orientation, court_orientation_size,
                               penalties, penalties_size)) {
        return reject(
            assets, "TGBC-1 same-pack TGOR-1/TPNL-1 dependencies rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGBC-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    source = &assets->sources[0U];
    source->kind = expected->kind;
    source->bank = expected->bank;
    source->fixed_bank = expected->fixed_bank != 0U;
    source->cpu_start = expected->cpu_start;
    source->cpu_end = (uint16_t)(
        (uint32_t)expected->cpu_start + expected->byte_count - 1U);
    source->byte_count = expected->byte_count;
    source->fingerprint = expected->fingerprint;
    source->bytes = storage + expected->payload_offset;
    assets->required_global_object_state = storage[76U];
    assets->frontcourt_progress_mask = storage[77U];
    assets->violation_selector = storage[78U];
    assets->orientation_count = storage[79U];
    assets->orientation_zero_frontcourt_x = read_u16(storage + 80U);
    assets->orientation_zero_return_low = storage[82U];
    assets->orientation_one_frontcourt_x = read_u16(storage + 84U);
    assets->orientation_one_return_low = storage[86U];
    assets->backcourt_code_offset = read_u16(storage + 88U);
    assets->backcourt_code_size = read_u16(storage + 90U);
    assets->court_orientation_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32;
    assets->penalties_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGBC-1 exact live backcourt detector assetpack");
    return true;
}

bool tecmo_gameplay_backcourt_assets_load(
    TecmoGameplayBackcourtAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *court_orientation = NULL;
    uint8_t *penalties = NULL;
    uint64_t payload_size = 0U;
    uint64_t court_orientation_size = 0U;
    uint64_t penalties_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_backcourt_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGBC-1 gameplay/backcourt entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ID,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE,
            &court_orientation, &court_orientation_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
            &penalties, &penalties_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(court_orientation);
        tecmo_asset_pack_free(penalties);
        return reject(
            assets,
            "TGBC-1 same-pack TGOR-1/TPNL-1 dependencies missing");
    }
    loaded = tecmo_gameplay_backcourt_assets_parse(
        assets, payload, (size_t)payload_size,
        court_orientation, (size_t)court_orientation_size,
        penalties, (size_t)penalties_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(court_orientation);
    tecmo_asset_pack_free(penalties);
    return loaded;
}

static bool assets_valid(const TecmoGameplayBackcourtAssets *assets)
{
    return assets != NULL &&
           assets->lifecycle_tag ==
               TECMO_GAMEPLAY_BACKCOURT_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_FNV1A32 &&
           assets->required_global_object_state == 0U &&
           assets->frontcourt_progress_mask == 0x10U &&
           assets->violation_selector == 2U &&
           assets->orientation_count ==
               TECMO_GAMEPLAY_BACKCOURT_ORIENTATION_COUNT &&
           assets->orientation_zero_frontcourt_x == 0x0178U &&
           assets->orientation_zero_return_low == 0x0AU &&
           assets->orientation_one_frontcourt_x == 0x0188U &&
           assets->orientation_one_return_low == 0xF8U &&
           assets->backcourt_code_offset == 20U &&
           assets->backcourt_code_size == 104U &&
           assets->court_orientation_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 &&
           assets->penalties_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
}

const TecmoGameplayBackcourtSourceSpan *tecmo_gameplay_backcourt_find_source(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtSourceKind kind)
{
    if (!assets_valid(assets)) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_backcourt_state_valid(
    const TecmoGameplayBackcourtAssets *assets,
    const TecmoGameplayBackcourtState *state)
{
    return assets_valid(assets) && state != NULL &&
           state->contract_tag == TECMO_GAMEPLAY_BACKCOURT_STATE_TAG &&
           state->frontcourt_established <= 1U &&
           bytes_are_zero(state->reserved, sizeof(state->reserved));
}

bool tecmo_gameplay_backcourt_state_initialize(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtState *state_out)
{
    TecmoGameplayBackcourtState initialized;
    if (!assets_valid(assets) || state_out == NULL) return false;
    memset(&initialized, 0, sizeof(initialized));
    initialized.contract_tag = TECMO_GAMEPLAY_BACKCOURT_STATE_TAG;
    if (!tecmo_gameplay_backcourt_state_valid(assets, &initialized)) {
        return false;
    }
    *state_out = initialized;
    return true;
}

bool tecmo_gameplay_backcourt_step(
    const TecmoGameplayBackcourtAssets *assets,
    TecmoGameplayBackcourtState *state,
    const TecmoGameplayBackcourtStepInput *input,
    bool *violation_out)
{
    TecmoGameplayBackcourtState next;
    bool violation = false;
    uint16_t x;
    uint16_t difference;
    if (!tecmo_gameplay_backcourt_state_valid(assets, state) ||
        input == NULL || violation_out == NULL ||
        !tecmo_gameplay_court_coordinate_valid(&input->ball_position) ||
        input->orientation >= assets->orientation_count) {
        return false;
    }
    next = *state;
    if (input->global_object_state ==
        assets->required_global_object_state) {
        x = (uint16_t)input->ball_position.x;
        if (input->orientation == 0U) {
            difference = (uint16_t)(
                x - assets->orientation_zero_frontcourt_x);
            if (x < assets->orientation_zero_frontcourt_x) {
                next.frontcourt_established = 1U;
            } else if (next.frontcourt_established != 0U &&
                       (difference & 0x8000U) == 0U &&
                       (uint8_t)difference >=
                           assets->orientation_zero_return_low) {
                violation = true;
            }
        } else {
            difference = (uint16_t)(
                x - assets->orientation_one_frontcourt_x);
            if (x >= assets->orientation_one_frontcourt_x) {
                next.frontcourt_established = 1U;
            } else if (next.frontcourt_established != 0U &&
                       (difference & 0x8000U) != 0U &&
                       (uint8_t)difference <
                           assets->orientation_one_return_low) {
                violation = true;
            }
        }
    }
    if (!tecmo_gameplay_backcourt_state_valid(assets, &next)) return false;
    *state = next;
    *violation_out = violation;
    return true;
}

bool tecmo_gameplay_backcourt_self_test(const char *asset_pack_path,
                                        char *message,
                                        size_t message_size)
{
    TecmoGameplayBackcourtAssets assets;
    TecmoGameplayBackcourtState state;
    TecmoGameplayBackcourtState before;
    TecmoGameplayBackcourtStepInput input;
    bool violation = true;
    bool violation_before;
    bool ok = false;
    tecmo_gameplay_backcourt_assets_init(&assets);
    if (!tecmo_gameplay_backcourt_assets_load(&assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s", assets.status);
        goto cleanup;
    }
    if (tecmo_gameplay_backcourt_find_source(
            &assets, TECMO_GAMEPLAY_BACKCOURT_SOURCE_DETECTOR) == NULL ||
        !tecmo_gameplay_backcourt_state_initialize(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "TGBC-1 source/state initialization failed");
        goto cleanup;
    }
    memset(&input, 0, sizeof(input));
    input.ball_position.y = 148;
    input.orientation = 0U;
    input.ball_position.x = 376;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation ||
        state.frontcourt_established != 0U) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-zero neutral vector failed");
        goto cleanup;
    }
    input.ball_position.x = 375;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation ||
        state.frontcourt_established != 1U) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-zero progress vector failed");
        goto cleanup;
    }
    input.ball_position.x = 385;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-zero hysteresis vector failed");
        goto cleanup;
    }
    input.ball_position.x = 386;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || !violation) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-zero return vector failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_backcourt_state_initialize(&assets, &state)) {
        goto cleanup;
    }
    input.orientation = 1U;
    input.ball_position.x = 391;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation ||
        state.frontcourt_established != 0U) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-one neutral vector failed");
        goto cleanup;
    }
    input.ball_position.x = 392;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation ||
        state.frontcourt_established != 1U) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-one progress vector failed");
        goto cleanup;
    }
    input.ball_position.x = 384;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-one hysteresis vector failed");
        goto cleanup;
    }
    input.ball_position.x = 383;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || !violation) {
        (void)snprintf(message, message_size,
                       "TGBC-1 orientation-one return vector failed");
        goto cleanup;
    }
    before = state;
    input.global_object_state = 1U;
    input.ball_position.x = 392;
    if (!tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) || violation ||
        memcmp(&state, &before, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGBC-1 global-state gate vector failed");
        goto cleanup;
    }
    input.global_object_state = 0U;
    input.orientation = TECMO_GAMEPLAY_BACKCOURT_ORIENTATION_COUNT;
    before = state;
    violation = true;
    violation_before = violation;
    if (tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) ||
        memcmp(&state, &before, sizeof(state)) != 0 ||
        violation != violation_before) {
        (void)snprintf(message, message_size,
                       "TGBC-1 transactional input rejection failed");
        goto cleanup;
    }
    input.orientation = 0U;
    state.reserved[0U] = 1U;
    before = state;
    violation_before = violation;
    if (tecmo_gameplay_backcourt_step(
            &assets, &state, &input, &violation) ||
        memcmp(&state, &before, sizeof(state)) != 0 ||
        violation != violation_before) {
        (void)snprintf(message, message_size,
                       "TGBC-1 transactional state rejection failed");
        goto cleanup;
    }
    ok = true;
    (void)snprintf(message, message_size,
                   "TGBC-1 parser/detector self-test passed");
cleanup:
    tecmo_gameplay_backcourt_assets_destroy(&assets);
    return ok;
}
