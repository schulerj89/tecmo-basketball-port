#include "tecmo_asset_pack_gameplay_hud.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define GAMEPLAY_HUD_PRG_BANK_COUNT 8U
#define GAMEPLAY_HUD_CHR_BANK_COUNT 32U
#define GAMEPLAY_HUD_REV1_ROM_SIZE 393232U
#define GAMEPLAY_HUD_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t gameplay_hud_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t gameplay_hud_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayHudExpectedSource
    tecmo_gameplay_hud_expected_sources[TECMO_GAMEPLAY_HUD_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LAYOUT, 1U, 0U, 0xBDF0U,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET},
        {TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LABELS, 1U, 0U, 0xBE1FU,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET},
        {TECMO_GAMEPLAY_HUD_SOURCE_TEXT_FORMAT, 2U, 0U, 0xAF64U,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayHudExpectedSource *source)
{
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static void store_u64(uint8_t *bytes, uint64_t value)
{
    tecmo_asset_pack_store_u32(bytes, (uint32_t)value);
    tecmo_asset_pack_store_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static int semantic_sources_valid(const uint8_t *payload)
{
    const uint8_t *layout = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET;
    const uint8_t *labels = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET;
    const uint8_t *text = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET;
    const uint8_t *font = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET;
    static const uint8_t layout_prefix[8] = {
        0xA0U,0x00U,0x20U,0xF7U,0xBDU,0xA0U,0x01U,0xA9U
    };
    static const uint8_t name_prefix[12] = {
        0xA0U,0x00U,0xB1U,0x18U,0xAAU,0xBDU,
        0xE6U,0xAFU,0x8DU,0x07U,0x20U,0xA9U
    };
    size_t index;

    if (memcmp(layout, layout_prefix, sizeof(layout_prefix)) != 0 ||
        layout[45U] != 0x41U || layout[46U] != 0x57U ||
        memcmp(text + (0xAF96U - 0xAF64U), name_prefix,
               sizeof(name_prefix)) != 0 ||
        text[0xAFA2U - 0xAF64U] != 0x81U ||
        font[0U] != 0xFFU || font['.' - TECMO_GAMEPLAY_HUD_FONT_FIRST] !=
            0x81U) {
        return 0;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_TEAM_COUNT; ++index) {
        if (labels[index] != index * TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) {
            return 0;
        }
    }
    for (index = 0U; index < 10U; ++index) {
        if (font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST + index] !=
            0x82U + index ||
            text[(0xB051U - 0xAF64U) + index] != 0x72U + index) {
            return 0;
        }
    }
    for (index = 0U; index < 26U; ++index) {
        if (font['A' - TECMO_GAMEPLAY_HUD_FONT_FIRST + index] !=
                0x8CU + index ||
            text[(0xB062U - 0xAF64U) + index] != 0x7CU + index) {
            return 0;
        }
    }
    return 1;
}

int tecmo_asset_pack_build_gameplay_hud(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayHudProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    size_t index;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE ||
        prg_banks != GAMEPLAY_HUD_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != GAMEPLAY_HUD_REV1_ROM_SIZE ||
        prg_offset != sizeof(gameplay_hud_rev1_ines_header) ||
        memcmp(rom, gameplay_hud_rev1_ines_header,
               sizeof(gameplay_hud_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, gameplay_hud_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "THUD-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *expected =
            &tecmo_gameplay_hud_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE;
        if (expected->bank >= prg_banks || expected->fixed_bank != 0U ||
            expected->cpu_start < 0x8000U || cpu_end >= 0xC000U ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "THUD-1 Bank%02u $%04X-$%04X fingerprint mismatch.",
                (unsigned)expected->bank, (unsigned)expected->cpu_start,
                (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->fingerprint);
        tecmo_asset_pack_store_u32(record + 16U,
                                   expected->payload_offset);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)offset, expected->byte_count);
        provenance->source_offsets[index] = offset;
    }

    if (!semantic_sources_valid(payload) ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            GAMEPLAY_HUD_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "THUD-1 source relationships or full-ROM revision rejected.");
        return -1;
    }

    memcpy(payload, "THUD", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_HUD_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_HUD_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_HUD_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 20U, TECMO_GAMEPLAY_HUD_TEAM_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 22U, TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH);
    tecmo_asset_pack_store_u16(
        payload + 24U, TECMO_GAMEPLAY_HUD_FONT_FIRST);
    tecmo_asset_pack_store_u16(
        payload + 26U, TECMO_GAMEPLAY_HUD_FONT_COUNT);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_TILE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, 0U);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 48U, TECMO_ASSET_PACK_TEAM_DATA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_TEAM_DATA_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 56U, TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 60U, TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32);
    store_u64(
        payload + 64U, TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64);
    tecmo_asset_pack_store_u32(
        payload + 72U, GAMEPLAY_HUD_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 76U, GAMEPLAY_HUD_REV1_ROM_FNV1A32);
    memcpy(payload + 80U, gameplay_hud_rev1_sha256,
           sizeof(gameplay_hud_rev1_sha256));
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *expected =
            &tecmo_gameplay_hud_expected_sources[index];
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_STRIDE;
        tecmo_asset_pack_store_u32(descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(
            descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            descriptor + 8U, expected->fingerprint);
    }

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "THUD-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived THUD-1 gameplay HUD asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_hud_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(gameplay_hud_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)GAMEPLAY_HUD_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)GAMEPLAY_HUD_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE];
    TecmoGameplayHudProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "THUD-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != GAMEPLAY_HUD_REV1_ROM_SIZE ||
        memcmp(rom, gameplay_hud_rev1_ines_header,
               sizeof(gameplay_hud_rev1_ines_header)) != 0 ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "THUD-1 direct source test requires the exact Rev1 layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_hud(
        rom, rom_size, prg_offset, GAMEPLAY_HUD_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_hud_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE];
    TecmoGameplayHudProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET +
                TECMO_GAMEPLAY_HUD_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_TILE_OFFSET +
                TECMO_GAMEPLAY_HUD_TEAM_COUNT *
                    TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH >
            TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET +
                TECMO_GAMEPLAY_HUD_FONT_COUNT >
            TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE ||
        tecmo_asset_pack_build_gameplay_hud(
            truncated_rom, sizeof(truncated_rom), 16U,
            GAMEPLAY_HUD_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "THUD-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "THUD-1 layout self-test passed.");
    return 0;
}
