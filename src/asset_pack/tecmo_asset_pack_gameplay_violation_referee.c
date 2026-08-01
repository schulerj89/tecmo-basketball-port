#include "tecmo_asset_pack_gameplay_violation_referee.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_gameplay_penalties.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define VIOLATION_REFEREE_PRG_BANK_COUNT 8U
#define VIOLATION_REFEREE_REV1_ROM_SIZE 393232U
#define VIOLATION_REFEREE_SCREEN_STREAM_CPU 0xB600U
#define VIOLATION_REFEREE_METASPRITE_CPU 0xB33FU
#define VIOLATION_REFEREE_SEQUENCE_TABLE_CPU 0xB317U

_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET +
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET,
    "TGVR-1 source table must end at its raw-source region");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET +
            TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE <=
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET,
    "TGVR-1 message table must not overlap its sequence table");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET +
            TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE,
    "TGVR-1 group table must end at its payload boundary");

static const uint8_t violation_referee_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t violation_referee_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayViolationRefereeExpectedSource
    tecmo_gameplay_violation_referee_expected_sources[
        TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_ROUTE,
         3U,0U,0xBE87U,26U,576U,0x95E7254BU},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_TEXT_ROUTINE,
         3U,0U,0xBEC7U,226U,602U,0x522DC46BU},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_CHARACTER_MAP,
         3U,0U,0x9AC8U,64U,828U,0x4664C4CCU},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SCREEN_DESCRIPTOR,
         7U,1U,0xDCA8U,7U,892U,
         TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DESCRIPTOR_FNV1A32},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SCREEN_STREAM,
         0U,0U,0xB600U,288U,899U,
         TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_STREAM_FNV1A32},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_BACKGROUND_PALETTE,
         0U,0U,0xB5F0U,16U,1187U,
         TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_BG_PALETTE_FNV1A32},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_CONTROLLER,
         4U,0U,0xBA1FU,211U,1203U,0xD61CA1D3U},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SPRITE_PALETTE,
         4U,0U,0xBA06U,16U,1414U,
         TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SPRITE_PALETTE_FNV1A32},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SEQUENCE_TABLES,
         4U,0U,0xB317U,40U,1430U,0x77B2BEBCU},
        {TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_METASPRITES,
         4U,0U,0xB33FU,1735U,1470U,0xCC00227BU}
    };

static const char *const violation_messages[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    "OUT OF BOUNDS", "BACKCOURT", "5 SECOND VIOLATION",
    "10 SECOND VIOLATION", "SHOT CLOCK VIOLATION", "TRAVELING",
    "GOALTENDING"
};

static const uint8_t violation_message_ppu_low[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    0xE9U,0xEBU,0xE7U,0xE7U,0xE6U,0xEBU,0xEAU
};

static const uint8_t violation_sequence_id[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    1U,1U,3U,3U,3U,4U,1U
};

static const uint8_t sequence_lengths[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT] = {
    4U,5U,5U,4U,7U
};

static const uint8_t sequence_groups[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT]
    [TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS] = {
    {1U,2U,2U,2U,0xFFU,0xFFU,0xFFU,0xFFU},
    {3U,4U,5U,5U,5U,0xFFU,0xFFU,0xFFU},
    {6U,7U,8U,8U,8U,0xFFU,0xFFU,0xFFU},
    {9U,10U,10U,10U,0xFFU,0xFFU,0xFFU,0xFFU},
    {11U,12U,13U,14U,11U,11U,11U,0xFFU}
};

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
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
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayViolationRefereeExpectedSource *source)
{
    uint16_t base = source->fixed_bank != 0U ? 0xC000U : 0x8000U;
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - base);
}

