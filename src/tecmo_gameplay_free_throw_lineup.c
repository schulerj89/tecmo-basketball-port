#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_free_throw_lineup.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_FREE_THROW_LINEUP_LIFECYCLE_TAG 0x4C464754U
#define FREE_THROW_LINEUP_ACTOR_RECORD_CPU 0x8000U
#define FREE_THROW_LINEUP_ACTOR_RECORD_END_CPU 0xA5B9U

static const uint8_t free_throw_lineup_rev1_sha256[32] = {
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

static bool reject(TecmoGameplayFreeThrowLineupAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->pose_lookup = NULL;
    assets->round_setup = NULL;
    assets->followup = NULL;
    assets->tables = NULL;
    assets->gameplay_core_fingerprint = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGFL-1 rejected");
    return false;
}

void tecmo_gameplay_free_throw_lineup_init(
    TecmoGameplayFreeThrowLineupAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag =
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_LIFECYCLE_TAG;
}

void tecmo_gameplay_free_throw_lineup_destroy(
    TecmoGameplayFreeThrowLineupAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_free_throw_lineup_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    size_t index;
    if (payload_size !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE ||
        memcmp(payload, "TGFL", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET ||
        read_u16(payload + 20U) !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT ||
        read_u16(payload + 22U) !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT ||
        read_u32(payload + 24U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(payload + 28U) != TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        read_u32(payload + 32U) != 393232U ||
        read_u32(payload + 36U) != 0x0650F5B0U ||
        memcmp(payload + 40U, free_throw_lineup_rev1_sha256,
               sizeof(free_throw_lineup_rev1_sha256)) != 0 ||
        payload[120U] != 6U || payload[121U] != 0U ||
        payload[122U] != 16U || payload[123U] != 8U ||
        payload[124U] != 2U ||
        !bytes_are_zero(
            payload + 125U,
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_HEADER_SIZE -
                125U)) {
        return false;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *expected =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        const uint8_t *descriptor = payload + 72U + index * 12U;
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
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *expected =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank || record[3U] != 0U ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u32(record + 16U) != expected->payload_offset ||
            !bytes_are_zero(record + 20U, 12U) ||
            expected->cpu_start < 0x8000U || cpu_end >= 0xC000U ||
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
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE));
}

static bool contains_bytes(const uint8_t *bytes,
                           size_t byte_count,
                           const uint8_t *needle,
                           size_t needle_count)
{
    if (needle_count == 0U || needle_count > byte_count) return false;
    for (size_t offset = 0U;
         offset <= byte_count - needle_count; ++offset) {
        if (memcmp(bytes + offset, needle, needle_count) == 0) return true;
    }
    return false;
}

static bool validate_opcode_relationships(const uint8_t *payload)
{
    static const uint8_t pose_prefix[] = {
        0xBCU,0x63U,0x04U,0xB9U,0xCAU,0x88U,0x9DU,0x42U,
        0x04U,0xB9U,0xD2U,0x88U,0x9DU,0x4DU,0x04U
    };
    static const uint8_t round_pointer_loads[] = {
        0xBDU,0x79U,0x98U,0x85U,0x84U,0xBDU,0x7AU,0x98U,
        0x85U,0x85U,0xBDU,0x81U,0x98U,0x85U,0x86U,
        0xBDU,0x82U,0x98U,0x85U,0x87U
    };
    static const uint8_t followup_pointer_loads[] = {
        0xBDU,0x7DU,0x98U,0x85U,0x84U,0xBDU,0x7EU,0x98U,
        0x85U,0x85U,0xBDU,0x85U,0x98U,0x85U,0x86U,
        0xBDU,0x86U,0x98U,0x85U,0x87U
    };
    static const uint8_t pose_dispatch[] = {
        0xB9U,0x65U,0x98U,0x9DU,0x63U,0x04U,
        0x20U,0xB0U,0x88U
    };
    static const uint8_t shooter_seed[] = {
        0xB9U,0x77U,0x98U,0x9DU,0x63U,0x04U,0x98U,0x0AU,
        0xA8U,0xB9U,0x5FU,0x98U,0x95U,0x73U,
        0xB9U,0x60U,0x98U,0x95U,0xE8U
    };
    const uint8_t *pose = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET;
    const uint8_t *round = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET;
    const uint8_t *followup = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET;
    return memcmp(pose, pose_prefix, sizeof(pose_prefix)) == 0 &&
           contains_bytes(round,
                          TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_SIZE,
                          round_pointer_loads,
                          sizeof(round_pointer_loads)) &&
           contains_bytes(
               followup,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE,
               followup_pointer_loads, sizeof(followup_pointer_loads)) &&
           contains_bytes(
               followup,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE,
               pose_dispatch, sizeof(pose_dispatch)) &&
           contains_bytes(
               followup,
               TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE,
               shooter_seed, sizeof(shooter_seed));
}

static bool validate_pose_record(const uint8_t *gameplay_core,
                                 uint16_t pointer_index)
{
    const uint8_t *pointers = gameplay_core +
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET;
    const uint8_t *records = gameplay_core +
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET;
    uint16_t target;
    size_t record_offset;
    uint8_t dimensions;
    unsigned width;
    unsigned height;
    size_t piece_count;
    if (pointer_index >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) return false;
    target = read_u16(pointers + (size_t)pointer_index * 2U);
    if (target < FREE_THROW_LINEUP_ACTOR_RECORD_CPU ||
        target >= FREE_THROW_LINEUP_ACTOR_RECORD_END_CPU) {
        return false;
    }
    record_offset =
        (size_t)(target - FREE_THROW_LINEUP_ACTOR_RECORD_CPU);
    if (record_offset >=
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE) {
        return false;
    }
    dimensions = records[record_offset];
    width = dimensions & 0x0FU;
    height = dimensions >> 4U;
    piece_count = (size_t)width * height;
    return width != 0U && height != 0U &&
           piece_count <= TECMO_GAMEPLAY_RESOLVED_PIECE_MAX &&
           record_offset + 4U <=
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE &&
           piece_count <=
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE -
                   record_offset - 4U;
}

static bool validate_gameplay_core(const uint8_t *gameplay_core,
                                   size_t gameplay_core_size)
{
    static const uint16_t pose_indices[4] = {
        517U, 518U, 519U, 520U
    };
    if (gameplay_core == NULL ||
        gameplay_core_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        memcmp(gameplay_core, "TGPL", 4U) != 0 ||
        read_u16(gameplay_core + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_VERSION ||
        read_u16(gameplay_core + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE ||
        read_u32(gameplay_core + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(gameplay_core + 52U) !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET ||
        read_u32(gameplay_core + 56U) !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE ||
        read_u32(gameplay_core + 60U) !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET ||
        read_u32(gameplay_core + 64U) !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_SIZE ||
        read_u16(gameplay_core + 172U) !=
            TECMO_GAMEPLAY_ASSET_POINTER_COUNT ||
        read_u16(gameplay_core + 182U) != 0U ||
        fnv1a32(gameplay_core, gameplay_core_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index) {
        if (!validate_pose_record(gameplay_core, pose_indices[index])) {
            return false;
        }
    }
    return true;
}

static bool pose_from_direction(const uint8_t *pose_lookup,
                                uint8_t direction,
                                uint16_t *raw_pose_offset,
                                uint16_t *pose_index)
{
    uint16_t raw;
    if (direction >= 8U || raw_pose_offset == NULL ||
        pose_index == NULL) {
        return false;
    }
    raw = (uint16_t)pose_lookup[26U + direction] |
          ((uint16_t)pose_lookup[34U + direction] << 8U);
    if ((raw & 1U) != 0U || raw < 0x040AU || raw > 0x0410U) {
        return false;
    }
    *raw_pose_offset = raw;
    *pose_index = (uint16_t)(raw / 2U);
    return *pose_index >= 517U && *pose_index <= 520U;
}

static bool validate_table_relationships(const uint8_t *payload,
                                         const uint8_t *gameplay_core)
{
    static const uint16_t position_pointers[2] = {
        0x9889U, 0x989BU
    };
    static const uint16_t height_pointers[2] = {
        0x98ADU, 0x98BFU
    };
    static const uint8_t pose_seeds[2] = {17U, 8U};
    static const uint8_t shooter_directions[2] = {1U, 0U};
    static const uint16_t shooter_x[2] = {0x00FAU, 0x0206U};
    const uint8_t *pose_lookup = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET;
    const uint8_t *tables = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET;
    if (tables[0U] != 1U || tables[1U] != 6U ||
        read_u16(tables + (0x985FU - 0x985DU)) != shooter_x[0U] ||
        read_u16(tables + (0x9861U - 0x985DU)) != shooter_x[1U]) {
        return false;
    }
    for (size_t direction = 0U; direction < 8U; ++direction) {
        uint16_t raw_pose_offset;
        uint16_t pose_index;
        if (!pose_from_direction(pose_lookup, (uint8_t)direction,
                                 &raw_pose_offset, &pose_index) ||
            !validate_pose_record(gameplay_core, pose_index)) {
            return false;
        }
    }
    for (size_t orientation = 0U;
         orientation <
             TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT;
         ++orientation) {
        uint16_t position_pointer = read_u16(
            tables + (0x987DU - 0x985DU) + orientation * 2U);
        uint16_t height_pointer = read_u16(
            tables + (0x9885U - 0x985DU) + orientation * 2U);
        size_t position_offset;
        size_t height_offset;
        if (tables[(0x9863U - 0x985DU) + orientation] !=
                pose_seeds[orientation] ||
            tables[(0x9877U - 0x985DU) + orientation] !=
                shooter_directions[orientation] ||
            position_pointer != position_pointers[orientation] ||
            height_pointer != height_pointers[orientation] ||
            position_pointer < 0x985DU ||
            height_pointer < 0x985DU) {
            return false;
        }
        position_offset = (size_t)(position_pointer - 0x985DU);
        height_offset = (size_t)(height_pointer - 0x985DU);
        if (!range_ok(
                position_offset, 18U,
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE) ||
            !range_ok(
                height_offset, 18U,
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE)) {
            return false;
        }
        for (size_t pair = 0U; pair < 9U; ++pair) {
            if (read_u16(tables + height_offset + pair * 2U) >
                UINT8_MAX) {
                return false;
            }
        }
    }
    for (size_t index = 0U; index < 18U; ++index) {
        if (tables[(0x9865U - 0x985DU) + index] >= 8U) {
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_free_throw_lineup_parse(
    TecmoGameplayFreeThrowLineupAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_free_throw_lineup_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets,
                      "TGFL-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FNV1A32) {
        return reject(assets,
                      "TGFL-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload) ||
        !validate_opcode_relationships(payload)) {
        return reject(assets,
                      "TGFL-1 source/opcode contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size) ||
        !validate_table_relationships(payload, gameplay_core)) {
        return reject(
            assets,
            "TGFL-1 table/TGPL-1 dependency contract rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGFL-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *expected =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        TecmoGameplayFreeThrowLineupSourceSpan *source =
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
    assets->pose_lookup = storage +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET;
    assets->round_setup = storage +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET;
    assets->followup = storage +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET;
    assets->tables = storage +
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET;
    assets->gameplay_core_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGFL-1 gameplay free-throw lineup assetpack");
    return true;
}

bool tecmo_gameplay_free_throw_lineup_load(
    TecmoGameplayFreeThrowLineupAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_free_throw_lineup_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ID,
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGFL-1 gameplay/free-throw-lineup entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets,
            "TGFL-1 gameplay/core dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_free_throw_lineup_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    return loaded;
}

const TecmoGameplayFreeThrowLineupSourceSpan *
tecmo_gameplay_free_throw_lineup_find_source(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    TecmoGameplayFreeThrowLineupSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT;
         ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_free_throw_lineup_derive(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    uint8_t orientation,
    uint8_t shooter_slot,
    uint8_t secondary_slot,
    TecmoGameplayFreeThrowLineup *lineup)
{
    TecmoGameplayFreeThrowLineup derived;
    const uint8_t *tables;
    uint16_t position_pointer;
    uint16_t height_pointer;
    size_t position_offset;
    size_t height_offset;
    uint8_t pose_stream_index;
    uint8_t position_pair_index = 8U;
    if (assets == NULL || !assets->available || lineup == NULL ||
        orientation >=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT ||
        shooter_slot >=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT ||
        secondary_slot >=
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT ||
        shooter_slot == secondary_slot) {
        return false;
    }

    memset(&derived, 0, sizeof(derived));
    derived.orientation = orientation;
    derived.shooter_slot = shooter_slot;
    derived.secondary_slot = secondary_slot;
    tables = assets->tables;
    position_pointer = read_u16(
        tables + (0x987DU - 0x985DU) + (size_t)orientation * 2U);
    height_pointer = read_u16(
        tables + (0x9885U - 0x985DU) + (size_t)orientation * 2U);
    if (position_pointer < 0x985DU || height_pointer < 0x985DU) {
        return false;
    }
    position_offset = (size_t)(position_pointer - 0x985DU);
    height_offset = (size_t)(height_pointer - 0x985DU);
    pose_stream_index =
        tables[(0x9863U - 0x985DU) + orientation];

    for (int slot = 9; slot >= 0; --slot) {
        TecmoGameplayFreeThrowLineupActor *actor =
            &derived.actors[(size_t)slot];
        actor->shooter = (uint8_t)slot == shooter_slot;
        actor->secondary = (uint8_t)slot == secondary_slot;
        actor->position_defined = true;
        actor->raw_palette_attributes = 0x30U;
        actor->raw_actor_state = 0x01U;
        actor->raw_aux_state = 0x00U;
        if (actor->shooter) {
            actor->raw_world_x = read_u16(
                tables + (0x985FU - 0x985DU) +
                (size_t)orientation * 2U);
            actor->raw_world_y = 0x94U;
            actor->direction_index =
                tables[(0x9877U - 0x985DU) + orientation];
            actor->raw_pose_offset = UINT16_MAX;
            actor->pose_index = UINT16_MAX;
            actor->position_pair_index =
                TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX;
            actor->pose_stream_index =
                TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX;
            actor->raw_action_phase = 0x20U;
            actor->raw_pose_flags = 0x41U;
            actor->pose_defined = false;
            continue;
        }
        {
            size_t pair_offset =
                position_offset + (size_t)position_pair_index * 2U;
            uint16_t raw_pose_offset;
            uint16_t pointer_index;
            actor->raw_world_x = read_u16(tables + pair_offset);
            actor->raw_world_y =
                tables[height_offset +
                       (size_t)position_pair_index * 2U];
            actor->direction_index =
                tables[(0x9865U - 0x985DU) + pose_stream_index];
            if (!pose_from_direction(
                    assets->pose_lookup, actor->direction_index,
                    &raw_pose_offset, &pointer_index)) {
                return false;
            }
            actor->raw_pose_offset = raw_pose_offset;
            actor->pose_index = pointer_index;
            actor->position_pair_index = position_pair_index;
            actor->pose_stream_index = pose_stream_index;
            actor->raw_action_phase = 0x00U;
            actor->raw_pose_flags = 0xC1U;
            actor->pose_defined = true;
        }
        if (position_pair_index != 0U) --position_pair_index;
        if (pose_stream_index != 0U) --pose_stream_index;
    }
    *lineup = derived;
    return true;
}

static bool validate_derived_actor(
    const TecmoGameplayFreeThrowLineupAssets *assets,
    const TecmoGameplayFreeThrowLineup *lineup,
    uint8_t orientation,
    uint8_t shooter_slot,
    uint8_t secondary_slot,
    uint8_t slot)
{
    static const uint16_t golden_x[2][9] = {
        {210U,187U,550U,510U,520U,190U,167U,207U,480U},
        {248U,578U,601U,561U,288U,558U,581U,218U,258U}
    };
    static const uint8_t golden_y[2][9] = {
        {120U,180U,100U,200U,150U,120U,180U,180U,90U},
        {150U,120U,180U,180U,90U,120U,180U,100U,200U}
    };
    static const uint8_t golden_direction[2][9] = {
        {2U,5U,2U,5U,2U,2U,5U,5U,5U},
        {2U,2U,5U,5U,5U,2U,5U,2U,5U}
    };
    const uint8_t *tables = assets->tables;
    const TecmoGameplayFreeThrowLineupActor *actor =
        &lineup->actors[slot];
    if (actor->shooter != (slot == shooter_slot) ||
        actor->secondary != (slot == secondary_slot) ||
        !actor->position_defined ||
        actor->raw_actor_state != 0x01U ||
        actor->raw_palette_attributes != 0x30U ||
        actor->raw_aux_state != 0x00U) {
        return false;
    }
    if (slot == shooter_slot) {
        return actor->raw_world_x == read_u16(
                   tables + (0x985FU - 0x985DU) +
                   (size_t)orientation * 2U) &&
               actor->raw_world_y == 0x94U &&
               actor->direction_index ==
                   tables[(0x9877U - 0x985DU) + orientation] &&
               actor->raw_pose_offset == UINT16_MAX &&
               actor->pose_index == UINT16_MAX &&
               actor->position_pair_index ==
                   TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX &&
               actor->pose_stream_index ==
                   TECMO_GAMEPLAY_FREE_THROW_LINEUP_UNDEFINED_INDEX &&
               actor->raw_action_phase == 0x20U &&
               actor->raw_pose_flags == 0x41U &&
               !actor->pose_defined;
    }
    {
        uint8_t pair_index =
            slot < shooter_slot ? slot : (uint8_t)(slot - 1U);
        uint8_t higher_nonshooters =
            (uint8_t)((9U - slot) -
                (shooter_slot > slot ? 1U : 0U));
        uint8_t stream_index = (uint8_t)(
            tables[(0x9863U - 0x985DU) + orientation] -
            higher_nonshooters);
        uint16_t raw_pose_offset;
        uint16_t pointer_index;
        return pose_from_direction(
                   assets->pose_lookup,
                   golden_direction[orientation][pair_index],
                   &raw_pose_offset, &pointer_index) &&
               actor->raw_world_x == golden_x[orientation][pair_index] &&
               actor->raw_world_y == golden_y[orientation][pair_index] &&
               actor->direction_index ==
                   golden_direction[orientation][pair_index] &&
               actor->raw_pose_offset == raw_pose_offset &&
               actor->pose_index == pointer_index &&
               actor->position_pair_index == pair_index &&
               actor->pose_stream_index == stream_index &&
               actor->raw_action_phase == 0x00U &&
               actor->raw_pose_flags == 0xC1U &&
               actor->pose_defined;
    }
}

bool tecmo_gameplay_free_throw_lineup_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayFreeThrowLineupAssets assets;
    TecmoGameplayFreeThrowLineup lineup;
    TecmoGameplayFreeThrowLineup unchanged;
    uint32_t storage_hash;
    const TecmoGameplayFreeThrowLineupSourceSpan *pose_source;
    const TecmoGameplayFreeThrowLineupSourceSpan *tables_source;
    bool passed = false;
    tecmo_gameplay_free_throw_lineup_init(&assets);
    memset(&lineup, 0xA5, sizeof(lineup));
    unchanged = lineup;
    if (tecmo_gameplay_free_throw_lineup_derive(
            &assets, 0U, 0U, 1U, &lineup) ||
        memcmp(&lineup, &unchanged, sizeof(lineup)) != 0 ||
        tecmo_gameplay_free_throw_lineup_find_source(
            &assets,
            TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_POSE_LOOKUP) != NULL) {
        (void)snprintf(message, message_size,
                       "unavailable helper accepted or mutated input");
        goto cleanup;
    }
    if (asset_pack_path == NULL ||
        !tecmo_gameplay_free_throw_lineup_load(
            &assets, asset_pack_path) ||
        !tecmo_gameplay_free_throw_lineup_load(
            &assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s",
                       asset_pack_path != NULL ? assets.status
                                               : "PACK path required");
        goto cleanup;
    }
    pose_source = tecmo_gameplay_free_throw_lineup_find_source(
        &assets,
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_POSE_LOOKUP);
    tables_source = tecmo_gameplay_free_throw_lineup_find_source(
        &assets, TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_TABLES);
    if (pose_source == NULL || pose_source->bank != 6U ||
        pose_source->fixed_bank ||
        pose_source->cpu_start != 0x88B0U ||
        pose_source->cpu_end != 0x88D9U ||
        pose_source->byte_count != 42U ||
        pose_source->fingerprint != 0xAD834719U ||
        tables_source == NULL || tables_source->bank != 6U ||
        tables_source->fixed_bank ||
        tables_source->cpu_start != 0x985DU ||
        tables_source->cpu_end != 0x9918U ||
        tables_source->byte_count != 188U ||
        tables_source->fingerprint != 0xAFB31306U ||
        tecmo_gameplay_free_throw_lineup_find_source(
            &assets,
            (TecmoGameplayFreeThrowLineupSourceKind)0) != NULL ||
        tecmo_gameplay_free_throw_lineup_find_source(
            &assets,
            (TecmoGameplayFreeThrowLineupSourceKind)5) != NULL) {
        (void)snprintf(message, message_size,
                       "source provenance contract failed");
        goto cleanup;
    }
    storage_hash = fnv1a32(assets.storage, assets.storage_size);
    for (uint8_t orientation = 0U;
         orientation <
             TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT;
         ++orientation) {
        for (uint8_t shooter = 0U;
             shooter <
                 TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT;
             ++shooter) {
            for (uint8_t secondary = 0U;
                 secondary <
                     TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT;
                 ++secondary) {
                if (secondary == shooter) continue;
                if (!tecmo_gameplay_free_throw_lineup_derive(
                        &assets, orientation, shooter, secondary,
                        &lineup) ||
                    lineup.orientation != orientation ||
                    lineup.shooter_slot != shooter ||
                    lineup.secondary_slot != secondary) {
                    (void)snprintf(
                        message, message_size,
                        "derive rejected orientation=%u shooter=%u secondary=%u",
                        (unsigned)orientation, (unsigned)shooter,
                        (unsigned)secondary);
                    goto cleanup;
                }
                for (uint8_t slot = 0U;
                     slot <
                         TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT;
                     ++slot) {
                    if (!validate_derived_actor(
                            &assets, &lineup, orientation, shooter,
                            secondary, slot)) {
                        const TecmoGameplayFreeThrowLineupActor *actor =
                            &lineup.actors[slot];
                        (void)snprintf(
                            message, message_size,
                            "rank/pose seed failed o=%u s=%u n=%u slot=%u "
                            "x=%u y=%u dir=%u pair=%u stream=%u pose=%u",
                            (unsigned)orientation, (unsigned)shooter,
                            (unsigned)secondary, (unsigned)slot,
                            (unsigned)actor->raw_world_x,
                            (unsigned)actor->raw_world_y,
                            (unsigned)actor->direction_index,
                            (unsigned)actor->position_pair_index,
                            (unsigned)actor->pose_stream_index,
                            (unsigned)actor->pose_index);
                        goto cleanup;
                    }
                }
            }
        }
    }
    if (fnv1a32(assets.storage, assets.storage_size) != storage_hash) {
        (void)snprintf(message, message_size,
                       "pure derive mutated TGFL-1 storage");
        goto cleanup;
    }

    memset(&lineup, 0xA5, sizeof(lineup));
    unchanged = lineup;
    if (tecmo_gameplay_free_throw_lineup_derive(
            &assets, 2U, 0U, 1U, &lineup) ||
        tecmo_gameplay_free_throw_lineup_derive(
            &assets, 0U, 10U, 1U, &lineup) ||
        tecmo_gameplay_free_throw_lineup_derive(
            &assets, 0U, 0U, 10U, &lineup) ||
        tecmo_gameplay_free_throw_lineup_derive(
            &assets, 0U, 1U, 1U, &lineup) ||
        tecmo_gameplay_free_throw_lineup_derive(
            &assets, 0U, 0U, 1U, NULL) ||
        memcmp(&lineup, &unchanged, sizeof(lineup)) != 0 ||
        fnv1a32(assets.storage, assets.storage_size) != storage_hash) {
        (void)snprintf(message, message_size,
                       "invalid input accepted or mutated output/storage");
        goto cleanup;
    }

    (void)snprintf(
        message, message_size,
        "TGFL-1 free-throw lineup passed: orientations=2 actors=10 "
        "poses=040A/040C/040E/0410 indices=517-520");
    passed = true;

cleanup:
    tecmo_gameplay_free_throw_lineup_destroy(&assets);
    tecmo_gameplay_free_throw_lineup_destroy(&assets);
    return passed;
}
