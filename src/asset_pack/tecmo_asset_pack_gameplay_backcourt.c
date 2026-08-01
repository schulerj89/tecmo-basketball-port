#include "tecmo_asset_pack_gameplay_backcourt.h"

#include "tecmo_asset_pack_gameplay_court_orientation.h"
#include "tecmo_asset_pack_gameplay_penalties.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define BACKCOURT_PRG_BANK_COUNT 8U
#define BACKCOURT_REV1_ROM_SIZE 393232U
#define BACKCOURT_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t backcourt_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t backcourt_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayBackcourtExpectedSource
    tecmo_gameplay_backcourt_expected_sources[
        TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_BACKCOURT_SOURCE_DETECTOR, 5U, 0U, 0x970BU,
         TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET}
    };

const uint8_t tecmo_gameplay_backcourt_expected_rules[
    TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE] = {
    0x00U,0x10U,0x02U,0x02U,0x78U,0x01U,0x0AU,0x00U,
    0x88U,0x01U,0xF8U,0x00U,0x14U,0x00U,0x68U,0x00U,
    0x0BU,0x97U,0x1FU,0x97U,0x86U,0x97U,0x42U,0x07U,
    0x88U,0x05U,0x5AU,0x03U,0x78U,0x04U,0x7DU,0xF2U
};

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayBackcourtExpectedSource *source)
{
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int detector_semantics_valid(const uint8_t *detector)
{
    static const uint8_t ten_second_prefix[20] = {
        0xADU,0x88U,0x05U,0x29U,0x10U,0xD0U,0x0CU,0xADU,
        0x43U,0x07U,0xC9U,0x0AU,0x90U,0x05U,0xA9U,0x04U,
        0x8DU,0x42U,0x07U,0xADU
    };
    static const uint8_t orientation_gate[7] = {
        0x78U,0x04U,0xD0U,0x47U,0xADU,0x5AU,0x03U
    };
    static const uint8_t set_frontcourt[8] = {
        0xA9U,0x10U,0x0DU,0x88U,0x05U,0x8DU,0x88U,0x05U
    };
    static const uint8_t selector_two[6] = {
        0xA9U,0x02U,0x8DU,0x42U,0x07U,0x60U
    };
    return detector != NULL &&
           memcmp(detector, ten_second_prefix,
                  sizeof(ten_second_prefix)) == 0 &&
           memcmp(detector + 20U, orientation_gate,
                  sizeof(orientation_gate)) == 0 &&
           memcmp(detector + 44U, set_frontcourt,
                  sizeof(set_frontcourt)) == 0 &&
           memcmp(detector + 90U, selector_two,
                  sizeof(selector_two)) == 0 &&
           memcmp(detector + 118U, selector_two,
                  sizeof(selector_two)) == 0;
}

int tecmo_asset_pack_build_gameplay_backcourt(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayBackcourtProvenance *provenance,
    char *message,
    size_t message_size)
{
    const TecmoGameplayBackcourtExpectedSource *source =
        &tecmo_gameplay_backcourt_expected_sources[0U];
    uint8_t input_sha256[32];
    uint8_t *record;
    uint64_t offset;
    uint32_t cpu_end;
    uint32_t payload_fingerprint;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE ||
        prg_banks != BACKCOURT_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != BACKCOURT_REV1_ROM_SIZE ||
        prg_offset != sizeof(backcourt_rev1_ines_header) ||
        memcmp(rom, backcourt_rev1_ines_header,
               sizeof(backcourt_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, backcourt_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBC-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    offset = source_offset(prg_offset, source);
    cpu_end = (uint32_t)source->cpu_start + source->byte_count - 1U;
    if (source->bank >= prg_banks || source->fixed_bank != 0U ||
        source->cpu_start < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
        cpu_end >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
        !range_ok(offset, source->byte_count, rom_size) ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)offset, source->byte_count) !=
                source->fingerprint ||
        !detector_semantics_valid(rom + (size_t)offset) ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            BACKCOURT_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBC-1 Bank05 $970B-$9786 source contract rejected.");
        return -1;
    }
    provenance->source_offsets[0U] = offset;

    memcpy(payload, "TGBC", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 36U, BACKCOURT_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(payload + 40U, BACKCOURT_REV1_ROM_FNV1A32);
    memcpy(payload + 44U, backcourt_rev1_sha256,
           sizeof(backcourt_rev1_sha256));
    payload[76U] = 0U;
    payload[77U] = 0x10U;
    payload[78U] = 2U;
    payload[79U] = TECMO_GAMEPLAY_BACKCOURT_ORIENTATION_COUNT;
    tecmo_asset_pack_store_u16(payload + 80U, 0x0178U);
    payload[82U] = 0x0AU;
    tecmo_asset_pack_store_u16(payload + 84U, 0x0188U);
    payload[86U] = 0xF8U;
    tecmo_asset_pack_store_u16(payload + 88U, 20U);
    tecmo_asset_pack_store_u16(payload + 90U, 104U);
    tecmo_asset_pack_store_u32(
        payload + 92U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 96U, TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE);
    tecmo_asset_pack_store_u32(payload + 100U, source->payload_offset);
    tecmo_asset_pack_store_u32(payload + 104U, source->byte_count);
    tecmo_asset_pack_store_u32(payload + 108U, source->fingerprint);

    record = payload + TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET;
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
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET,
           tecmo_gameplay_backcourt_expected_rules,
           sizeof(tecmo_gameplay_backcourt_expected_rules));

    payload_fingerprint =
        tecmo_asset_pack_fnv1a32(payload, payload_size);
    if (payload_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGBC-1 canonical payload fingerprint mismatch (got %08X).",
            payload_fingerprint);
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGBC-1 backcourt detector asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_backcourt_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE];
    TecmoGameplayBackcourtProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBC-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_backcourt(
        rom, rom_size, sizeof(backcourt_rev1_ines_header),
        BACKCOURT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
        &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_backcourt_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE];
    TecmoGameplayBackcourtProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET +
                TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE ||
        tecmo_asset_pack_build_gameplay_backcourt(
            truncated_rom, sizeof(truncated_rom), 16U,
            BACKCOURT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGBC-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGBC-1 layout self-test passed.");
    return 0;
}
