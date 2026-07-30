#include "tecmo_asset_pack_gameplay_court_orientation.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_shot_resolution.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define COURT_ORIENTATION_BANK 5U
#define COURT_ORIENTATION_PRG_BANK_COUNT 8U
#define COURT_ORIENTATION_CHR_BANK_COUNT 32U
#define COURT_ORIENTATION_REV1_ROM_SIZE 393232U
#define COURT_ORIENTATION_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t court_orientation_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t court_orientation_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayCourtOrientationExpectedSource
    tecmo_gameplay_court_orientation_expected_sources[
        TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_POSSESSION_GATE_AND_SWAP,
         COURT_ORIENTATION_BANK, 0x8FADU,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET},
        {TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_ACTOR_ROLE_TOGGLE,
         COURT_ORIENTATION_BANK, 0x9042U,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET},
        {TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_TARGET_DELTA,
         COURT_ORIENTATION_BANK, 0x9054U,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET},
        {TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_TARGET_TABLE,
         COURT_ORIENTATION_BANK, 0xBDEFU,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayCourtOrientationExpectedSource *source)
{
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int validate_semantics(const uint8_t *payload)
{
    static const uint8_t gate_prefix[] = {
        0xADU,0xA1U,0x05U,0xD0U,0x06U,0xA5U,
        0xBAU,0x29U,0x03U,0xF0U,0x01U,0x60U
    };
    static const uint8_t accepted_toggle[] = {
        0xADU,0x5AU,0x03U,0x8DU,0x5BU,0x03U,
        0x49U,0x01U,0x8DU,0x5AU,0x03U
    };
    static const uint8_t role_toggle[] = {
        0xA2U,0x09U,0xBDU,0xB0U,0x04U,0x49U,0x10U,
        0x9DU,0xB0U,0x04U,0xCAU,0x10U,0xF5U,
        0xA9U,0x17U,0x4CU,0x11U,0xC7U
    };
    static const uint8_t delta_prefix[] = {
        0xACU,0x5AU,0x03U,0xB5U,0x73U,0x38U,
        0xF9U,0xEFU,0xBDU,0x8DU,0x6AU,0x03U
    };
    static const uint8_t y_delta[] = {
        0xB5U,0xF3U,0x38U,0xE9U,0x94U
    };
    static const uint8_t targets[] = {0xA0U,0x60U,0x00U,0x02U};
    const uint8_t *gate = payload +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET;
    const uint8_t *role = payload +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET;
    const uint8_t *delta = payload +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET;
    const uint8_t *target = payload +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET;
    size_t accepted_offset = 0x8FBCU - 0x8FADU;
    size_t y_offset = 0x9084U - 0x9054U;

    return memcmp(gate, gate_prefix, sizeof(gate_prefix)) == 0 &&
           memcmp(gate + accepted_offset, accepted_toggle,
                  sizeof(accepted_toggle)) == 0 &&
           memcmp(role, role_toggle, sizeof(role_toggle)) == 0 &&
           memcmp(delta, delta_prefix, sizeof(delta_prefix)) == 0 &&
           memcmp(delta + y_offset, y_delta, sizeof(y_delta)) == 0 &&
           delta[
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_SIZE -
               1U] == 0x60U &&
           memcmp(target, targets, sizeof(targets)) == 0;
}

int tecmo_asset_pack_build_gameplay_court_orientation(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCourtOrientationProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE ||
        prg_banks != COURT_ORIENTATION_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != COURT_ORIENTATION_REV1_ROM_SIZE ||
        prg_offset != sizeof(court_orientation_rev1_ines_header) ||
        memcmp(rom, court_orientation_rev1_ines_header,
               sizeof(court_orientation_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, court_orientation_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGOR-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtOrientationExpectedSource *expected =
            &tecmo_gameplay_court_orientation_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE;
        if (expected->bank >= prg_banks || cpu_end >= 0xC000U ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGOR-1 Bank05 $%04X-$%04X fingerprint mismatch.",
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = 0U;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->fingerprint);
        tecmo_asset_pack_store_u32(record + 16U, expected->payload_offset);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)offset, expected->byte_count);
        provenance->source_offsets[index] = offset;
    }

    if (!validate_semantics(payload) ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            COURT_ORIENTATION_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGOR-1 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGOR", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 20U, TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 22U, TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 36U,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 40U, COURT_ORIENTATION_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 44U, COURT_ORIENTATION_REV1_ROM_FNV1A32);
    memcpy(payload + 48U, court_orientation_rev1_sha256,
           sizeof(court_orientation_rev1_sha256));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtOrientationExpectedSource *expected =
            &tecmo_gameplay_court_orientation_expected_sources[index];
        uint8_t *descriptor = payload + 80U + index * 12U;
        tecmo_asset_pack_store_u32(descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(
            descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            descriptor + 8U, expected->fingerprint);
    }
    {
        const uint8_t *target_table = payload +
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET;
        tecmo_asset_pack_store_u16(
            payload + 128U,
            (uint16_t)((uint16_t)target_table[0U] |
                       ((uint16_t)target_table[2U] << 8U)));
        tecmo_asset_pack_store_u16(
            payload + 130U,
            (uint16_t)((uint16_t)target_table[1U] |
                       ((uint16_t)target_table[3U] << 8U)));
    }
    payload[132U] = 0x94U;
    payload[133U] = 0U;
    payload[134U] = TECMO_GAMEPLAY_COURT_ORIENTATION_TEAM_AWAY;
    payload[135U] = 0x10U;
    payload[136U] = 0x17U;
    payload[137U] = 0x1BU;
    payload[138U] = 0x2EU;
    payload[139U] = 0x07U;

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGOR-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGOR-1 court-orientation asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_court_orientation_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(court_orientation_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)COURT_ORIENTATION_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)COURT_ORIENTATION_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE];
    uint8_t input_sha256[32];
    TecmoGameplayCourtOrientationProvenance provenance;
    int result;

    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGOR-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != COURT_ORIENTATION_REV1_ROM_SIZE ||
        memcmp(rom, court_orientation_rev1_ines_header,
               sizeof(court_orientation_rev1_ines_header)) != 0 ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGOR-1 direct source test requires the exact Rev1 iNES layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_court_orientation(
        rom, rom_size, prg_offset, COURT_ORIENTATION_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    if (result == 0 &&
        (tecmo_asset_pack_sha256_digest(
             rom, (size_t)rom_size, input_sha256) != 0 ||
         memcmp(input_sha256, court_orientation_rev1_sha256,
                sizeof(input_sha256)) != 0)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGOR-1 direct source test full-ROM SHA-256 mismatch.");
        result = -1;
    }
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_court_orientation_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE];
    TecmoGameplayCourtOrientationProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET +
                TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE ||
        tecmo_asset_pack_build_gameplay_court_orientation(
            truncated_rom, sizeof(truncated_rom), 16U,
            COURT_ORIENTATION_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGOR-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGOR-1 layout self-test passed.");
    return 0;
}
