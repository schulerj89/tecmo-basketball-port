#include "tecmo_asset_pack_gameplay_ball_dribble.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define BALL_DRIBBLE_PRG_BANK_COUNT 8U
#define BALL_DRIBBLE_REV1_ROM_SIZE 393232U
#define BALL_DRIBBLE_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t ball_dribble_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t ball_dribble_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayBallDribbleExpectedSource
    tecmo_gameplay_ball_dribble_expected_sources[
        TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_ROUTINE,
         5U, 0U, 0xB52EU,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET},
        {TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_TABLES,
         5U, 0U, 0xB5C0U,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayBallDribbleExpectedSource *source)
{
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int validate_semantics(const uint8_t *payload)
{
    static const uint8_t prefix[16] = {
        0xA5U,0x7EU,0x29U,0xFBU,0x85U,0x7EU,0xADU,0x88U,
        0x05U,0x29U,0xBFU,0x8DU,0x88U,0x05U,0xAEU,0x08U
    };
    static const uint8_t half_select[12] = {
        0xBCU,0x63U,0x04U,0xB9U,0xC0U,0xB5U,
        0x8DU,0xFBU,0x06U,0xADU,0xFBU,0x06U
    };
    static const uint8_t dmc_trigger[21] = {
        0xA9U,0x02U,0x8DU,0x12U,0x40U,
        0xA9U,0x20U,0x8DU,0x13U,0x40U,
        0xA9U,0x0FU,0x8DU,0x10U,0x40U,
        0xA9U,0x1FU,0x8DU,0x15U,0x40U,0x60U
    };
    static const uint8_t direction_half[8] = {
        1U,0U,0U,1U,0U,0U,0U,1U
    };
    const uint8_t *routine = payload +
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET;
    const uint8_t *tables = payload +
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET;
    if (memcmp(routine, prefix, sizeof(prefix)) != 0 ||
        memcmp(routine + (0xB54BU - 0xB52EU),
               half_select, sizeof(half_select)) != 0 ||
        memcmp(routine + (0xB5ABU - 0xB52EU),
               dmc_trigger, sizeof(dmc_trigger)) != 0 ||
        memcmp(tables, direction_half, sizeof(direction_half)) != 0) {
        return 0;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_HALF_COUNT *
                     TECMO_GAMEPLAY_BALL_DRIBBLE_DIRECTION_COUNT *
                     TECMO_GAMEPLAY_BALL_DRIBBLE_PHASE_COUNT;
         ++index) {
        if (tables[8U + index] > 0x16U) return 0;
    }
    return 1;
}

int tecmo_asset_pack_build_gameplay_ball_dribble(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayBallDribbleProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE ||
        prg_banks != BALL_DRIBBLE_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != BALL_DRIBBLE_REV1_ROM_SIZE ||
        prg_offset != sizeof(ball_dribble_rev1_ines_header) ||
        memcmp(rom, ball_dribble_rev1_ines_header,
               sizeof(ball_dribble_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, ball_dribble_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBD-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }
    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT; ++index) {
        const TecmoGameplayBallDribbleExpectedSource *source =
            &tecmo_gameplay_ball_dribble_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, source);
        uint32_t cpu_end =
            (uint32_t)source->cpu_start + source->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE;
        if (source->bank >= prg_banks || cpu_end >= 0xC000U ||
            !range_ok(offset, source->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, source->byte_count) !=
                    source->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGBD-1 Bank05 $%04X-$%04X fingerprint mismatch.",
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
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)provenance->source_offsets[0U],
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE +
                TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            BALL_DRIBBLE_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBD-1 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGBD", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 32U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 36U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 40U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 48U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 56U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 60U, BALL_DRIBBLE_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 64U, BALL_DRIBBLE_REV1_ROM_FNV1A32);
    memcpy(payload + 68U, ball_dribble_rev1_sha256,
           sizeof(ball_dribble_rev1_sha256));
    payload[100U] = TECMO_GAMEPLAY_BALL_DRIBBLE_DIRECTION_COUNT;
    payload[101U] = TECMO_GAMEPLAY_BALL_DRIBBLE_PHASE_COUNT;
    payload[102U] = TECMO_GAMEPLAY_BALL_DRIBBLE_HALF_COUNT;
    payload[103U] = 3U;
    payload[104U] = 0U;
    payload[105U] = 10U;
    payload[106U] = 6U;
    tecmo_asset_pack_store_u32(
        payload + 108U,
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_FNV1A32);
    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGBD-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGBD-1 held-ball animation asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_ball_dribble_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE];
    TecmoGameplayBallDribbleProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGBD-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_ball_dribble(
        rom, rom_size, sizeof(ball_dribble_rev1_ines_header),
        BALL_DRIBBLE_PRG_BANK_COUNT, 1, payload, sizeof(payload),
        &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_ball_dribble_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE];
    TecmoGameplayBallDribbleProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET +
                TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET <
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE ||
        tecmo_asset_pack_build_gameplay_ball_dribble(
            truncated_rom, sizeof(truncated_rom), 16U,
            BALL_DRIBBLE_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGBD-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGBD-1 layout self-test passed.");
    return 0;
}
