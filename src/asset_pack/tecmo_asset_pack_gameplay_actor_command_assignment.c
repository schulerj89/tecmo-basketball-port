#include "tecmo_asset_pack_gameplay_actor_command_assignment.h"

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define A023_PRG_BANK_COUNT 8U
#define A023_REV1_ROM_SIZE 393232U
#define A023_REV1_ROM_FNV1A32 0x0650F5B0U
#define A023_FIXED_PRG_CPU_BASE 0xC000U

static const uint8_t a023_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

const uint8_t tecmo_gameplay_actor_command_assignment_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayActorCommandAssignmentExpectedSource
    tecmo_gameplay_actor_command_assignment_expected_sources[
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_DISTANCE_HELPER,
         5U, 0U, 0x9DF6U, 110U, 416U,
         0xBE56D3D7U, 0xBF24D84A61C2F497ULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_CALLER_AND_ASSIGNMENT,
         5U, 0U, 0x9F2FU, 430U, 526U,
         0x13F3A41BU, 0x0C3DED79B8BCFADBULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_DISPATCH,
         5U, 0U, 0xA214U, 75U, 956U,
         0x4FC82BF8U, 0x84059724738A3C78ULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE10,
         5U, 0U, 0xB6E5U, 144U, 1031U,
         0x6AD67C6AU, 0xC4DDB2D7A58EF6AAULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE17_18,
         5U, 0U, 0xB775U, 76U, 1175U,
         0xD90B723BU, 0x9EBF758818C65AFBULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_DISPATCH,
         7U, 1U, 0xC711U, 43U, 1251U,
         0xAF434105U, 0x453F5F11B98924E5ULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_SELECTOR,
         7U, 1U, 0xCAF5U, 62U, 1294U,
         0x798F7231U, 0xB65EBECE5F5ECFB1ULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_LOW,
         7U, 1U, 0xCB33U, 62U, 1356U,
         0xCB4B3C42U, 0x25A0A0DFEDDD4702ULL},
        {TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_HIGH,
         7U, 1U, 0xCB71U, 62U, 1418U,
         0xCD228EDDU, 0xF8DCBA38D20596DDULL}
    };

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0U] |
                      ((uint16_t)bytes[1U] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0U] | ((uint32_t)bytes[1U] << 8U) |
           ((uint32_t)bytes[2U] << 16U) |
           ((uint32_t)bytes[3U] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    unsigned index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void store_u64(uint8_t *bytes, uint64_t value)
{
    unsigned index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t source_offset(uint64_t prg_offset,
                              const TecmoGameplayActorCommandAssignmentExpectedSource
                                  *source)
{
    uint16_t base = source->fixed_bank != 0U
        ? A023_FIXED_PRG_CPU_BASE
        : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
    return prg_offset +
        (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
        (uint64_t)(source->cpu_start - base);
}

static bool source_semantics_valid(
    const TecmoGameplayActorCommandAssignmentExpectedSource *source,
    const uint8_t *bytes)
{
    static const uint8_t distance_prefix[] = {
        0xADU,0x78U,0x04U,0xC9U,0x10U,0xD0U,0x07U,0xAEU
    };
    static const uint8_t caller_prefix[] = {
        0xADU,0xA1U,0x05U,0xF0U,0x01U,0x60U,0xADU,0x78U
    };
    static const uint8_t object_dispatch_prefix[] = {
        0xA2U,0x0AU,0xBCU,0x6EU,0x04U,0xB9U,0x27U,0xA2U
    };
    static const uint8_t state10_prefix[] = {
        0xA5U,0xBAU,0x29U,0x03U,0xF0U,0x0BU
    };
    static const uint8_t state17_prefix[] = {
        0xADU,0x99U,0x04U,0xC9U,0x04U,0xB0U,0x31U
    };
    if (source == NULL || bytes == NULL) return false;
    switch (source->kind) {
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_DISTANCE_HELPER:
        return memcmp(bytes, distance_prefix, sizeof(distance_prefix)) == 0;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_CALLER_AND_ASSIGNMENT:
        return memcmp(bytes, caller_prefix, sizeof(caller_prefix)) == 0;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_DISPATCH:
        return memcmp(bytes, object_dispatch_prefix,
                      sizeof(object_dispatch_prefix)) == 0;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE10:
        return memcmp(bytes, state10_prefix, sizeof(state10_prefix)) == 0;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_OBJECT_STATE17_18:
        return memcmp(bytes, state17_prefix, sizeof(state17_prefix)) == 0;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_DISPATCH:
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_SELECTOR:
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_LOW:
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_ACTION_TABLE_HIGH:
        return true;
    default:
        return false;
    }
}

uint32_t tecmo_asset_pack_gameplay_actor_command_assignment_verify_span(
    size_t index,
    const uint8_t *record,
    size_t record_size,
    const uint8_t *span_bytes,
    size_t span_size)
{
    const TecmoGameplayActorCommandAssignmentExpectedSource *expected;
    uint32_t failures = 0U;
    uint32_t cpu_end;
    if (index >= TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT ||
        record == NULL || span_bytes == NULL ||
        record_size !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE) {
        return TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_BAD_INPUT;
    }
    expected =
        &tecmo_gameplay_actor_command_assignment_expected_sources[index];
    cpu_end = (uint32_t)expected->cpu_start + expected->byte_count - 1U;
    if (span_size != expected->byte_count ||
        read_u16(record) != (uint16_t)expected->kind ||
        record[2U] != expected->bank ||
        record[3U] != expected->fixed_bank ||
        read_u16(record + 4U) != expected->cpu_start ||
        read_u16(record + 6U) != (uint16_t)cpu_end ||
        read_u32(record + 8U) != expected->byte_count ||
        read_u32(record + 12U) != expected->payload_offset ||
        read_u32(record + 16U) != expected->fingerprint_fnv1a32 ||
        read_u64(record + 20U) != expected->fingerprint_fnv1a64 ||
        read_u16(record + 28U) != (uint16_t)index ||
        record[30U] != 0U || record[31U] != 0U) {
        failures |=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_DESCRIPTOR;
    }
    if (span_size != expected->byte_count ||
        tecmo_asset_pack_fnv1a32(span_bytes, span_size) !=
            expected->fingerprint_fnv1a32) {
        failures |=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_FNV1A32;
    }
    if (span_size != expected->byte_count ||
        fnv1a64(span_bytes, span_size) != expected->fingerprint_fnv1a64) {
        failures |=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_FNV1A64;
    }
    if (span_size != expected->byte_count ||
        !source_semantics_valid(expected, span_bytes)) {
        failures |=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_SEMANTICS;
    }
    return failures;
}

int tecmo_asset_pack_build_gameplay_actor_command_assignment(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayActorCommandAssignmentProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    size_t index;
    uint32_t payload_fingerprint;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE ||
        prg_banks != A023_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != A023_REV1_ROM_SIZE ||
        prg_offset != sizeof(a023_rev1_ines_header) ||
        memcmp(rom, a023_rev1_ines_header, sizeof(a023_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256,
               tecmo_gameplay_actor_command_assignment_rev1_sha256,
               sizeof(input_sha256)) != 0 ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            A023_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCA-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    memcpy(payload, "TGCA", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 32U, A023_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(payload + 36U, A023_REV1_ROM_FNV1A32);
    memcpy(payload + 40U,
           tecmo_gameplay_actor_command_assignment_rev1_sha256,
           sizeof(tecmo_gameplay_actor_command_assignment_rev1_sha256));

    for (index = 0U;
         index < TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayActorCommandAssignmentExpectedSource *source =
            &tecmo_gameplay_actor_command_assignment_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, source);
        uint32_t cpu_end = (uint32_t)source->cpu_start +
            source->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE;
        uint32_t cpu_limit = source->fixed_bank != 0U
            ? 0x10000U : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT;
        if (source->bank >= prg_banks ||
            source->cpu_start < (source->fixed_bank != 0U
                                     ? A023_FIXED_PRG_CPU_BASE
                                     : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) ||
            cpu_end >= cpu_limit ||
            !range_ok(offset, source->byte_count, rom_size)) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGCA-1 source span %u ($%04X) rejected.",
                (unsigned)index, (unsigned)source->cpu_start);
            return -1;
        }
        provenance->source_offsets[index] = offset;
        tecmo_asset_pack_store_u16(record, (uint16_t)source->kind);
        record[2U] = source->bank;
        record[3U] = source->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, source->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, source->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, source->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, source->fingerprint_fnv1a32);
        store_u64(record + 20U, source->fingerprint_fnv1a64);
        tecmo_asset_pack_store_u16(record + 28U, (uint16_t)index);
        if (tecmo_asset_pack_gameplay_actor_command_assignment_verify_span(
                index, record,
                TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE,
                rom + (size_t)offset, source->byte_count) != 0U) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGCA-1 source span %u ($%04X) fingerprint/descriptor rejected.",
                (unsigned)index, (unsigned)source->cpu_start);
            return -1;
        }
        memcpy(payload + source->payload_offset,
               rom + (size_t)offset, source->byte_count);
    }
    if (tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE) !=
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
            "TGCA-1 combined source payload fingerprint rejected.");
        return -1;
    }
    payload_fingerprint = tecmo_asset_pack_fnv1a32(payload, payload_size);
    if (payload_fingerprint !=
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCA-1 canonical payload fingerprint mismatch (got %08X).",
            payload_fingerprint);
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGCA-1 actor-command-assignment asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_actor_command_assignment_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE];
    TecmoGameplayActorCommandAssignmentProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCA-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_actor_command_assignment(
        rom, rom_size, sizeof(a023_rev1_ines_header), A023_PRG_BANK_COUNT,
        1, payload, sizeof(payload), &provenance, message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_actor_command_assignment_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE];
    TecmoGameplayActorCommandAssignmentProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET +
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE ||
        tecmo_asset_pack_build_gameplay_actor_command_assignment(
            truncated_rom, sizeof(truncated_rom), 16U, A023_PRG_BANK_COUNT,
            1, payload, sizeof(payload), &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGCA-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGCA-1 layout self-test passed.");
    return 0;
}
