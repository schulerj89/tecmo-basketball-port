#include "tecmo_asset_pack_gameplay_movement.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_camera.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define MOVEMENT_PRG_BANK_COUNT 8U
#define MOVEMENT_CHR_BANK_COUNT 32U
#define MOVEMENT_FIXED_CPU_BASE 0xC000U
#define MOVEMENT_REV1_ROM_SIZE 393232U
#define MOVEMENT_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t movement_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t movement_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayMovementExpectedSource
    tecmo_gameplay_movement_expected_sources[
        TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_PROFILE_AND_SPEED,
         2U, 0U, 0xA89EU,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_ANIMATION_CONFIG,
         4U, 0U, 0xACE4U,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_DELTA_HELPERS,
         5U, 0U, 0x879BU,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_DIRECTION_HANDLERS,
         5U, 0U, 0x88F9U,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_INPUT_AND_POSE,
         5U, 0U, 0x8E58U,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_DIRECTION_MAP,
         5U, 0U, 0xBF6CU,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET},
        {TECMO_GAMEPLAY_MOVEMENT_SOURCE_ACTOR_CLAMP,
         7U, 1U, 0xF106U,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayMovementExpectedSource *source)
{
    uint16_t cpu_base = source->fixed_bank != 0U
        ? MOVEMENT_FIXED_CPU_BASE
        : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
    uint32_t bank = source->fixed_bank != 0U
        ? prg_banks - 1U
        : source->bank;
    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - cpu_base);
}

static int validate_semantics(const uint8_t *payload)
{
    static const uint8_t speed_adjustments[3] = {0x05U,0xFFU,0xFAU};
    static const uint8_t animation_setup[10] = {
        0xA9U,0x08U,0x8DU,0x85U,0x03U,
        0xA9U,0x03U,0x8DU,0x91U,0x03U
    };
    static const uint8_t input_prefix[8] = {
        0xA6U,0xBEU,0xA4U,0xBFU,0xB5U,0x05U,0x29U,0x20U
    };
    static const uint8_t direction_map[16] = {
        0U,0U,1U,0U,2U,3U,4U,0U,5U,6U,7U,0U,1U,2U,4U,5U
    };
    static const uint8_t pose_low_high[32] = {
        0x6AU,0xAAU,0x4AU,0x5AU,0x3AU,0x8AU,0xFAU,0x9AU,
        0xEAU,0x2AU,0xCAU,0xDAU,0xBAU,0x0AU,0x7AU,0x1AU,
        0x01U,0x00U,0x01U,0x01U,0x01U,0x00U,0x00U,0x00U,
        0x00U,0x01U,0x00U,0x00U,0x00U,0x01U,0x01U,0x01U
    };
    const uint8_t *profile = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_OFFSET;
    const uint8_t *config = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CONFIG_OFFSET;
    const uint8_t *delta = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DELTA_OFFSET;
    const uint8_t *handlers = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_OFFSET;
    const uint8_t *input = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_INPUT_OFFSET;
    const uint8_t *map = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET;
    const uint8_t *clamp = payload +
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET;

    return memcmp(profile + (0xA90BU - 0xA89EU), speed_adjustments,
                  sizeof(speed_adjustments)) == 0 &&
           memcmp(config + (0xAD1CU - 0xACE4U), animation_setup,
                  sizeof(animation_setup)) == 0 &&
           memcmp(delta, "\x20\x45\xC0", 3U) == 0 &&
           memcmp(delta + (0x883BU - 0x879BU),
                  "\x20\xD7\x87", 3U) == 0 &&
           delta[0x8848U - 0x879BU] == 0xECU &&
           delta[0x885EU - 0x879BU] == 0x4AU &&
           handlers[0x88FFU - 0x88F9U] == 0x0FU &&
           handlers[0x8901U - 0x88F9U] == 0x01U &&
           handlers[0x899DU - 0x88F9U] == 0x10U &&
           handlers[TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HANDLERS_SIZE - 1U]
               == 0x60U &&
           memcmp(input, input_prefix, sizeof(input_prefix)) == 0 &&
           memcmp(input + (0x8F47U - 0x8E58U), pose_low_high,
                  sizeof(pose_low_high)) == 0 &&
           memcmp(map + (0xBF94U - 0xBF6CU), direction_map,
                  sizeof(direction_map)) == 0 &&
           clamp[0U] == 0xA9U && clamp[1U] == 0x01U &&
           clamp[2U] == 0x8DU && clamp[3U] == 0x42U &&
           clamp[4U] == 0x07U &&
           memcmp(
               clamp + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE - 3U,
               "\x4C\x06\xF1", 3U) == 0;
}

