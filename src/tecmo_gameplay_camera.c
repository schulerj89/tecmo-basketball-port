#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_camera.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_CAMERA_LIFECYCLE_TAG 0x50434754U
#define GAMEPLAY_CAMERA_REV1_ROM_SIZE 393232U
#define GAMEPLAY_CAMERA_REV1_ROM_FNV1A32 0x0650F5B0U
#define GAMEPLAY_CAMERA_SETTLE_LIMIT 1024U

static const uint8_t gameplay_camera_rev1_sha256[32] = {
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

static bool reject(TecmoGameplayCameraAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->initialize_routine = NULL;
    assets->stream_columns = NULL;
    assets->attribute_quadrants = NULL;
    assets->follow_routine = NULL;
    assets->forced_settle_routine = NULL;
    assets->actor_projection_routine = NULL;
    assets->gameplay_core_fingerprint = 0U;
    assets->gameplay_court_fingerprint = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGCP-1 rejected");
    return false;
}

void tecmo_gameplay_camera_assets_init(
    TecmoGameplayCameraAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_CAMERA_LIFECYCLE_TAG;
}

void tecmo_gameplay_camera_assets_destroy(
    TecmoGameplayCameraAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CAMERA_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_camera_assets_init(assets);
}

static bool validate_header(const uint8_t *payload,
                            size_t payload_size)
{
    if (payload_size != TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        memcmp(payload, "TGCP", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET ||
        read_u32(payload + 20U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(payload + 24U) != TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        read_u32(payload + 28U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE ||
        read_u32(payload + 32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32 ||
        read_u32(payload + 36U) != GAMEPLAY_CAMERA_REV1_ROM_SIZE ||
        read_u32(payload + 40U) != GAMEPLAY_CAMERA_REV1_ROM_FNV1A32 ||
        memcmp(payload + 44U, gameplay_camera_rev1_sha256,
               sizeof(gameplay_camera_rev1_sha256)) != 0 ||
        read_u16(payload + 148U) != 0x0100U ||
        payload[150U] != 0U || payload[151U] != 0U ||
        payload[152U] != 0U || payload[153U] != 0U ||
        payload[154U] != 0U || payload[155U] != 0x20U ||
        payload[156U] != 2U || payload[157U] != 7U ||
        read_u16(payload + 158U) != GAMEPLAY_CAMERA_SETTLE_LIMIT ||
        payload[160U] != 16U || payload[161U] != 8U ||
        payload[162U] != 0U || payload[163U] != 1U ||
        memcmp(payload + 164U, "\x50\xD8\x20\xA0\xE8\x04", 6U) != 0 ||
        !bytes_are_zero(
            payload + 170U,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_SIZE - 170U)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        const uint8_t *descriptor = payload + 76U + index * 12U;
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
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE;
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
            expected->fixed_bank == 0U ||
            expected->bank != 7U ||
            expected->cpu_start < 0xC000U ||
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
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_SIZE));
}

static bool validate_opcode_relationships(const uint8_t *payload)
{
    static const uint8_t initialize_exact[26] = {
        0xA2U,0x00U,0x8EU,0x00U,0x03U,0x8EU,0x01U,0x03U,
        0x8EU,0x02U,0x03U,0x8EU,0x03U,0x03U,0x86U,0x3BU,
        0x86U,0x00U,0xE8U,0x86U,0x01U,0xA9U,0x20U,0x85U,
        0x38U,0x60U
    };
    static const uint8_t settle_loop[] = {
        0xADU,0x00U,0x03U,0x48U,0x20U,0x6EU,0xE1U,0x68U,
        0xCDU,0x00U,0x03U,0xF0U,0x06U,0x20U,0x62U,0xCEU,
        0x4CU,0x6CU,0xEBU
    };
    static const uint8_t projection_subtract[] = {
        0xB5U,0x73U,0x38U,0xE5U,0x00U,0x85U,0x09U,
        0xB5U,0xE8U,0xE5U,0x01U,0x85U,0x0AU,0xD0U
    };
    const uint8_t *initialize = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET;
    const uint8_t *settle = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET;
    const uint8_t *projection = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET;
    return memcmp(initialize, initialize_exact,
                  sizeof(initialize_exact)) == 0 &&
           memcmp(settle + 29U, settle_loop, sizeof(settle_loop)) == 0 &&
           memcmp(projection + 12U, projection_subtract,
                  sizeof(projection_subtract)) == 0;
}

static bool validate_gameplay_core(const uint8_t *payload,
                                   size_t payload_size)
{
    static const size_t required_sources[2] = {7U, 8U};
    if (payload == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        memcmp(payload, "TGPL", 4U) != 0 ||
        read_u16(payload + 4U) != TECMO_ASSET_PACK_GAMEPLAY_VERSION ||
        read_u16(payload + 6U) != TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE ||
        read_u32(payload + 8U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u16(payload + 14U) != TECMO_GAMEPLAY_ASSET_SOURCE_COUNT ||
        read_u16(payload + 18U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE ||
        read_u32(payload + 24U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET ||
        read_u32(payload + 108U) !=
            TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_OFFSET ||
        read_u32(payload + 112U) !=
            TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_SIZE ||
        fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) {
        return false;
    }
    if (tecmo_gameplay_expected_sources[7U].kind !=
            TECMO_GAMEPLAY_SOURCE_ACTOR_RENDERER ||
        tecmo_gameplay_expected_sources[7U].cpu_start != 0xD413U ||
        tecmo_gameplay_expected_sources[7U].fingerprint != 0xD487D107U ||
        tecmo_gameplay_expected_sources[8U].kind !=
            TECMO_GAMEPLAY_SOURCE_ACTOR_RENDER_STAGING ||
        tecmo_gameplay_expected_sources[8U].cpu_start != 0xF1F2U ||
        tecmo_gameplay_expected_sources[8U].fingerprint != 0xA93E123BU) {
        return false;
    }
    for (size_t index = 0U; index < 2U; ++index) {
        const TecmoGameplayExpectedSource *source =
            &tecmo_gameplay_expected_sources[required_sources[index]];
        const uint8_t *record =
            payload + TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET +
            required_sources[index] *
                TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE;
        uint32_t cpu_end =
            (uint32_t)source->cpu_start + source->byte_count - 1U;
        if (read_u16(record) != (uint16_t)source->kind ||
            record[2U] != source->bank ||
            record[3U] != source->fixed_bank ||
            read_u16(record + 4U) != source->cpu_start ||
            read_u16(record + 6U) != 0U ||
            read_u32(record + 8U) != source->byte_count ||
            read_u32(record + 12U) != source->payload_offset ||
            read_u32(record + 16U) != source->fingerprint ||
            read_u16(record + 20U) != (uint16_t)cpu_end ||
            !bytes_are_zero(record + 22U, 10U) ||
            !range_ok(source->payload_offset, source->byte_count,
                      payload_size) ||
            fnv1a32(payload + source->payload_offset,
                    source->byte_count) != source->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_gameplay_court(const uint8_t *payload,
                                    size_t payload_size)
{
    static const size_t required_sources[4] = {4U, 5U, 6U, 8U};
    if (payload == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE ||
        memcmp(payload, "TGCT", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET ||
        read_u16(payload + 56U) != 15U ||
        read_u16(payload + 58U) != 16U ||
        fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32) {
        return false;
    }
    if (tecmo_gameplay_court_expected_sources[4U].kind !=
            TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT ||
        tecmo_gameplay_court_expected_sources[4U].cpu_start != 0x93C6U ||
        tecmo_gameplay_court_expected_sources[4U].byte_count != 1440U ||
        tecmo_gameplay_court_expected_sources[4U].fingerprint !=
            0x578BFD90U ||
        tecmo_gameplay_court_expected_sources[5U].kind !=
            TECMO_GAMEPLAY_COURT_SOURCE_MACRO_TILES ||
        tecmo_gameplay_court_expected_sources[5U].fingerprint !=
            0xA6CBF6F7U ||
        tecmo_gameplay_court_expected_sources[6U].kind !=
            TECMO_GAMEPLAY_COURT_SOURCE_MACRO_ATTRIBUTES ||
        tecmo_gameplay_court_expected_sources[6U].fingerprint !=
            0x2BE5CD2FU ||
        tecmo_gameplay_court_expected_sources[8U].kind !=
            TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT_LOOP ||
        tecmo_gameplay_court_expected_sources[8U].fingerprint !=
            0x2779098EU) {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index) {
        const TecmoGameplayCourtExpectedSource *source =
            &tecmo_gameplay_court_expected_sources[required_sources[index]];
        const uint8_t *record =
            payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET +
            required_sources[index] *
                TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE;
        uint32_t cpu_end =
            (uint32_t)source->cpu_start + source->byte_count - 1U;
        if (read_u16(record) != (uint16_t)source->kind ||
            record[2U] != source->bank ||
            record[3U] != source->fixed_bank ||
            read_u16(record + 4U) != source->cpu_start ||
            read_u16(record + 6U) != 0U ||
            read_u32(record + 8U) != source->byte_count ||
            read_u32(record + 12U) != source->payload_offset ||
            read_u32(record + 16U) != source->fingerprint ||
            read_u16(record + 20U) != (uint16_t)cpu_end ||
            !bytes_are_zero(record + 22U, 10U) ||
            !range_ok(source->payload_offset, source->byte_count,
                      payload_size) ||
            fnv1a32(payload + source->payload_offset,
                    source->byte_count) != source->fingerprint) {
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_camera_assets_parse(
    TecmoGameplayCameraAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_court,
    size_t gameplay_court_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CAMERA_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_camera_assets_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(
            assets, "TGCP-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32) {
        return reject(
            assets, "TGCP-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload) ||
        !validate_opcode_relationships(payload)) {
        return reject(assets, "TGCP-1 source/opcode contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size) ||
        !validate_gameplay_court(gameplay_court, gameplay_court_size)) {
        return reject(
            assets, "TGCP-1 TGPL-1/TGCT-1 dependency contract rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGCP-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        TecmoGameplayCameraSourceSpan *source =
            &assets->sources[index];
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
    assets->initialize_routine = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET;
    assets->stream_columns = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET;
    assets->attribute_quadrants = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET;
    assets->follow_routine = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET;
    assets->forced_settle_routine = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET;
    assets->actor_projection_routine = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET;
    assets->gameplay_core_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->gameplay_court_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGCP-1 gameplay camera/projection assetpack");
    return true;
}

bool tecmo_gameplay_camera_assets_load(
    TecmoGameplayCameraAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *gameplay_court = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t gameplay_court_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CAMERA_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_camera_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ID,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGCP-1 gameplay/camera-projection entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets,
            "TGCP-1 gameplay/core dependency missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_COURT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE,
            &gameplay_court, &gameplay_court_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        return reject(
            assets,
            "TGCP-1 gameplay/court dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_camera_assets_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        gameplay_court, (size_t)gameplay_court_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(gameplay_court);
    return loaded;
}

const TecmoGameplayCameraSourceSpan *
tecmo_gameplay_camera_find_source(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_camera_state_initialize(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state)
{
    TecmoGameplayCameraState initialized;
    if (assets == NULL || !assets->available || state == NULL) {
        return false;
    }
    memset(&initialized, 0, sizeof(initialized));
    initialized.camera_x = read_u16(assets->storage + 148U);
    initialized.scroll_x = assets->storage[150U];
    initialized.scroll_aux = assets->storage[151U];
    initialized.nametable_page = assets->storage[152U];
    initialized.aux = assets->storage[153U];
    initialized.stream_direction = assets->storage[154U];
    initialized.layout_cursor = assets->storage[155U];
    *state = initialized;
    return true;
}

static bool camera_state_is_valid(const TecmoGameplayCameraState *state)
{
    return state != NULL && state->nametable_page <= 1U &&
           state->stream_direction <= 1U;
}

static void stream_left(TecmoGameplayCameraState *state)
{
    if (state->stream_direction == 0U) {
        state->layout_cursor =
            (uint8_t)(state->layout_cursor - 2U);
        state->stream_direction = 1U;
    }
    --state->layout_cursor;
}

static void stream_right(TecmoGameplayCameraState *state)
{
    if (state->stream_direction != 0U) {
        state->layout_cursor =
            (uint8_t)(state->layout_cursor + 2U);
        state->stream_direction = 0U;
    }
    ++state->layout_cursor;
}

bool tecmo_gameplay_camera_follow(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCameraFollowInput *input)
{
    TecmoGameplayCameraState next;
    uint16_t delta;
    uint8_t delta_low;
    uint8_t delta_high;
    uint8_t maximum_step;
    uint8_t amount;
    bool move_left;
    bool move_right;
    if (assets == NULL || !assets->available ||
        !camera_state_is_valid(state) || input == NULL ||
        input->orientation >= TECMO_GAMEPLAY_CAMERA_ORIENTATION_COUNT) {
        return false;
    }
    next = *state;
    if (input->camera_disabled) return true;

    if (input->action_route == 0U) {
        if (input->orientation == 0U) {
            if (next.endpoint_latched ||
                input->focus_world_x < 0x0160U) {
                next.left_threshold = assets->storage[165U];
                next.right_threshold = assets->storage[168U];
                maximum_step = assets->storage[156U];
                next.endpoint_latched = true;
            } else {
                next.left_threshold = assets->storage[164U];
                next.right_threshold = assets->storage[167U];
                maximum_step = assets->storage[157U];
            }
        } else {
            if (next.endpoint_latched ||
                input->focus_world_x >= 0x01A0U) {
                next.left_threshold = assets->storage[166U];
                next.right_threshold = assets->storage[169U];
                maximum_step = assets->storage[156U];
                next.endpoint_latched = true;
            } else {
                next.left_threshold = assets->storage[164U];
                next.right_threshold = assets->storage[167U];
                maximum_step = assets->storage[157U];
            }
        }
        next.thresholds_valid = true;
    } else {
        if (!next.thresholds_valid) return false;
        maximum_step = assets->storage[157U];
    }

    delta = (uint16_t)(input->focus_world_x - next.camera_x);
    delta_low = (uint8_t)delta;
    delta_high = (uint8_t)(delta >> 8U);
    amount = 0U;
    move_left = false;
    move_right = false;
    if ((delta_high & 0x80U) != 0U) {
        amount = maximum_step;
        move_left = true;
    } else if (delta_high == 0U &&
               delta_low <= next.left_threshold) {
        uint8_t difference =
            (uint8_t)(next.left_threshold - delta_low);
        amount = difference < maximum_step
            ? difference : maximum_step;
        move_left = true;
    } else {
        uint16_t difference =
            (uint16_t)delta - next.right_threshold;
        if ((int32_t)(uint32_t)difference >= 0 &&
            delta >= next.right_threshold) {
            amount = difference < maximum_step
                ? (uint8_t)difference : maximum_step;
            move_right = true;
        }
    }

    if (move_left) {
        uint8_t old_scroll;
        if (next.layout_cursor < 0x0CU) {
            *state = next;
            return true;
        }
        if (input->action_route == 0x01U ||
            input->action_route == 0x12U ||
            input->action_route == 0x13U) {
            *state = next;
            return true;
        }
        next.camera_x = (uint16_t)(next.camera_x - amount);
        old_scroll = next.scroll_x;
        next.scroll_x = (uint8_t)(next.scroll_x - amount);
        if (old_scroll < amount) next.nametable_page ^= 1U;
        if ((old_scroll & 0xF8U) != (next.scroll_x & 0xF8U)) {
            stream_left(&next);
        }
    } else if (move_right) {
        uint8_t old_scroll;
        uint16_t scroll_sum;
        if (next.layout_cursor >= 0x34U) {
            *state = next;
            return true;
        }
        if (input->action_route == 0x01U ||
            input->action_route == 0x12U ||
            input->action_route == 0x13U) {
            *state = next;
            return true;
        }
        next.camera_x = (uint16_t)(next.camera_x + amount);
        old_scroll = next.scroll_x;
        scroll_sum = (uint16_t)old_scroll + amount;
        next.scroll_x = (uint8_t)scroll_sum;
        if (scroll_sum > 0xFFU) next.nametable_page ^= 1U;
        if ((old_scroll & 0xF8U) != (next.scroll_x & 0xF8U)) {
            stream_right(&next);
        }
    }
    *state = next;
    return true;
}

bool tecmo_gameplay_camera_settle(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCameraFollowInput *input)
{
    TecmoGameplayCameraState settled;
    TecmoGameplayCameraFollowInput forced;
    uint16_t limit;
    if (assets == NULL || !assets->available ||
        !camera_state_is_valid(state) || input == NULL ||
        input->orientation >= TECMO_GAMEPLAY_CAMERA_ORIENTATION_COUNT) {
        return false;
    }
    settled = *state;
    forced = *input;
    forced.action_route = 0U;
    limit = read_u16(assets->storage + 158U);
    if (limit == 0U || limit > GAMEPLAY_CAMERA_SETTLE_LIMIT) return false;
    for (uint16_t update = 0U; update < limit; ++update) {
        uint8_t old_scroll = settled.scroll_x;
        if (!tecmo_gameplay_camera_follow(
                assets, &settled, &forced)) {
            return false;
        }
        if (settled.scroll_x == old_scroll) {
            *state = settled;
            return true;
        }
    }
    return false;
}

bool tecmo_gameplay_camera_project_actor(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state,
    uint16_t world_x,
    uint8_t world_y,
    uint8_t altitude,
    TecmoGameplayActorProjection *projection)
{
    TecmoGameplayActorProjection projected;
    uint16_t delta;
    if (assets == NULL || !assets->available ||
        !camera_state_is_valid(state) || projection == NULL) {
        return false;
    }
    memset(&projected, 0, sizeof(projected));
    delta = (uint16_t)(world_x - state->camera_x);
    projected.visible = (uint8_t)(delta >> 8U) == 0U;
    if (projected.visible) projected.screen_x = (uint8_t)delta;
    projected.screen_y = world_y >= altitude
        ? (uint8_t)(world_y - altitude) : 0U;
    *projection = projected;
    return true;
}

static bool camera_state_matches(
    const TecmoGameplayCameraState *state,
    uint16_t camera_x,
    uint8_t scroll_x,
    uint8_t page,
    uint8_t cursor,
    uint8_t direction)
{
    return state->camera_x == camera_x &&
           state->scroll_x == scroll_x &&
           state->scroll_aux == 0U &&
           state->nametable_page == page &&
           state->aux == 0U &&
           state->stream_direction == direction &&
           state->layout_cursor == cursor;
}

bool tecmo_gameplay_camera_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    static const uint16_t lineup_x[10] = {
        0x00F8U,0x0242U,0x0259U,0x0231U,0x0120U,
        0x022EU,0x0206U,0x0245U,0x00DAU,0x0102U
    };
    static const bool lineup_visible[10] = {
        false,true,true,true,false,true,true,true,false,false
    };
    static const uint8_t lineup_screen_x[10] = {
        0U,0xAAU,0xC1U,0x99U,0U,0x96U,0x6EU,0xADU,0U,0U
    };
    TecmoGameplayCameraAssets assets;
    TecmoGameplayCameraState state;
    TecmoGameplayCameraState unchanged;
    TecmoGameplayCameraFollowInput input;
    TecmoGameplayActorProjection projection;
    TecmoGameplayActorProjection projection_unchanged;
    uint32_t storage_hash;
    bool passed = false;
    tecmo_gameplay_camera_assets_init(&assets);
    memset(&state, 0xA5, sizeof(state));
    unchanged = state;
    memset(&input, 0, sizeof(input));
    if (tecmo_gameplay_camera_state_initialize(&assets, &state) ||
        tecmo_gameplay_camera_follow(&assets, &state, &input) ||
        tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "unavailable API accepted or mutated state");
        goto cleanup;
    }
    if (asset_pack_path == NULL ||
        !tecmo_gameplay_camera_assets_load(&assets, asset_pack_path) ||
        !tecmo_gameplay_camera_assets_load(&assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s",
                       asset_pack_path != NULL ? assets.status
                                               : "PACK path required");
        goto cleanup;
    }
    storage_hash = fnv1a32(assets.storage, assets.storage_size);

    if (!tecmo_gameplay_camera_state_initialize(&assets, &state) ||
        !camera_state_matches(&state, 0x0100U, 0x00U, 0U,
                              0x20U, 0U) ||
        state.thresholds_valid || state.endpoint_latched) {
        (void)snprintf(message, message_size,
                       "fixed $DE13 initializer golden failed");
        goto cleanup;
    }
    input.focus_world_x = 0x00FAU;
    input.orientation = 0U;
    input.action_route = 0x7FU;
    input.camera_disabled = false;
    unchanged = state;
    if (tecmo_gameplay_camera_follow(&assets, &state, &input) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "invalid preserved-threshold route mutated state");
        goto cleanup;
    }
    input.action_route = 0U;
    if (!tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        !camera_state_matches(&state, 0x006EU, 0x6EU, 1U,
                              0x0BU, 1U) ||
        !state.thresholds_valid || !state.endpoint_latched ||
        state.left_threshold != 0xD8U ||
        state.right_threshold != 0xE8U) {
        (void)snprintf(message, message_size,
                       "orientation-0 raw initializer settle golden failed");
        goto cleanup;
    }

    if (!tecmo_gameplay_camera_state_initialize(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "initializer repeat failed");
        goto cleanup;
    }
    input.focus_world_x = 0x0206U;
    input.orientation = 1U;
    if (!tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        !camera_state_matches(&state, 0x01A0U, 0xA0U, 0U,
                              0x34U, 0U) ||
        state.left_threshold != 0x20U ||
        state.right_threshold != 0x04U ||
        !state.endpoint_latched) {
        (void)snprintf(message, message_size,
                       "orientation-1 raw initializer settle golden failed");
        goto cleanup;
    }

    if (!tecmo_gameplay_camera_state_initialize(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "post-preload initializer failed");
        goto cleanup;
    }
    state.layout_cursor = 0x21U;
    if (!tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        !camera_state_matches(&state, 0x0198U, 0x98U, 0U,
                              0x34U, 0U)) {
        (void)snprintf(message, message_size,
                       "orientation-1 cursor-21 settle golden failed");
        goto cleanup;
    }
    for (size_t slot = 0U; slot < 10U; ++slot) {
        memset(&projection, 0xA5, sizeof(projection));
        if (!tecmo_gameplay_camera_project_actor(
                &assets, &state, lineup_x[slot], 0x94U, 0U,
                &projection) ||
            projection.visible != lineup_visible[slot] ||
            projection.screen_x != lineup_screen_x[slot] ||
            projection.screen_y != 0x94U) {
            (void)snprintf(
                message, message_size,
                "slot-3 lineup projection failed at actor %u",
                (unsigned)slot);
            goto cleanup;
        }
    }
    if (!tecmo_gameplay_camera_project_actor(
            &assets, &state, 0x0206U, 10U, 11U, &projection) ||
        !projection.visible || projection.screen_x != 0x6EU ||
        projection.screen_y != 0U) {
        (void)snprintf(message, message_size,
                       "actor Y saturation golden failed");
        goto cleanup;
    }

    unchanged = state;
    memset(&projection, 0xA5, sizeof(projection));
    projection_unchanged = projection;
    input.orientation = 2U;
    if (tecmo_gameplay_camera_follow(&assets, &state, &input) ||
        tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        tecmo_gameplay_camera_project_actor(
            &assets, &state, 0U, 0U, 0U, NULL) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0 ||
        memcmp(&projection, &projection_unchanged,
               sizeof(projection)) != 0 ||
        fnv1a32(assets.storage, assets.storage_size) != storage_hash) {
        (void)snprintf(message, message_size,
                       "invalid input mutated state/output/storage");
        goto cleanup;
    }

    (void)snprintf(
        message, message_size,
        "TGCP-1 gameplay camera self-test passed: init=0100 "
        "bounded=006E/01A0 preload=0198 visible=6");
    passed = true;

cleanup:
    tecmo_gameplay_camera_assets_destroy(&assets);
    tecmo_gameplay_camera_assets_destroy(&assets);
    return passed;
}
