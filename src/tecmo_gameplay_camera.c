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

static const uint8_t gameplay_camera_actor_clamp_sha256[32] = {
    0x0BU,0x97U,0xA9U,0xAAU,0xC4U,0xDFU,0x35U,0xE4U,
    0xEDU,0xF7U,0x97U,0x9CU,0x6CU,0x03U,0x55U,0x85U,
    0x2BU,0x9DU,0xE7U,0x39U,0x88U,0x44U,0xB2U,0x67U,
    0x9CU,0xFAU,0xB2U,0x98U,0xF0U,0xC0U,0xCBU,0xA6U
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
    assets->actor_clamp_routine = NULL;
    assets->gameplay_core_fingerprint = 0U;
    assets->gameplay_court_fingerprint = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGCP-2 rejected");
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
        read_u16(
            payload +
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_CAMERA_X_OFFSET) !=
            0x0100U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_SCROLL_X_OFFSET] !=
            0U ||
        payload[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_SCROLL_AUX_OFFSET] !=
            0U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_PAGE_OFFSET] != 0U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_AUX_OFFSET] != 0U ||
        payload[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_DIRECTION_OFFSET] !=
            0U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_CURSOR_OFFSET] !=
            0x20U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ENDPOINT_SPEED_OFFSET] !=
            2U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_GENERIC_SPEED_OFFSET] !=
            7U ||
        read_u16(
            payload +
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_LIMIT_OFFSET) !=
            GAMEPLAY_CAMERA_SETTLE_LIMIT ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_WORLD_X_WIDTH_OFFSET] !=
            16U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_WORLD_Y_WIDTH_OFFSET] !=
            8U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_VISIBLE_HIGH_OFFSET] != 0U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_Y_SATURATES_OFFSET] != 1U ||
        memcmp(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET,
            "\x50\xD8\x20\xA0\xE8\x04", 6U) != 0 ||
        !bytes_are_zero(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PRE_SHA_RESERVED_OFFSET,
            2U) ||
        memcmp(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_SHA256_OFFSET,
            gameplay_camera_actor_clamp_sha256,
            sizeof(gameplay_camera_actor_clamp_sha256)) != 0 ||
        !bytes_are_zero(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_RESERVED_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_SIZE -
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_RESERVED_OFFSET)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        const uint8_t *descriptor =
            payload +
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_DESCRIPTOR_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_DESCRIPTOR_STRIDE;
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
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_SIZE));
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
    static const uint8_t stream_terminal_table[] = {
        0xA2U,0x60U,0x48U,0x08U
    };
    static const uint8_t projection_terminal_store[] = {
        0xA9U,0x00U,0x85U,0x0BU
    };
    static const uint8_t actor_clamp_dispatch[] = {
        0xB5U,0xE8U,0xF0U,0x05U,0xC9U,0x02U,0xF0U,0x43U,0x60U
    };
    static const uint8_t actor_clamp_left[] = {
        0x98U,0x4AU,0x49U,0xFFU,0x18U,0x69U,0xE0U,0xD5U,
        0x73U,0x90U,0xDBU,0x95U,0x73U
    };
    static const uint8_t actor_clamp_right[] = {
        0x98U,0x4AU,0x18U,0x69U,0x20U,0xD5U,0x73U,0xB0U,
        0x9BU,0x95U,0x73U
    };
    const uint8_t *initialize = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET;
    const uint8_t *stream = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET;
    const uint8_t *settle = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET;
    const uint8_t *projection = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET;
    const uint8_t *actor_clamp = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET;
    return memcmp(initialize, initialize_exact,
                  sizeof(initialize_exact)) == 0 &&
           memcmp(settle + 29U, settle_loop, sizeof(settle_loop)) == 0 &&
           memcmp(projection + 12U, projection_subtract,
                  sizeof(projection_subtract)) == 0 &&
           memcmp(stream + 247U, stream_terminal_table,
                  sizeof(stream_terminal_table)) == 0 &&
           memcmp(projection + 35U, projection_terminal_store,
                  sizeof(projection_terminal_store)) == 0 &&
           memcmp(actor_clamp + 33U, actor_clamp_dispatch,
                  sizeof(actor_clamp_dispatch)) == 0 &&
           memcmp(actor_clamp + 67U, actor_clamp_left,
                  sizeof(actor_clamp_left)) == 0 &&
           memcmp(actor_clamp + 133U, actor_clamp_right,
                  sizeof(actor_clamp_right)) == 0;
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
            assets, "TGCP-2 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32) {
        return reject(
            assets, "TGCP-2 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload) ||
        !validate_opcode_relationships(payload)) {
        return reject(assets, "TGCP-2 source/opcode contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size) ||
        !validate_gameplay_court(gameplay_court, gameplay_court_size)) {
        return reject(
            assets, "TGCP-2 TGPL-1/TGCT-1 dependency contract rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGCP-2 allocation failed");
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
    assets->actor_clamp_routine = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ACTOR_CLAMP_OFFSET;
    assets->gameplay_core_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->gameplay_court_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGCP-2 gameplay camera/projection/clamp assetpack");
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
            "TGCP-2 gameplay/camera-projection entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets,
            "TGCP-2 gameplay/core dependency missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_COURT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE,
            &gameplay_court, &gameplay_court_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        return reject(
            assets,
            "TGCP-2 gameplay/court dependency missing or wrong-sized");
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
    initialized.camera_x = read_u16(
        assets->storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_CAMERA_X_OFFSET);
    initialized.scroll_x =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_SCROLL_X_OFFSET];
    initialized.scroll_aux =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_SCROLL_AUX_OFFSET];
    initialized.nametable_page =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_PAGE_OFFSET];
    initialized.aux =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_AUX_OFFSET];
    initialized.stream_direction =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_DIRECTION_OFFSET];
    initialized.layout_cursor =
        assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIAL_CURSOR_OFFSET];
    *state = initialized;
    return true;
}

