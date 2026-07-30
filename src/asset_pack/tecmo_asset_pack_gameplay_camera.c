#include "tecmo_asset_pack_gameplay_camera.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_court.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define GAMEPLAY_CAMERA_FIXED_BANK 7U
#define GAMEPLAY_CAMERA_PRG_BANK_COUNT 8U
#define GAMEPLAY_CAMERA_CHR_BANK_COUNT 32U
#define GAMEPLAY_CAMERA_FIXED_CPU_BASE 0xC000U
#define GAMEPLAY_CAMERA_REV1_ROM_SIZE 393232U
#define GAMEPLAY_CAMERA_REV1_ROM_FNV1A32 0x0650F5B0U
#define GAMEPLAY_CAMERA_FORCED_SETTLE_LIMIT 1024U

static const uint8_t gameplay_camera_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t gameplay_camera_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayCameraExpectedSource
    tecmo_gameplay_camera_expected_sources[
        TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_CAMERA_SOURCE_INITIALIZE,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xDE13U, 26U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET},
        {TECMO_GAMEPLAY_CAMERA_SOURCE_STREAM_COLUMNS,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xDF05U, 251U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET},
        {TECMO_GAMEPLAY_CAMERA_SOURCE_ATTRIBUTE_QUADRANTS,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xE0E7U, 85U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET},
        {TECMO_GAMEPLAY_CAMERA_SOURCE_FOLLOW,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xE168U, 383U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET},
        {TECMO_GAMEPLAY_CAMERA_SOURCE_FORCED_SETTLE,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xEB4FU, 62U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET},
        {TECMO_GAMEPLAY_CAMERA_SOURCE_ACTOR_PROJECTION,
         GAMEPLAY_CAMERA_FIXED_BANK, 1U, 0xF1CBU, 39U,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t fixed_source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayCameraExpectedSource *source)
{
    return prg_offset +
           (uint64_t)(prg_banks - 1U) *
               TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      GAMEPLAY_CAMERA_FIXED_CPU_BASE);
}

static int validate_source_relationships(const uint8_t *payload)
{
    static const uint8_t initialize_exact[26] = {
        0xA2U,0x00U,0x8EU,0x00U,0x03U,0x8EU,0x01U,0x03U,
        0x8EU,0x02U,0x03U,0x8EU,0x03U,0x03U,0x86U,0x3BU,
        0x86U,0x00U,0xE8U,0x86U,0x01U,0xA9U,0x20U,0x85U,
        0x38U,0x60U
    };
    static const uint8_t follow_prefix[12] = {
        0x50U,0xD8U,0x20U,0xA0U,0xE8U,0x04U,
        0xADU,0xDEU,0x07U,0xF0U,0x01U,0x60U
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
    const uint8_t *initialize = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET;
    const uint8_t *stream = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET;
    const uint8_t *follow = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET;
    const uint8_t *settle = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET;
    const uint8_t *projection = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET;
    return memcmp(initialize, initialize_exact,
                  sizeof(initialize_exact)) == 0 &&
           memcmp(follow, follow_prefix, sizeof(follow_prefix)) == 0 &&
           memcmp(settle + 29U, settle_loop, sizeof(settle_loop)) == 0 &&
           memcmp(projection + 12U, projection_subtract,
                  sizeof(projection_subtract)) == 0 &&
           memcmp(stream + 247U, stream_terminal_table,
                  sizeof(stream_terminal_table)) == 0 &&
           memcmp(projection + 35U, projection_terminal_store,
                  sizeof(projection_terminal_store)) == 0
        ? 0 : -1;
}

int tecmo_asset_pack_build_gameplay_camera(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCameraProvenance *provenance,
    char *message,
    size_t message_size)
{
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        prg_banks != GAMEPLAY_CAMERA_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != GAMEPLAY_CAMERA_REV1_ROM_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        uint64_t offset = fixed_source_offset(
            prg_offset, prg_banks, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE;
        if (expected->bank != prg_banks - 1U ||
            expected->fixed_bank == 0U || cpu_end > 0xFFFFU ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGCP-1 fixed $%04X-$%04X fingerprint mismatch.",
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
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

    if (validate_source_relationships(payload) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 camera opcode relationships rejected.");
        return -1;
    }
    if (tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            GAMEPLAY_CAMERA_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 full-ROM FNV1a32 mismatch for target Rev1.");
        return -1;
    }

    memcpy(payload, "TGCP", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 36U, GAMEPLAY_CAMERA_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 40U, GAMEPLAY_CAMERA_REV1_ROM_FNV1A32);
    memcpy(payload + 44U, gameplay_camera_rev1_sha256,
           sizeof(gameplay_camera_rev1_sha256));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *expected =
            &tecmo_gameplay_camera_expected_sources[index];
        uint8_t *descriptor = payload + 76U + index * 12U;
        tecmo_asset_pack_store_u32(
            descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(
            descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            descriptor + 8U, expected->fingerprint);
    }
    tecmo_asset_pack_store_u16(payload + 148U, 0x0100U);
    payload[150U] = 0x00U; /* scroll_x */
    payload[151U] = 0x00U; /* scroll_aux */
    payload[152U] = 0x00U; /* nametable page */
    payload[153U] = 0x00U; /* auxiliary state */
    payload[154U] = 0x00U; /* initial stream direction */
    payload[155U] = 0x20U; /* initial layout cursor */
    payload[156U] = 2U;    /* endpoint raw speed 3 after DEX */
    payload[157U] = 7U;    /* generic raw speed 8 after DEX */
    tecmo_asset_pack_store_u16(
        payload + 158U, GAMEPLAY_CAMERA_FORCED_SETTLE_LIMIT);
    payload[160U] = 16U;   /* world/camera X width */
    payload[161U] = 8U;    /* world Y/altitude width */
    payload[162U] = 0U;    /* visible subtraction high byte */
    payload[163U] = 1U;    /* Y saturates on borrow */
    memcpy(payload + 164U,
           payload + TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET,
           6U);

    if (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32 != 0U &&
        tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCP-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGCP-1 gameplay camera asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_camera_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(gameplay_camera_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)GAMEPLAY_CAMERA_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)GAMEPLAY_CAMERA_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE];
    uint8_t input_sha256[32];
    TecmoGameplayCameraProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != GAMEPLAY_CAMERA_REV1_ROM_SIZE ||
        memcmp(rom, gameplay_camera_rev1_ines_header,
               sizeof(gameplay_camera_rev1_ines_header)) != 0 ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 direct source test requires the exact Rev1 iNES layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_camera(
        rom, rom_size, prg_offset, GAMEPLAY_CAMERA_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    if (result == 0 &&
        (tecmo_asset_pack_sha256_digest(
             rom, (size_t)rom_size, input_sha256) != 0 ||
         memcmp(input_sha256, gameplay_camera_rev1_sha256,
                sizeof(input_sha256)) != 0)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 direct source test full-ROM SHA-256 mismatch.");
        result = -1;
    }
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_camera_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE];
    TecmoGameplayCameraProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET +
                TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE ||
        tecmo_asset_pack_build_gameplay_camera(
            truncated_rom, sizeof(truncated_rom), 16U,
            GAMEPLAY_CAMERA_PRG_BANK_COUNT, 1,
            payload, sizeof(payload), &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCP-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGCP-1 layout self-test passed.");
    return 0;
}
