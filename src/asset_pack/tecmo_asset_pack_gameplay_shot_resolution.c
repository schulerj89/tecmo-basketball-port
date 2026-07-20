#include "tecmo_asset_pack_gameplay_shot_resolution.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define SHOT_RESOLUTION_PRG_BANK_COUNT 8U

const TecmoGameplayShotResolutionExpectedSource
    tecmo_gameplay_shot_resolution_expected_sources[
        TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_OUTCOME_CALCULATION,
         0x91BCU, 639U, 0x4A0C68ACU, 0x1AE095FB719E110CULL},
        {TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_RIM_ROUTE_DISPATCH,
         0xA6EEU, 748U, 0x21A416FDU, 0x95A583E4DC801DFDULL},
        {TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SCAN,
         0xB73EU, 318U, 0x574FEE44U, 0xB2E76E7A39990624ULL},
        {TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_CLAIMANT_SETTLEMENT,
         0xB87CU, 122U, 0x9E2F1F28U, 0xC4F3A0BCC17BFCA8ULL}
    };

const uint8_t tecmo_gameplay_shot_resolution_expected_metadata[
    TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE] = {
        0x80U,
        TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE,
        TECMO_GAMEPLAY_SHOT_OUTCOME_MISS,
        0x01U,
        0x03U,
        TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT,
        0xF5U, 0x0AU,
        0xF9U, 0x06U,
        0x27U, 0x3BU,
        0x10U, 0x0AU, 0x0AU, 0x01U,
        0x2DU, 0x94U,
        0x34U, 0x94U,
        0x3BU, 0x93U,
        0xEEU, 0xA6U,
        0x3EU, 0xB7U,
        0x0EU, 0xB8U,
        0x7CU, 0xB8U
    };