static bool camera_state_is_valid(const TecmoGameplayCameraState *state)
{
    return state != NULL && state->nametable_page <= 1U &&
           state->stream_direction <= 1U;
}

bool tecmo_gameplay_camera_state_prime_live(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state)
{
    TecmoGameplayCameraState primed;
    if (assets == NULL || !assets->available ||
        !camera_state_is_valid(state)) {
        return false;
    }
    primed = *state;
    if (primed.camera_x != 0x0100U || primed.scroll_x != 0U ||
        primed.scroll_aux != 0U || primed.nametable_page != 0U ||
        primed.aux != 0U || primed.stream_direction != 0U ||
        primed.layout_cursor != 0x20U || primed.thresholds_valid ||
        primed.endpoint_latched) {
        return false;
    }
    /*
     * Fixed $DDFB calls $DF05 once immediately after $DE13. With the initial
     * rightward stream direction, that first prefetch advances $38 from $20
     * to $21 while leaving the camera/scroll tuple unchanged.
     */
    primed.layout_cursor = 0x21U;
    *state = primed;
    return true;
}

bool tecmo_gameplay_camera_state_live_valid(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state)
{
    const uint8_t *thresholds;
    uint16_t coarse_camera;
    uint16_t cursor_value;
    uint8_t expected_cursor;
    uint8_t expected_page;
    if (assets == NULL || !assets->available ||
        !camera_state_is_valid(state) ||
        state->camera_x > 0x0200U ||
        state->scroll_x != (uint8_t)state->camera_x ||
        state->scroll_aux != 0U || state->aux != 0U ||
        state->layout_cursor < 0x0BU || state->layout_cursor > 0x34U) {
        return false;
    }
    coarse_camera = state->camera_x >> 3U;
    if (state->stream_direction == 0U) {
        cursor_value = coarse_camera + 1U;
        expected_cursor = cursor_value > 0x34U
            ? 0x34U : (uint8_t)cursor_value;
    } else {
        cursor_value = coarse_camera == 0U ? 0U : coarse_camera - 1U;
        expected_cursor = cursor_value < 0x0BU
            ? 0x0BU : (uint8_t)cursor_value;
    }
    if (state->layout_cursor != expected_cursor ||
        (!state->thresholds_valid && state->endpoint_latched)) {
        return false;
    }
    expected_page = (uint8_t)(
        (((state->camera_x >> 8U) ^ 1U) & 1U));
    if (state->nametable_page != expected_page ||
        !state->thresholds_valid) {
        return state->nametable_page == expected_page &&
               !state->thresholds_valid;
    }
    thresholds =
        assets->storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET;
    if (!state->endpoint_latched) {
        return state->left_threshold == thresholds[0U] &&
               state->right_threshold == thresholds[3U];
    }
    return (state->left_threshold == thresholds[1U] &&
            state->right_threshold == thresholds[4U]) ||
           (state->left_threshold == thresholds[2U] &&
            state->right_threshold == thresholds[5U]);
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
                next.left_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        1U];
                next.right_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        4U];
                maximum_step =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ENDPOINT_SPEED_OFFSET];
                next.endpoint_latched = true;
            } else {
                next.left_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET];
                next.right_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        3U];
                maximum_step =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_GENERIC_SPEED_OFFSET];
            }
        } else {
            if (next.endpoint_latched ||
                input->focus_world_x >= 0x01A0U) {
                next.left_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        2U];
                next.right_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        5U];
                maximum_step =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ENDPOINT_SPEED_OFFSET];
                next.endpoint_latched = true;
            } else {
                next.left_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET];
                next.right_threshold =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_THRESHOLDS_OFFSET +
                        3U];
                maximum_step =
                    assets->storage[
                        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_GENERIC_SPEED_OFFSET];
            }
        }
        next.thresholds_valid = true;
    } else {
        if (!next.thresholds_valid) return false;
        maximum_step =
            assets->storage[
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_GENERIC_SPEED_OFFSET];
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
    limit = read_u16(
        assets->storage +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_LIMIT_OFFSET);
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
    if (projected.visible) {
        projected.screen_x = (uint8_t)delta;
        projected.screen_y = world_y >= altitude
            ? (uint8_t)(world_y - altitude) : 0U;
    }
    *projection = projected;
    return true;
}

