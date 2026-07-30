#include "tecmo_asset_pack_gameplay_free_throw_lineup.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define FREE_THROW_LINEUP_BANK 6U
#define FREE_THROW_LINEUP_PRG_BANK_COUNT 8U
#define FREE_THROW_LINEUP_CHR_BANK_COUNT 32U
#define FREE_THROW_LINEUP_REV1_ROM_SIZE 393232U
#define FREE_THROW_LINEUP_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t free_throw_lineup_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t free_throw_lineup_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayFreeThrowLineupExpectedSource
    tecmo_gameplay_free_throw_lineup_expected_sources[
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_POSE_LOOKUP,
         FREE_THROW_LINEUP_BANK, 0x88B0U, 42U, 0xAD834719U,
         TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET},
        {TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_ROUND_SETUP,
         FREE_THROW_LINEUP_BANK, 0x9621U, 334U, 0x998D84B8U,
         TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET},
        {TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_FOLLOWUP,
         FREE_THROW_LINEUP_BANK, 0x976FU, 238U, 0xFB7680EFU,
         TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET},
        {TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_TABLES,
         FREE_THROW_LINEUP_BANK, 0x985DU, 188U, 0xAFB31306U,
         TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayFreeThrowLineupExpectedSource *source)
{
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int validate_table_relationships(const uint8_t *tables)
{
    static const uint16_t position_pointers[2] = {0x9889U, 0x989BU};
    static const uint16_t height_pointers[2] = {0x98ADU, 0x98BFU};
    static const uint8_t pose_seeds[2] = {17U, 8U};
    static const uint8_t shooter_directions[2] = {1U, 0U};
    for (size_t orientation = 0U;
         orientation < TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT;
         ++orientation) {
        uint16_t position_pointer = tecmo_asset_pack_read_u16(
            tables + (0x987DU - 0x985DU) + orientation * 2U);
        uint16_t height_pointer = tecmo_asset_pack_read_u16(
            tables + (0x9885U - 0x985DU) + orientation * 2U);
        size_t position_offset;
        size_t height_offset;
        if (position_pointer != position_pointers[orientation] ||
            height_pointer != height_pointers[orientation] ||
            tables[(0x9863U - 0x985DU) + orientation] !=
                pose_seeds[orientation] ||
            tables[(0x9877U - 0x985DU) + orientation] !=
                shooter_directions[orientation] ||
            position_pointer < 0x985DU || height_pointer < 0x985DU) {
            return -1;
        }
        position_offset = (size_t)(position_pointer - 0x985DU);
        height_offset = (size_t)(height_pointer - 0x985DU);
        if (position_offset >
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE -
                    18U ||
            height_offset >
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE -
                    18U) {
            return -1;
        }
    }
    for (size_t index = 0U; index < 18U; ++index) {
        if (tables[(0x9865U - 0x985DU) + index] >= 8U) return -1;
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_free_throw_lineup(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayFreeThrowLineupProvenance *provenance,
    char *message,
    size_t message_size)
{
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE ||
        prg_banks != FREE_THROW_LINEUP_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != FREE_THROW_LINEUP_REV1_ROM_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT; ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *expected =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE;
        if (expected->bank >= prg_banks || cpu_end > 0xFFFFU ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGFL-1 Bank06 $%04X-$%04X fingerprint mismatch.",
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
    if (validate_table_relationships(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET) !=
        0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 lineup table relationships rejected.");
        return -1;
    }
    if (tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            FREE_THROW_LINEUP_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 full-ROM FNV1a32 mismatch for target Rev1.");
        return -1;
    }

    memcpy(payload, "TGFL", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U,
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U,
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U,
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 20U, TECMO_GAMEPLAY_FREE_THROW_LINEUP_ACTOR_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 22U, TECMO_GAMEPLAY_FREE_THROW_LINEUP_ORIENTATION_COUNT);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 32U, FREE_THROW_LINEUP_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 36U, FREE_THROW_LINEUP_REV1_ROM_FNV1A32);
    memcpy(payload + 40U, free_throw_lineup_rev1_sha256,
           sizeof(free_throw_lineup_rev1_sha256));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT; ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *expected =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        uint8_t *descriptor = payload + 72U + index * 12U;
        tecmo_asset_pack_store_u32(
            descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(
            descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            descriptor + 8U, expected->fingerprint);
    }
    payload[120U] = FREE_THROW_LINEUP_BANK;
    payload[121U] = 0U; /* shooter pose preserved/undefined */
    payload[122U] = 16U; /* raw world-X width */
    payload[123U] = 8U;  /* raw world-Y width */
    payload[124U] = 2U;  /* raw pose byte offset to TGPL index divisor */

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGFL-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGFL-1 free-throw lineup asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_free_throw_lineup_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(free_throw_lineup_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)FREE_THROW_LINEUP_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)FREE_THROW_LINEUP_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE];
    uint8_t input_sha256[32];
    TecmoGameplayFreeThrowLineupProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != FREE_THROW_LINEUP_REV1_ROM_SIZE ||
        memcmp(rom, free_throw_lineup_rev1_ines_header,
               sizeof(free_throw_lineup_rev1_ines_header)) != 0 ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 direct source test requires the exact Rev1 iNES layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_free_throw_lineup(
        rom, rom_size, prg_offset, FREE_THROW_LINEUP_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    if (result == 0 &&
        (tecmo_asset_pack_sha256_digest(
             rom, (size_t)rom_size, input_sha256) != 0 ||
         memcmp(input_sha256, free_throw_lineup_rev1_sha256,
                sizeof(input_sha256)) != 0)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 direct source test full-ROM SHA-256 mismatch.");
        result = -1;
    }
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_free_throw_lineup_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE];
    TecmoGameplayFreeThrowLineupProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET +
                TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE ||
        tecmo_asset_pack_build_gameplay_free_throw_lineup(
            truncated_rom, sizeof(truncated_rom), 16U,
            FREE_THROW_LINEUP_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFL-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGFL-1 layout self-test passed.");
    return 0;
}