const uint8_t tecmo_gameplay_shot_resolution_expected_routes[
    TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE] = {
        0U, TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708, 0U, 0U,
        0x08U, 0xA7U, 0U, 0U,
        1U, TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A7A9, 0U, 0U,
        0xA9U, 0xA7U, 1U, 0U,
        2U, TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A8E9, 0U, 0U,
        0xE9U, 0xA8U, 2U, 0U,
        3U, TECMO_GAMEPLAY_SHOT_RIM_ROUTE_A708, 0U, 0U,
        0x08U, 0xA7U, 3U, 0U
    };

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t bank05_cpu_offset(uint64_t prg_offset, uint16_t cpu)
{
    return prg_offset +
           (uint64_t)TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_BANK *
               TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
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

static void store_u64(uint8_t *bytes, uint64_t value)
{
    for (unsigned index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static bool bytes_match(const uint8_t *rom,
                        uint64_t rom_size,
                        uint64_t prg_offset,
                        uint16_t cpu,
                        const uint8_t *expected,
                        size_t count)
{
    uint64_t offset = bank05_cpu_offset(prg_offset, cpu);
    return range_ok(offset, count, rom_size) &&
           memcmp(rom + (size_t)offset, expected, count) == 0;
}

static int validate_derived_contract(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint8_t *routes,
                                     char *message,
                                     size_t message_size)
{
    static const uint8_t outcome_gate[] = {
        0xA5U, 0x9AU, 0xC5U, 0x6AU, 0x90U, 0x06U
    };
    static const uint8_t clear_helper[] = {
        0xA5U, 0xBAU, 0x29U, 0x7FU, 0x85U, 0xBAU, 0x60U
    };
    static const uint8_t set_helper[] = {
        0xA5U, 0xBAU, 0x09U, 0x80U, 0x85U, 0xBAU, 0x60U
    };
    static const uint8_t dispatch_prefix[] = {
        0xA5U, 0x6AU, 0x29U, 0x03U, 0xA8U,
        0xB9U, 0x00U, 0xA7U, 0x85U, 0x94U,
        0xB9U, 0x04U, 0xA7U, 0x85U, 0x95U,
        0x6CU, 0x94U, 0x00U
    };
    static const uint8_t horizontal_positive_limit[] = {
        0xC9U, 0x0BU, 0xB0U, 0x22U
    };
    static const uint8_t horizontal_negative_limit[] = {
        0xC9U, 0xF5U, 0x90U, 0x15U
    };
    static const uint8_t depth_positive_limit[] = {
        0xC9U, 0x07U, 0xB0U, 0x07U
    };
    static const uint8_t depth_negative_limit[] = {
        0xC9U, 0xF9U, 0xB0U, 0x03U
    };
    static const uint8_t airborne_altitude_limit[] = {
        0xC9U, 0x3CU, 0xB0U, 0xEBU
    };
    static const uint8_t grounded_altitude_limit[] = {
        0xC9U, 0x28U, 0xB0U, 0xE2U
    };
    static const uint8_t settlement_team_gate[] = {
        0xBDU, 0xB0U, 0x04U, 0x29U, 0x10U, 0xF0U, 0x2DU
    };
    uint64_t table_offset = bank05_cpu_offset(prg_offset, 0xA700U);

    if (!bytes_match(rom, rom_size, prg_offset, 0x933BU,
                     outcome_gate, sizeof(outcome_gate)) ||
        !bytes_match(rom, rom_size, prg_offset, 0x942DU,
                     clear_helper, sizeof(clear_helper)) ||
        !bytes_match(rom, rom_size, prg_offset, 0x9434U,
                     set_helper, sizeof(set_helper)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xA6EEU,
                     dispatch_prefix, sizeof(dispatch_prefix)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB828U,
                     horizontal_positive_limit,
                     sizeof(horizontal_positive_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB835U,
                     horizontal_negative_limit,
                     sizeof(horizontal_negative_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB843U,
                     depth_positive_limit,
                     sizeof(depth_positive_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB84AU,
                     depth_negative_limit,
                     sizeof(depth_negative_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB85FU,
                     airborne_altitude_limit,
                     sizeof(airborne_altitude_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB868U,
                     grounded_altitude_limit,
                     sizeof(grounded_altitude_limit)) ||
        !bytes_match(rom, rom_size, prg_offset, 0xB8C2U,
                     settlement_team_gate, sizeof(settlement_team_gate)) ||
        !range_ok(table_offset, 8U, rom_size)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGSR-1 derived outcome/route/claimant contract mismatch.");
        return -1;
    }

    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT; ++index) {
        uint16_t target = (uint16_t)(rom[(size_t)table_offset + index] |
            ((uint16_t)rom[(size_t)table_offset + 4U + index] << 8U));
        uint8_t *record = routes +
            index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE;
        if (target != (uint16_t)(
                tecmo_gameplay_shot_resolution_expected_routes[
                    index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE +
                    4U] |
                ((uint16_t)tecmo_gameplay_shot_resolution_expected_routes[
                    index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE +
                    5U] << 8U))) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGSR-1 numeric rim-route target mismatch.");
            return -1;
        }
        memcpy(record,
               tecmo_gameplay_shot_resolution_expected_routes +
                   index *
                       TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE,
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE);
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_shot_resolution(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayShotResolutionProvenance *provenance,
    char *message,
    size_t message_size)
{
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE ||
        prg_banks != SHOT_RESOLUTION_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGSR-1 import requires the exact Rev1 ROM contract.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        const TecmoGameplayShotResolutionExpectedSource *expected =
            &tecmo_gameplay_shot_resolution_expected_sources[index];
        uint64_t source_offset = bank05_cpu_offset(
            prg_offset, expected->cpu_start);
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        uint32_t fingerprint32;
        uint64_t fingerprint64;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE;
        if (expected->cpu_start < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
            cpu_end >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
            !range_ok(source_offset, expected->byte_count, rom_size)) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGSR-1 source range is outside the Rev1 ROM.");
            return -1;
        }
        fingerprint32 = tecmo_asset_pack_fnv1a32(
            rom + (size_t)source_offset, expected->byte_count);
        fingerprint64 = fnv1a64(
            rom + (size_t)source_offset, expected->byte_count);
        if (fingerprint32 != expected->fingerprint_fnv1a32 ||
            fingerprint64 != expected->fingerprint_fnv1a64) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGSR-1 Bank05 $%04X-$%04X revision fingerprint mismatch.",
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_BANK;
        record[3U] = 0U;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, fingerprint32);
        store_u64(record + 16U, fingerprint64);
        tecmo_asset_pack_store_u16(record + 24U, (uint16_t)index);
        provenance->source_offsets[index] = source_offset;
    }

    memcpy(payload +
               TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET,
           tecmo_gameplay_shot_resolution_expected_metadata,
           TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE);
    if (validate_derived_contract(
            rom, rom_size, prg_offset,
            payload + TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET,
            message, message_size) != 0) {
        return -1;
    }

    memcpy(payload, "TGSR", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 32U, TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 34U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 44U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A32);
    store_u64(
        payload + 48U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A64);
    tecmo_asset_pack_store_u32(
        payload + 56U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A32);
    store_u64(
        payload + 60U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A64);
    tecmo_asset_pack_store_u16(payload + 80U, 0x942DU);
    tecmo_asset_pack_store_u16(payload + 82U, 0x9434U);
    payload[84U] = 1U;

    if (tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A32 ||
        fnv1a64(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A64 ||
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET,
            TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A32 ||
        fnv1a64(
            payload + TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET,
            TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A64 ||
        tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 ||
        fnv1a64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A64) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGSR-1 canonical derived payload fingerprint mismatch.");
        return -1;
    }

    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGSR-1 shot-resolution asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_shot_resolution_self_test(
    char *message,
    size_t message_size)
{
    uint32_t prior_end = 0U;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        const TecmoGameplayShotResolutionExpectedSource *source =
            &tecmo_gameplay_shot_resolution_expected_sources[index];
        uint32_t end = (uint32_t)source->cpu_start +
                       source->byte_count - 1U;
        if (source->kind !=
                (TecmoGameplayShotResolutionSourceKind)(index + 1U) ||
            source->cpu_start < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
            end >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
            (index != 0U && source->cpu_start <= prior_end) ||
            source->fingerprint_fnv1a32 == 0U ||
            source->fingerprint_fnv1a64 == 0U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGSR-1 source layout self-test failed.");
            return -1;
        }
        prior_end = end;
    }
    if (tecmo_gameplay_shot_resolution_expected_metadata[0U] != 0x80U ||
        tecmo_gameplay_shot_resolution_expected_metadata[1U] !=
            TECMO_GAMEPLAY_SHOT_OUTCOME_MAKE ||
        tecmo_gameplay_shot_resolution_expected_metadata[2U] !=
            TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
        tecmo_gameplay_shot_resolution_expected_metadata[3U] != 1U ||
        tecmo_gameplay_shot_resolution_expected_metadata[6U] != 0xF5U ||
        tecmo_gameplay_shot_resolution_expected_metadata[7U] != 0x0AU ||
        tecmo_gameplay_shot_resolution_expected_metadata[8U] != 0xF9U ||
        tecmo_gameplay_shot_resolution_expected_metadata[9U] != 0x06U ||
        tecmo_gameplay_shot_resolution_expected_metadata[10U] != 0x27U ||
        tecmo_gameplay_shot_resolution_expected_metadata[11U] != 0x3BU ||
        memcmp(tecmo_gameplay_shot_resolution_expected_routes,
               (const uint8_t[]){
                   0U, 1U, 0U, 0U, 0x08U, 0xA7U, 0U, 0U,
                   1U, 2U, 0U, 0U, 0xA9U, 0xA7U, 1U, 0U,
                   2U, 3U, 0U, 0U, 0xE9U, 0xA8U, 2U, 0U,
                   3U, 1U, 0U, 0U, 0x08U, 0xA7U, 3U, 0U},
               sizeof(tecmo_gameplay_shot_resolution_expected_routes)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGSR-1 semantic layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGSR-1 layout self-test passed.");
    return 0;
}