static bool camera_court_follow_input(
    const TecmoGameplayCourtCoordinateQ8 *focus,
    uint8_t orientation,
    uint8_t action_route,
    bool camera_disabled,
    TecmoGameplayCameraFollowInput *input_out)
{
    TecmoGameplayCourtCoordinate focus_pixel;
    TecmoGameplayCameraFollowInput input;
    if (input_out == NULL ||
        !tecmo_gameplay_court_coordinate_q8_floor(
            focus, &focus_pixel)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.focus_world_x = (uint16_t)focus_pixel.x;
    input.orientation = orientation;
    input.action_route = action_route;
    input.camera_disabled = camera_disabled;
    *input_out = input;
    return true;
}

bool tecmo_gameplay_camera_follow_court(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCourtCoordinateQ8 *focus,
    uint8_t orientation,
    uint8_t action_route,
    bool camera_disabled)
{
    TecmoGameplayCameraFollowInput input;
    return camera_court_follow_input(
               focus, orientation, action_route, camera_disabled, &input) &&
           tecmo_gameplay_camera_follow(assets, state, &input);
}

bool tecmo_gameplay_camera_settle_court(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    const TecmoGameplayCourtCoordinateQ8 *focus,
    uint8_t orientation,
    bool camera_disabled)
{
    TecmoGameplayCameraFollowInput input;
    return camera_court_follow_input(
               focus, orientation, 0U, camera_disabled, &input) &&
           tecmo_gameplay_camera_settle(assets, state, &input);
}

bool tecmo_gameplay_camera_project_court(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state,
    const TecmoGameplayCourtCoordinate *coordinate,
    uint8_t altitude,
    TecmoGameplayActorProjection *projection)
{
    if (!tecmo_gameplay_court_coordinate_valid(coordinate)) return false;
    return tecmo_gameplay_camera_project_actor(
        assets, state, (uint16_t)coordinate->x, (uint8_t)coordinate->y,
        altitude, projection);
}

bool tecmo_gameplay_camera_project_court_q8(
    const TecmoGameplayCameraAssets *assets,
    const TecmoGameplayCameraState *state,
    const TecmoGameplayCourtCoordinateQ8 *coordinate,
    uint8_t altitude,
    TecmoGameplayActorProjection *projection)
{
    TecmoGameplayCourtCoordinate coordinate_pixel;
    return tecmo_gameplay_court_coordinate_q8_floor(
               coordinate, &coordinate_pixel) &&
           tecmo_gameplay_camera_project_court(
               assets, state, &coordinate_pixel, altitude, projection);
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

static bool camera_states_equal(
    const TecmoGameplayCameraState *left,
    const TecmoGameplayCameraState *right)
{
    return left->camera_x == right->camera_x &&
           left->scroll_x == right->scroll_x &&
           left->scroll_aux == right->scroll_aux &&
           left->nametable_page == right->nametable_page &&
           left->aux == right->aux &&
           left->stream_direction == right->stream_direction &&
           left->layout_cursor == right->layout_cursor &&
           left->left_threshold == right->left_threshold &&
           left->right_threshold == right->right_threshold &&
           left->thresholds_valid == right->thresholds_valid &&
           left->endpoint_latched == right->endpoint_latched;
}

static bool live_validation_matches_without_mutation(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState *state,
    bool expected)
{
    TecmoGameplayCameraState before;
    bool actual;
    if (state == NULL) return false;
    before = *state;
    actual = tecmo_gameplay_camera_state_live_valid(assets, state);
    return actual == expected && camera_states_equal(state, &before);
}

static bool run_follow_vector(
    const TecmoGameplayCameraAssets *assets,
    TecmoGameplayCameraState initial,
    const TecmoGameplayCameraFollowInput *input,
    uint16_t camera_x,
    uint8_t scroll_x,
    uint8_t page,
    uint8_t cursor,
    uint8_t direction)
{
    if (!tecmo_gameplay_camera_follow(assets, &initial, input)) {
        return false;
    }
    return camera_state_matches(
               &initial, camera_x, scroll_x, page, cursor, direction) &&
           initial.left_threshold == 0x50U &&
           initial.right_threshold == 0xA0U &&
           initial.thresholds_valid &&
           !initial.endpoint_latched;
}

bool tecmo_gameplay_camera_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayCameraAssets assets;
    TecmoGameplayCameraState state;
    TecmoGameplayCameraState unchanged;
    TecmoGameplayCameraState vector;
    TecmoGameplayCameraState live_prime;
    TecmoGameplayCameraState live_candidate;
    TecmoGameplayCameraFollowInput input;
    TecmoGameplayActorProjection projection;
    TecmoGameplayActorProjection projection_unchanged;
    TecmoGameplayCourtCoordinate coordinate;
    TecmoGameplayCourtCoordinateQ8 coordinate_q8;
    uint32_t storage_hash;
    bool passed = false;
    tecmo_gameplay_camera_assets_init(&assets);
    memset(&state, 0xA5, sizeof(state));
    unchanged = state;
    memset(&input, 0, sizeof(input));
    coordinate_q8.x_q8 = 0x0100 * 256;
    coordinate_q8.y_q8 = 120 * 256;
    if (tecmo_gameplay_camera_state_initialize(&assets, &state) ||
        tecmo_gameplay_camera_follow(&assets, &state, &input) ||
        tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        tecmo_gameplay_camera_follow_court(
            &assets, &state, &coordinate_q8, 0U, 0U, false) ||
        tecmo_gameplay_camera_settle_court(
            &assets, &state, &coordinate_q8, 0U, false) ||
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

    memset(&input, 0, sizeof(input));
    input.orientation = 0U;
    input.focus_world_x = 0x01C0U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0100U;
    vector.scroll_x = 0x20U;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x0107U, 0x27U, 0U, 0x20U, 0U)) {
        (void)snprintf(message, message_size,
                       "generic seven-pixel right follow failed");
        goto cleanup;
    }
    coordinate_q8.x_q8 = 0x01C0 * 256 + 0xFF;
    coordinate_q8.y_q8 = 120 * 256 + 0xFF;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0100U;
    vector.scroll_x = 0x20U;
    vector.layout_cursor = 0x20U;
    if (!tecmo_gameplay_camera_follow_court(
            &assets, &vector, &coordinate_q8, 0U, 0U, false) ||
        !camera_state_matches(
            &vector, 0x0107U, 0x27U, 0U, 0x20U, 0U)) {
        (void)snprintf(message, message_size,
                       "canonical Q8 follow adapter failed");
        goto cleanup;
    }
    input.focus_world_x = 0x0170U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0200U;
    vector.scroll_x = 0x27U;
    vector.stream_direction = 1U;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x01F9U, 0x20U, 0U, 0x20U, 1U)) {
        (void)snprintf(message, message_size,
                       "generic seven-pixel left follow failed");
        goto cleanup;
    }

    input.focus_world_x = 0x01C0U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0100U;
    vector.scroll_x = 0xFCU;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x0107U, 0x03U, 1U, 0x21U, 0U)) {
        (void)snprintf(message, message_size,
                       "right page-carry/coarse-column follow failed");
        goto cleanup;
    }
    input.focus_world_x = 0x0170U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0200U;
    vector.scroll_x = 0x03U;
    vector.stream_direction = 1U;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x01F9U, 0xFCU, 1U, 0x1FU, 1U)) {
        (void)snprintf(message, message_size,
                       "left page-borrow/coarse-column follow failed");
        goto cleanup;
    }

    input.focus_world_x = 0x01C0U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0100U;
    vector.scroll_x = 0x1CU;
    vector.stream_direction = 1U;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x0107U, 0x23U, 0U, 0x23U, 0U)) {
        (void)snprintf(message, message_size,
                       "right reversal three-column follow failed");
        goto cleanup;
    }
    input.focus_world_x = 0x0170U;
    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0200U;
    vector.scroll_x = 0x20U;
    vector.layout_cursor = 0x20U;
    if (!run_follow_vector(
            &assets, vector, &input,
            0x01F9U, 0x19U, 0U, 0x1DU, 1U)) {
        (void)snprintf(message, message_size,
                       "left reversal three-column follow failed");
        goto cleanup;
    }

    memset(&vector, 0, sizeof(vector));
    vector.camera_x = 0x0100U;
    vector.scroll_x = 0x20U;
    vector.layout_cursor = 0x20U;
    unchanged = vector;
    input.focus_world_x = 0x01C0U;
    input.camera_disabled = true;
    if (!tecmo_gameplay_camera_follow(&assets, &vector, &input) ||
        !camera_states_equal(&vector, &unchanged)) {
        (void)snprintf(message, message_size,
                       "camera-disabled follow mutated state");
        goto cleanup;
    }
    input.camera_disabled = false;
    vector.left_threshold = 0x50U;
    vector.right_threshold = 0xA0U;
    vector.thresholds_valid = true;
    for (uint8_t route_index = 0U; route_index < 3U; ++route_index) {
        static const uint8_t suppressed_routes[3] = {
            0x01U, 0x12U, 0x13U
        };
        unchanged = vector;
        input.action_route = suppressed_routes[route_index];
        if (!tecmo_gameplay_camera_follow(&assets, &vector, &input) ||
            !camera_states_equal(&vector, &unchanged)) {
            (void)snprintf(
                message, message_size,
                "suppressed route %u mutated camera state",
                (unsigned)suppressed_routes[route_index]);
            goto cleanup;
        }
    }
    input.action_route = 0U;

    if (!tecmo_gameplay_camera_state_initialize(&assets, &state) ||
        !camera_state_matches(&state, 0x0100U, 0x00U, 0U,
                              0x20U, 0U) ||
        state.thresholds_valid || state.endpoint_latched) {
        (void)snprintf(message, message_size,
                       "fixed $DE13 initializer golden failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_camera_state_prime_live(&assets, &state) ||
        !camera_state_matches(&state, 0x0100U, 0x00U, 0U,
                              0x21U, 0U) ||
        !tecmo_gameplay_camera_state_live_valid(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "fixed $DDFB->$DF05 live prime golden failed");
        goto cleanup;
    }
    live_prime = state;
    live_candidate = live_prime;
    live_candidate.layout_cursor = 0x34U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted impossible right cursor");
        goto cleanup;
    }
    live_candidate = live_prime;
    live_candidate.stream_direction = 1U;
    live_candidate.layout_cursor = 0x0BU;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted impossible left cursor");
        goto cleanup;
    }
    live_candidate = live_prime;
    live_candidate.stream_direction = 1U;
    live_candidate.layout_cursor = 0x21U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted cursor/direction mismatch");
        goto cleanup;
    }
    live_candidate = live_prime;
    live_candidate.stream_direction = 1U;
    live_candidate.layout_cursor = 0x1FU;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, true)) {
        (void)snprintf(message, message_size,
                       "live camera rejected reachable left relationship");
        goto cleanup;
    }
    live_candidate = live_prime;
    live_candidate.endpoint_latched = true;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted invalid latch without thresholds");
        goto cleanup;
    }
    live_candidate = live_prime;
    live_candidate.thresholds_valid = true;
    live_candidate.left_threshold = 0x50U;
    live_candidate.right_threshold = 0xA0U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, true)) {
        (void)snprintf(message, message_size,
                       "live camera rejected generic threshold pair");
        goto cleanup;
    }
    live_candidate.endpoint_latched = true;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted generic pair with endpoint latch");
        goto cleanup;
    }
    live_candidate.left_threshold = 0xD8U;
    live_candidate.right_threshold = 0xE8U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, true)) {
        (void)snprintf(message, message_size,
                       "live camera rejected left endpoint threshold pair");
        goto cleanup;
    }
    live_candidate.endpoint_latched = false;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted left pair without endpoint latch");
        goto cleanup;
    }
    live_candidate.endpoint_latched = true;
    live_candidate.left_threshold = 0x20U;
    live_candidate.right_threshold = 0x04U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, true)) {
        (void)snprintf(message, message_size,
                       "live camera rejected right endpoint threshold pair");
        goto cleanup;
    }
    live_candidate.left_threshold = 0x21U;
    if (!live_validation_matches_without_mutation(
            &assets, &live_candidate, false)) {
        (void)snprintf(message, message_size,
                       "live camera accepted impossible threshold bytes");
        goto cleanup;
    }
    unchanged = state;
    if (tecmo_gameplay_camera_state_prime_live(&assets, &state) ||
        memcmp(&state, &unchanged, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "live camera accepted a repeated prime");
        goto cleanup;
    }
    state = live_prime;
    input.focus_world_x = 0x00FAU;
    input.orientation = 0U;
    input.action_route = 0U;
    input.camera_disabled = false;
    if (!tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        !camera_state_matches(&state, 0x0066U, 0x66U, 1U,
                              0x0BU, 1U) ||
        state.left_threshold != 0xD8U ||
        state.right_threshold != 0xE8U ||
        !state.thresholds_valid || !state.endpoint_latched ||
        !tecmo_gameplay_camera_state_live_valid(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "live-primed left endpoint settle golden failed");
        goto cleanup;
    }
    state = live_prime;
    input.focus_world_x = 0x0206U;
    input.orientation = 1U;
    if (!tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        !camera_state_matches(&state, 0x0198U, 0x98U, 0U,
                              0x34U, 0U) ||
        state.left_threshold != 0x20U ||
        state.right_threshold != 0x04U ||
        !state.thresholds_valid || !state.endpoint_latched ||
        !tecmo_gameplay_camera_state_live_valid(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "live-primed right endpoint settle golden failed");
        goto cleanup;
    }
    vector = state;
    state = live_prime;
    coordinate_q8.x_q8 = 0x0206 * 256 + 0xFF;
    coordinate_q8.y_q8 = 120 * 256 + 0xFF;
    if (!tecmo_gameplay_camera_settle_court(
            &assets, &state, &coordinate_q8, 1U, false) ||
        !camera_states_equal(&state, &vector)) {
        (void)snprintf(message, message_size,
                       "canonical Q8 settle adapter failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_camera_state_initialize(&assets, &state)) {
        (void)snprintf(message, message_size,
                       "initializer after live prime failed");
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

    if (!tecmo_gameplay_camera_project_actor(
            &assets, &state, 0x0206U, 10U, 11U, &projection) ||
        !projection.visible || projection.screen_x != 0x66U ||
        projection.screen_y != 0U) {
        (void)snprintf(message, message_size,
                       "actor Y saturation golden failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_camera_project_actor(
            &assets, &state, 0x0206U, 10U, 3U, &projection) ||
        !projection.visible || projection.screen_x != 0x66U ||
        projection.screen_y != 7U) {
        (void)snprintf(message, message_size,
                       "visible actor altitude subtraction failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_camera_project_actor(
            &assets, &state, 0x0090U, 0xFFU, 1U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U) {
        (void)snprintf(message, message_size,
                       "offscreen projection sentinel failed");
        goto cleanup;
    }
    coordinate.x = 0x0206;
    coordinate.y = 10;
    if (!tecmo_gameplay_camera_project_court(
            &assets, &state, &coordinate, 3U, &projection) ||
        !projection.visible || projection.screen_x != 0x66U ||
        projection.screen_y != 7U) {
        (void)snprintf(message, message_size,
                       "canonical integer projection adapter failed");
        goto cleanup;
    }
    coordinate_q8.x_q8 = 0x0206 * 256 + 0xFF;
    coordinate_q8.y_q8 = 10 * 256 + 0xFF;
    if (!tecmo_gameplay_camera_project_court_q8(
            &assets, &state, &coordinate_q8, 3U, &projection) ||
        !projection.visible || projection.screen_x != 0x66U ||
        projection.screen_y != 7U) {
        (void)snprintf(message, message_size,
                       "canonical Q8 projection adapter failed");
        goto cleanup;
    }
    coordinate.x = 0x0090;
    coordinate.y = TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
    if (!tecmo_gameplay_camera_project_court(
            &assets, &state, &coordinate, 1U, &projection) ||
        projection.visible || projection.screen_x != 0U ||
        projection.screen_y != 0U) {
        (void)snprintf(message, message_size,
                       "canonical offscreen projection sentinel failed");
        goto cleanup;
    }

    unchanged = state;
    memset(&projection, 0xA5, sizeof(projection));
    projection_unchanged = projection;
    input.orientation = 2U;
    coordinate.x = -1;
    coordinate.y = 0;
    coordinate_q8.x_q8 =
        TECMO_GAMEPLAY_COURT_COORDINATE_Q8_MAX_X + 1;
    coordinate_q8.y_q8 = 0;
    if (tecmo_gameplay_camera_follow(&assets, &state, &input) ||
        tecmo_gameplay_camera_settle(&assets, &state, &input) ||
        tecmo_gameplay_camera_follow_court(
            &assets, &state, &coordinate_q8, 0U, 0U, false) ||
        tecmo_gameplay_camera_settle_court(
            &assets, &state, &coordinate_q8, 0U, false) ||
        tecmo_gameplay_camera_project_court(
            &assets, &state, &coordinate, 0U, &projection) ||
        tecmo_gameplay_camera_project_court_q8(
            &assets, &state, &coordinate_q8, 0U, &projection) ||
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
        "TGCP-2 gameplay camera self-test passed: init=0100 "
        "pure=006E/01A0 live=0066/0198 strict-state/clamp");
    passed = true;

cleanup:
    tecmo_gameplay_camera_assets_destroy(&assets);
    tecmo_gameplay_camera_assets_destroy(&assets);
    return passed;
}