static int build_messages(uint8_t *payload, char *message,
                          size_t message_size)
{
    const uint8_t *character_map = payload +
        tecmo_gameplay_violation_referee_expected_sources[2U].payload_offset;
    size_t index;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT; ++index) {
        const char *text = violation_messages[index];
        size_t length = strlen(text);
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE;
        size_t character;
        if (length == 0U ||
            length > TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_MESSAGE_TILES) {
            tecmo_asset_pack_set_message(
                message, message_size, "TGVR-1 violation text length rejected.");
            return -1;
        }
        record[0U] = (uint8_t)(index + 1U);
        record[1U] = (uint8_t)(index + 1U);
        record[2U] = violation_sequence_id[index];
        record[3U] = (uint8_t)length;
        record[4U] = 0x22U;
        record[5U] = violation_message_ppu_low[index];
        for (character = 0U; character < length; ++character) {
            unsigned int code = (unsigned char)text[character];
            if (code < 0x20U || code >= 0x60U) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TGVR-1 violation text escaped the ROM character map.");
                return -1;
            }
            record[6U + character] = character_map[code - 0x20U];
        }
        memset(record + 6U + length, 0xFF,
               TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_MESSAGE_TILES - length);
    }
    return 0;
}

static int build_sequences(uint8_t *payload, char *message,
                           size_t message_size)
{
    const TecmoGameplayViolationRefereeExpectedSource *source =
        &tecmo_gameplay_violation_referee_expected_sources[8U];
    const uint8_t *raw = payload + source->payload_offset;
    size_t index;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT; ++index) {
        uint16_t pointer = (uint16_t)((uint16_t)raw[index] |
                            ((uint16_t)raw[5U + index] << 8U));
        size_t raw_offset;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE;
        if (pointer < VIOLATION_REFEREE_SEQUENCE_TABLE_CPU ||
            pointer >= VIOLATION_REFEREE_SEQUENCE_TABLE_CPU +
                           source->byte_count) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGVR-1 referee sequence pointer escaped its source span.");
            return -1;
        }
        raw_offset = (size_t)(pointer - VIOLATION_REFEREE_SEQUENCE_TABLE_CPU);
        if (raw_offset + sequence_lengths[index] >= source->byte_count ||
            memcmp(raw + raw_offset, sequence_groups[index],
                   sequence_lengths[index]) != 0 ||
            raw[raw_offset + sequence_lengths[index]] != 0x80U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGVR-1 referee sequence contents rejected.");
            return -1;
        }
        record[0U] = (uint8_t)index;
        record[1U] = sequence_lengths[index];
        memcpy(record + 2U, sequence_groups[index],
               TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS);
    }
    return 0;
}

