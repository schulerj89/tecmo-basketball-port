#include "tecmo_asset_pack_gameplay_cpu_steering.h"

#include "tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define CPU_STEERING_PRG_BANK_COUNT 8U
#define CPU_STEERING_CHR_BANK_COUNT 32U
#define CPU_STEERING_FIXED_CPU_BASE 0xC000U
#define CPU_STEERING_REV1_ROM_SIZE 393232U
#define CPU_STEERING_REV1_ROM_FNV1A32 0x0650F5B0U
#define CPU_STEERING_COMMAND_BASE_CPU 0x9F2EU

static const uint8_t cpu_steering_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t cpu_steering_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static const uint16_t cpu_steering_handler_cpu[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0x90E0U,0x934BU,0x9280U,0x905EU,0x8FFAU,0x8F92U,
    0x8F2DU,0x8F12U,0x8ED7U,0x8FC5U,0x8CD0U,0x8C40U,
    0x8E4FU,0x9125U,0x9146U,0x9172U,0x9085U,0x8C1AU,
    0x8C1AU,0x8C1AU,0x9032U,0x8BF6U,0x8BE1U,0x8F72U
};

static const uint8_t cpu_steering_direction_map[
    TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT] = {
    3U,6U,4U,7U,0U,1U,2U,5U
};

const TecmoGameplayCpuSteeringExpectedSource
    tecmo_gameplay_cpu_steering_expected_sources[
        TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ACTOR_DISPATCH,
         6U,0U,0x81F7U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_REFERENCE_DIRECTION,
         6U,0U,0x87AEU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_DIRECTION,
         6U,0U,0x88DAU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_DISPATCH,
         6U,0U,0x8B90U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_HANDLERS,
         6U,0U,0x8BE1U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_APPLY,
         6U,0U,0x9280U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_FORMATION_STREAM_SELECT,
         6U,0U,0x938BU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_TRAMPOLINE,
         7U,1U,0xC006U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_READER,
         7U,1U,0xCBE0U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_PLAY_COMMANDS,
         4U,0U,0x9F2EU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayCpuSteeringExpectedSource *source)
{
    uint16_t cpu_base = source->fixed_bank != 0U
        ? CPU_STEERING_FIXED_CPU_BASE
        : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
    uint32_t bank = source->fixed_bank != 0U
        ? prg_banks - 1U
        : source->bank;
    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - cpu_base);
}

static int play_commands_valid(const uint8_t *commands)
{
    static const uint8_t first_command[5] = {0x04U,0x0AU,0U,0U,0U};
    static const uint8_t free_throw_a[5] = {0x03U,0x08U,0U,0U,0U};
    static const uint8_t free_throw_b[5] = {0x02U,0xB4U,0U,0x96U,0U};
    static const uint8_t last_command[5] = {0x01U,0x80U,0x0CU,0U,0U};
    if (commands == NULL ||
        memcmp(commands, first_command, sizeof(first_command)) != 0 ||
        memcmp(commands + 0x007DU, free_throw_a,
               sizeof(free_throw_a)) != 0 ||
        memcmp(commands + 0x00D7U, free_throw_b,
               sizeof(free_throw_b)) != 0 ||
        memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE -
                   TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE,
               last_command, sizeof(last_command)) != 0) {
        return 0;
    }
    for (size_t offset = 0U;
         offset < TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE;
         offset += TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) {
        if (commands[offset] >=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
            return 0;
        }
    }
    return 1;
}