int tecmo_asset_pack_build_gameplay_movement(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayMovementProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE ||
        prg_banks != MOVEMENT_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != MOVEMENT_REV1_ROM_SIZE ||
        prg_offset != sizeof(movement_rev1_ines_header) ||
        memcmp(rom, movement_rev1_ines_header,
               sizeof(movement_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, movement_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGMO-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *expected =
            &tecmo_gameplay_movement_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCE_STRIDE;
        if (expected->bank >= prg_banks ||
            (expected->fixed_bank != 0U
                 ? cpu_end > 0xFFFFU || expected->cpu_start < 0xC000U
                 : cpu_end >= 0xC000U || expected->cpu_start < 0x8000U) ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGMO-1 %s $%04X-$%04X fingerprint mismatch.",
                expected->fixed_bank != 0U ? "fixed" : "switchable",
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

    if (!validate_semantics(payload) ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            MOVEMENT_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGMO-1 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGMO", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_TEAM_DATA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_TEAM_DATA_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 44U, MOVEMENT_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 48U, MOVEMENT_REV1_ROM_FNV1A32);
    memcpy(payload + 52U, movement_rev1_sha256,
           sizeof(movement_rev1_sha256));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *expected =
            &tecmo_gameplay_movement_expected_sources[index];
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DESCRIPTOR_STRIDE;
        tecmo_asset_pack_store_u32(
            descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(
            descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            descriptor + 8U, expected->fingerprint);
    }
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SPEEDS_OFFSET,
           payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_OFFSET +
               (0xA90BU - 0xA89EU),
           TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT);
    payload[179U] = 6U;
    payload[180U] = 8U;
    payload[181U] = 8U;
    payload[182U] = 3U;
    payload[183U] = 5U;
    payload[184U] = 0x4AU;
    payload[185U] = 0xECU;
    payload[186U] = 3U;
    payload[187U] = 4U;
    tecmo_asset_pack_store_u16(payload + 188U, 0x00DFU);
    tecmo_asset_pack_store_u16(payload + 190U, 0x0220U);
    tecmo_asset_pack_store_u16(payload + 192U, 0x00EFU);
    tecmo_asset_pack_store_u16(payload + 194U, 0x0028U);
    payload[196U] = 6U;
    memcpy(payload +
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_DIRECTION_MAP_OFFSET,
           payload + TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_MAP_OFFSET +
               (0xBF94U - 0xBF6CU),
           TECMO_GAMEPLAY_MOVEMENT_DIRECTION_TABLE_COUNT);

    if (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 != 0U &&
        tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGMO-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGMO-1 movement asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_movement_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(movement_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)MOVEMENT_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)MOVEMENT_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE];
    TecmoGameplayMovementProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGMO-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != MOVEMENT_REV1_ROM_SIZE ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGMO-1 direct source test requires the exact Rev1 layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_movement(
        rom, rom_size, prg_offset, MOVEMENT_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_movement_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE];
    TecmoGameplayMovementProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_PROFILE_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCES_OFFSET +
                TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_CLAMP_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE ||
        tecmo_asset_pack_build_gameplay_movement(
            truncated_rom, sizeof(truncated_rom), 16U,
            MOVEMENT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGMO-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGMO-1 layout self-test passed.");
    return 0;
}
