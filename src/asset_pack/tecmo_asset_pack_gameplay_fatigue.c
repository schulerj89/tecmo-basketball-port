#include "tecmo_asset_pack_gameplay_fatigue.h"

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define FATIGUE_PRG_BANK_COUNT 8U
#define FATIGUE_CHR_BANK_COUNT 32U
#define FATIGUE_REV1_ROM_SIZE 393232U
#define FATIGUE_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t fatigue_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t fatigue_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayFatigueExpectedSource
    tecmo_gameplay_fatigue_expected_sources[
        TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_FATIGUE_SOURCE_EVOLUTION, 2U, 0U, 0xB4E6U,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET},
        {TECMO_GAMEPLAY_FATIGUE_SOURCE_LIVE_CALLER, 7U, 1U, 0xED2FU,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayFatigueExpectedSource *source)
{
    uint32_t bank = source->fixed_bank != 0U
                        ? prg_banks - 1U : source->bank;
    uint16_t base = source->fixed_bank != 0U
                        ? 0xC000U
                        : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - base);
}

static int validate_semantics(const uint8_t *payload)
{
    static const uint8_t thresholds[3] = {6U,4U,1U};
    static const uint8_t routine_prefix[] = {
        0x20U,0x2AU,0xC0U,0xADU,0x99U,0x07U,0xD0U,0x38U,
        0xACU,0x5FU,0x07U,0xB9U,0xE6U,0xB4U,0x8DU,0x99U,0x07U
    };
    static const uint8_t caller[] = {
        0xADU,0x59U,0x03U,0xC9U,0x01U,0xF0U,0x01U,0x60U,
        0xA9U,0x02U,0x20U,0x6AU,0xD3U,0x4CU,0xE9U,0xB4U
    };
    static const uint8_t second_team_asymmetry[] = {
        0x9DU,0x6CU,0x7CU,0x9DU,0x84U,0x7CU
    };
    const uint8_t *evolution = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET;
    const uint8_t *live_caller = payload +
        TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_OFFSET;
    return memcmp(evolution, thresholds, sizeof(thresholds)) == 0 &&
           memcmp(evolution + 3U, routine_prefix,
                  sizeof(routine_prefix)) == 0 &&
           memcmp(evolution + 216U, second_team_asymmetry,
                  sizeof(second_team_asymmetry)) == 0 &&
           memcmp(live_caller, caller, sizeof(caller)) == 0;
}

int tecmo_asset_pack_build_gameplay_fatigue(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayFatigueProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE ||
        prg_banks != FATIGUE_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != FATIGUE_REV1_ROM_SIZE ||
        prg_offset != sizeof(fatigue_rev1_ines_header) ||
        memcmp(rom, fatigue_rev1_ines_header,
               sizeof(fatigue_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, fatigue_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFT-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }
    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *source =
            &tecmo_gameplay_fatigue_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, source);
        uint32_t cpu_end =
            (uint32_t)source->cpu_start + source->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE;
        if (source->bank >= prg_banks ||
            (source->fixed_bank == 0U && cpu_end >= 0xC000U) ||
            (source->fixed_bank != 0U && cpu_end > 0xFFFFU) ||
            !range_ok(offset, source->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, source->byte_count) !=
                    source->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGFT-1 %s $%04X-$%04X fingerprint mismatch.",
                source->fixed_bank != 0U ? "fixed" : "Bank02",
                (unsigned)source->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)source->kind);
        record[2U] = source->bank;
        record[3U] = source->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, source->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, source->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, source->fingerprint);
        tecmo_asset_pack_store_u32(record + 16U, source->payload_offset);
        memcpy(payload + source->payload_offset,
               rom + (size_t)offset, source->byte_count);
        provenance->source_offsets[index] = offset;
    }
    if (!validate_semantics(payload) ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            FATIGUE_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFT-1 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGFT", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_TEAM_DATA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_TEAM_DATA_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 28U, FATIGUE_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(payload + 32U, FATIGUE_REV1_ROM_FNV1A32);
    memcpy(payload + 36U, fatigue_rev1_sha256,
           sizeof(fatigue_rev1_sha256));
    payload[68U] = TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT;
    payload[69U] = TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
    payload[70U] = TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT;
    payload[71U] = TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT;
    payload[72U] = 3U;
    payload[73U] = 100U;
    payload[74U] = 4U;
    payload[75U] = 30U;
    memcpy(payload + 76U,
           payload + TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET,
           TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT);
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *source =
            &tecmo_gameplay_fatigue_expected_sources[index];
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_STRIDE;
        tecmo_asset_pack_store_u32(descriptor, source->payload_offset);
        tecmo_asset_pack_store_u32(descriptor + 4U, source->byte_count);
        tecmo_asset_pack_store_u32(descriptor + 8U, source->fingerprint);
    }
    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGFT-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGFT-1 fatigue asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_fatigue_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE];
    TecmoGameplayFatigueProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGFT-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_fatigue(
        rom, rom_size, sizeof(fatigue_rev1_ines_header),
        FATIGUE_PRG_BANK_COUNT, 1, payload, sizeof(payload),
        &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_fatigue_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE];
    TecmoGameplayFatigueProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET +
                TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE ||
        tecmo_asset_pack_build_gameplay_fatigue(
            truncated_rom, sizeof(truncated_rom), 16U,
            FATIGUE_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGFT-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGFT-1 layout self-test passed.");
    return 0;
}