static int validate_semantics(const uint8_t *payload)
{
    const uint8_t *actor = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET;
    const uint8_t *reference = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_OFFSET;
    const uint8_t *target_direction = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_OFFSET;
    const uint8_t *dispatch = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_OFFSET;
    const uint8_t *handlers = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET;
    const uint8_t *apply = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_OFFSET;
    const uint8_t *formation = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_OFFSET;
    const uint8_t *trampoline = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_OFFSET;
    const uint8_t *reader = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_OFFSET;
    const uint8_t *commands = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET;
    uint8_t handler_low[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint8_t handler_high[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];

    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        handler_low[index] =
            (uint8_t)(cpu_steering_handler_cpu[index] & 0xFFU);
        handler_high[index] =
            (uint8_t)(cpu_steering_handler_cpu[index] >> 8U);
    }

    return memcmp(actor + (0x8284U - 0x81F7U),
                  "\xA2\x09\xEC\x08\x03\xF0", 6U) == 0 &&
           memcmp(actor + (0x82B6U - 0x81F7U),
                  "\x9F\x9F\x9F\x9F\x90", 5U) == 0 &&
           reference[0U] == 0xA6U && reference[1U] == 0xAAU &&
           memcmp(reference + (0x887BU - 0x87AEU),
                  "\xB9\x8E\x8A\x9D\x63\x04", 6U) == 0 &&
           memcmp(target_direction, "\xA5\xA4\x85\xAB", 4U) == 0 &&
           memcmp(target_direction + (0x899AU - 0x88DAU),
                  "\xB9\x8E\x8A\x9D\x63\x04", 6U) == 0 &&
           memcmp(target_direction + (0x8A8EU - 0x88DAU),
                  cpu_steering_direction_map,
                  sizeof(cpu_steering_direction_map)) == 0 &&
           memcmp(dispatch,
                  "\xBD\x47\x05\x18\x69\x2E\x85\xA8\xBD\x51\x05\x69\x9F",
                  13U) == 0 &&
           memcmp(dispatch + (0x8B9FU - 0x8B90U),
                  "\x20\x06\xC0\xA4\xC7", 5U) == 0 &&
           memcmp(dispatch + (0x8BB1U - 0x8B90U),
                  handler_low,
                  TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) == 0 &&
           memcmp(dispatch + (0x8BC9U - 0x8B90U),
                  handler_high,
                  TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) == 0 &&
           handlers[0U] == 0xA5U && handlers[1U] == 0xC8U &&
           memcmp(handlers + (0x8CD0U - 0x8BE1U),
                  "\xEC\xDF\x07", 3U) == 0 &&
           memcmp(handlers + (0x90E0U - 0x8BE1U),
                  "\xA5\xC8\x85\xA4", 4U) == 0 &&
           memcmp(apply,
                  "\xAD\x5A\x03\xF0\x23\xA9\x00\x38\xE5\xC8", 10U) == 0 &&
           memcmp(apply + (0x92A8U - 0x9280U),
                  "\xA5\xC8\x9D\x5B\x05\x38\xF5\x73", 8U) == 0 &&
           memcmp(apply + (0x92D4U - 0x9280U),
                  "\xA5\xA4\x05\xA5\x05\xA6\x05\xA7\xF0\x23",
                  10U) == 0 &&
           memcmp(apply + (0x92FEU - 0x9280U),
                  "\x4C\xDA\x88", 3U) == 0 &&
           memcmp(formation,
                  "\xAE\x08\x03\xB5\x73\x85\xA4", 7U) == 0 &&
           memcmp(formation + (0x9424U - 0x938BU),
                  "\xAD\x90\x07", 3U) == 0 &&
           memcmp(formation + (0x946FU - 0x938BU),
                  "\x86\x94\x20\x8B\x93", 5U) == 0 &&
           memcmp(trampoline, "\x4C\xE0\xCB", 3U) == 0 &&
           memcmp(reader,
                  "\xAD\xFF\xBF\x48\xA9\x04\x20\x6A\xD3\xA0\x04",
                  11U) == 0 &&
           memcmp(reader + 11U,
                  "\xB1\xA8\x99\xC7\x00\x88\x10\xF8\x68\x4C\x6A\xD3",
                  12U) == 0 &&
           play_commands_valid(commands);
}

int tecmo_asset_pack_build_gameplay_cpu_steering(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCpuSteeringProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        prg_banks != CPU_STEERING_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != CPU_STEERING_REV1_ROM_SIZE ||
        prg_offset != sizeof(cpu_steering_rev1_ines_header) ||
        memcmp(rom, cpu_steering_rev1_ines_header,
               sizeof(cpu_steering_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, cpu_steering_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE;
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
                "TGAI-1 %s Bank%02u $%04X-$%04X fingerprint mismatch.",
                expected->fixed_bank != 0U ? "fixed" : "switchable",
                (unsigned)expected->bank,
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
        /* The byte immediately after the 680th record resumes Bank04 code. */
        rom[16U + 4U * 0x4000U + (0xAC76U - 0x8000U)] != 0x20U ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            CPU_STEERING_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-1 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGAI", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 28U, CPU_STEERING_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 32U, CPU_STEERING_REV1_ROM_FNV1A32);
    memcpy(payload + 36U, cpu_steering_rev1_sha256,
           sizeof(cpu_steering_rev1_sha256));
    tecmo_asset_pack_store_u16(payload + 68U,
                               CPU_STEERING_COMMAND_BASE_CPU);
    tecmo_asset_pack_store_u16(
        payload + 70U, TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 72U, TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 74U, TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 76U, TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT);
    tecmo_asset_pack_store_u16(payload + 78U, 10U);
    payload[80U] = 4U;
    payload[81U] = 6U;
    payload[82U] = 7U;
    payload[83U] = 4U;
    tecmo_asset_pack_store_u16(payload + 84U, 0x00C7U);
    memcpy(payload +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECTION_OFFSET,
           cpu_steering_direction_map,
           sizeof(cpu_steering_direction_map));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_STRIDE;
        tecmo_asset_pack_store_u32(descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(descriptor + 8U,
                                   expected->fingerprint);
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        tecmo_asset_pack_store_u16(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HANDLER_OFFSET +
                index * 2U,
            cpu_steering_handler_cpu[index]);
    }
    /* Target-producing command bits 0,2,4,10,12,13,16,20. Opcode 5 writes
       direction directly; the other entries remain control/state commands. */
    tecmo_asset_pack_store_u32(payload + 264U, 0x00113415U);
    payload[268U] = 5U;

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGAI-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGAI-1 CPU steering evidence asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_cpu_steering_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(cpu_steering_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)CPU_STEERING_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)CPU_STEERING_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE];
    TecmoGameplayCpuSteeringProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-1 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != CPU_STEERING_REV1_ROM_SIZE ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-1 direct source test requires the exact Rev1 layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_cpu_steering(
        rom, rom_size, prg_offset, CPU_STEERING_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_cpu_steering_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE];
    TecmoGameplayCpuSteeringProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
                TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE !=
            TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT *
                TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE ||
        tecmo_asset_pack_build_gameplay_cpu_steering(
            truncated_rom, sizeof(truncated_rom), 16U,
            CPU_STEERING_PRG_BANK_COUNT, 1,
            payload, sizeof(payload), &provenance,
            NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-1 importer layout/rejection self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGAI-1 importer self-test passed.");
    return 0;
}