static int build_groups(uint8_t *payload, uint64_t chr_size,
                        char *message, size_t message_size)
{
    const TecmoGameplayViolationRefereeExpectedSource *source =
        &tecmo_gameplay_violation_referee_expected_sources[9U];
    const uint8_t *raw = payload + source->payload_offset;
    static const uint8_t sprite_selectors[4] = {
        0xCAU,0x71U,0x72U,0x73U
    };
    size_t index;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT; ++index) {
        uint16_t pointer = (uint16_t)((uint16_t)raw[index] |
                            ((uint16_t)raw[15U + index] << 8U));
        size_t record_offset;
        size_t piece_size;
        uint8_t count;
        uint8_t *record;
        size_t piece;
        if (pointer < VIOLATION_REFEREE_METASPRITE_CPU ||
            pointer >= VIOLATION_REFEREE_METASPRITE_CPU +
                           source->byte_count) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGVR-1 referee metasprite pointer escaped its source span.");
            return -1;
        }
        record_offset = (size_t)(pointer - VIOLATION_REFEREE_METASPRITE_CPU);
        if (record_offset > source->byte_count - 3U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGVR-1 referee metasprite header was truncated.");
            return -1;
        }
        count = raw[record_offset];
        piece_size = (size_t)count * 4U;
        if (count == 0U ||
            count > TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_PIECES ||
            piece_size > source->byte_count - record_offset - 3U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGVR-1 referee metasprite piece range rejected.");
            return -1;
        }
        for (piece = 0U; piece < count; ++piece) {
            const uint8_t *piece_record = raw + record_offset + 3U +
                                          piece * 4U;
            uint8_t tile = (uint8_t)(piece_record[1U] + 0x3CU);
            uint8_t selector = sprite_selectors[(tile >> 6U) & 3U];
            uint64_t chr_offset = (uint64_t)selector * 1024ULL +
                                  (uint64_t)(tile & 0x3EU) * 16ULL;
            if ((piece_record[2U] & 0x3CU) != 0U ||
                chr_offset + 32U > chr_size) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TGVR-1 referee metasprite CHR/attribute rejected.");
                return -1;
            }
        }
        record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE;
        record[0U] = (uint8_t)index;
        record[1U] = count;
        record[2U] = raw[record_offset + 1U];
        record[3U] = raw[record_offset + 2U];
        tecmo_asset_pack_store_u16(record + 4U, pointer);
        tecmo_asset_pack_store_u32(
            record + 6U,
            source->payload_offset + (uint32_t)record_offset + 3U);
        tecmo_asset_pack_store_u16(record + 10U, (uint16_t)piece_size);
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_violation_referee(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayViolationRefereeProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    uint8_t decoded[
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_SIZE];
    size_t encoded_size = 0U;
    size_t index;
    const uint8_t *descriptor;
    const uint8_t *controller;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE ||
        enforce_revision_fingerprints == 0 ||
        rom_size != VIOLATION_REFEREE_REV1_ROM_SIZE ||
        prg_offset != sizeof(violation_referee_rev1_ines_header) ||
        prg_banks != VIOLATION_REFEREE_PRG_BANK_COUNT ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE ||
        !range_ok(chr_offset, chr_size, rom_size) ||
        memcmp(rom, violation_referee_rev1_ines_header,
               sizeof(violation_referee_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, violation_referee_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGVR-1 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }
    if (tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32 ||
        fnv1a64(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A64) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGVR-1 CHR revision fingerprint rejected.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT; ++index) {
        const TecmoGameplayViolationRefereeExpectedSource *source =
            &tecmo_gameplay_violation_referee_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, source);
        uint32_t cpu_end = (uint32_t)source->cpu_start +
                           source->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE;
        if (source->bank >= prg_banks ||
            source->cpu_start <
                (source->fixed_bank != 0U ? 0xC000U : 0x8000U) ||
            cpu_end >= (source->fixed_bank != 0U ? 0x10000U : 0xC000U) ||
            !range_ok(offset, source->byte_count, rom_size) ||
            source->payload_offset <
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET ||
            source->payload_offset > payload_size ||
            source->byte_count > payload_size - source->payload_offset ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, source->byte_count) !=
                    source->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGVR-1 source $%04X-$%04X fingerprint/range rejected.",
                (unsigned)source->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)source->kind);
        record[2U] = source->bank;
        record[3U] = source->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, source->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, source->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, source->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, source->fingerprint);
        tecmo_asset_pack_store_u16(record + 20U, (uint16_t)index);
        memcpy(payload + source->payload_offset,
               rom + (size_t)offset, source->byte_count);
        provenance->source_offsets[index] = offset;
    }
    provenance->chr_offset = chr_offset;

    descriptor = payload +
        tecmo_gameplay_violation_referee_expected_sources[3U].payload_offset;
    controller = payload +
        tecmo_gameplay_violation_referee_expected_sources[6U].payload_offset;
    if ((uint8_t)(descriptor[0U] * 2U) != 0xCAU ||
        (uint8_t)(descriptor[1U] * 2U) != 0xFAU ||
        tecmo_asset_pack_read_u16(descriptor + 2U) != 0xB5F0U ||
        tecmo_asset_pack_read_u16(descriptor + 4U) != 0xB600U ||
        descriptor[6U] != 0U ||
        controller[0xBA88U - 0xBA1FU] != 0U ||
        memcmp(controller + (0xBA89U - 0xBA1FU),
               violation_sequence_id,
               sizeof(violation_sequence_id)) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)prg_offset, TECMO_ASSET_PACK_PRG_BANK_BYTES,
            VIOLATION_REFEREE_SCREEN_STREAM_CPU - 0x8000U,
            decoded, sizeof(decoded), &encoded_size,
            message, message_size) != 0 ||
        encoded_size != 288U ||
        tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGVR-1 screen, selector, or decoded nametable contract rejected.");
        return -1;
    }
    memcpy(payload +
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET,
           decoded, sizeof(decoded));
    if (build_messages(payload, message, message_size) != 0 ||
        build_sequences(payload, message, message_size) != 0 ||
        build_groups(payload, chr_size, message, message_size) != 0) {
        return -1;
    }

    memcpy(payload, "TGVR", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 36U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 40U, TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 42U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 44U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 48U, TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 50U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 52U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 56U, TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 58U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 60U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 64U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32);
    store_u64(
        payload + 68U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A64);
    tecmo_asset_pack_store_u32(
        payload + 76U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DESCRIPTOR_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 80U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_STREAM_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 84U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 88U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_BG_PALETTE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 92U,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SPRITE_PALETTE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 96U,
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE));
    tecmo_asset_pack_store_u32(
        payload + 100U,
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE));
    tecmo_asset_pack_store_u32(
        payload + 104U,
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE));
    tecmo_asset_pack_store_u32(
        payload + 108U,
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE));
    tecmo_asset_pack_store_u32(
        payload + 112U,
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE));
    tecmo_asset_pack_store_u32(
        payload + 116U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 120U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32);
    tecmo_asset_pack_store_u16(payload + 124U, 0xDCA8U);
    tecmo_asset_pack_store_u16(payload + 126U, 0xB600U);
    tecmo_asset_pack_store_u16(payload + 128U, 0xB5F0U);
    tecmo_asset_pack_store_u16(payload + 130U, 0xBA1FU);
    payload[132U] = 5U;
    payload[133U] = 0U;
    payload[134U] = 0xCAU;
    payload[135U] = 0xFAU;
    payload[136U] = 0xCAU;
    payload[137U] = 0x71U;
    payload[138U] = 0x72U;
    payload[139U] = 0x73U;
    payload[140U] = 0U;
    payload[141U] = 16U;
    payload[142U] = 4U;
    payload[143U] = TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS;
    payload[144U] = 0x22U;
    payload[145U] = 16U;
    payload[146U] = 120U;
    memcpy(payload + 148U, violation_referee_rev1_sha256,
           sizeof(violation_referee_rev1_sha256));

    if (tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGVR-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGVR-1 violation referee cutaway asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_violation_referee_source_test(
    const char *rom_path, char *message, size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE];
    TecmoGameplayViolationRefereeProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGVR-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_violation_referee(
        rom, rom_size, sizeof(violation_referee_rev1_ines_header),
        VIOLATION_REFEREE_PRG_BANK_COUNT,
        sizeof(violation_referee_rev1_ines_header) +
            VIOLATION_REFEREE_PRG_BANK_COUNT *
                TECMO_ASSET_PACK_PRG_BANK_BYTES,
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE,
        1, payload, sizeof(payload), &provenance,
        message, message_size);
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_violation_referee_self_test(
    char *message, size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE];
    TecmoGameplayViolationRefereeProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET <
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_SIZE ||
        tecmo_asset_pack_build_gameplay_violation_referee(
            truncated_rom, sizeof(truncated_rom), 16U,
            VIOLATION_REFEREE_PRG_BANK_COUNT, 16U, 0U, 1,
            payload, sizeof(payload), &provenance,
            NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGVR-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGVR-1 layout self-test passed.");
    return 0;
}
