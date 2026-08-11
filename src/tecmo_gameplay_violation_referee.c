#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_violation_referee.h"

#include "asset_pack/tecmo_asset_pack_gameplay_penalties.h"
#include "asset_pack/tecmo_asset_pack_gameplay_violation_referee.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG 0x52564754U

static const uint8_t violation_referee_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static const uint8_t expected_message_length[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    13U,9U,18U,19U,20U,9U,11U
};

static const uint8_t expected_message_ppu_low[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    0xE9U,0xEBU,0xE7U,0xE7U,0xE6U,0xEBU,0xEAU
};

static const uint8_t expected_sequence_id[
    TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
    1U,1U,3U,3U,3U,4U,1U
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
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
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

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool bytes_are_value(const uint8_t *bytes, size_t count,
                            uint8_t value)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != value) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayViolationRefereeAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->messages, 0, sizeof(assets->messages));
    memset(assets->sequences, 0, sizeof(assets->sequences));
    memset(assets->groups, 0, sizeof(assets->groups));
    assets->decoded_screen = NULL;
    assets->background_palette = NULL;
    assets->sprite_palette = NULL;
    memset(assets->background_chr_selectors, 0,
           sizeof(assets->background_chr_selectors));
    memset(assets->sprite_chr_selectors, 0,
           sizeof(assets->sprite_chr_selectors));
    assets->chr_fingerprint = 0U;
    assets->penalty_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGVR-1 rejected");
    return false;
}

void tecmo_gameplay_violation_referee_init(
    TecmoGameplayViolationRefereeAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag =
        TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG;
}

void tecmo_gameplay_violation_referee_destroy(
    TecmoGameplayViolationRefereeAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_violation_referee_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE &&
           memcmp(payload, "TGVR", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET &&
           read_u32(payload + 32U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_SIZE &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET &&
           read_u16(payload + 40U) ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT &&
           read_u16(payload + 42U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET &&
           read_u16(payload + 48U) ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT &&
           read_u16(payload + 50U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE &&
           read_u32(payload + 52U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET &&
           read_u16(payload + 56U) ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT &&
           read_u16(payload + 58U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE &&
           read_u32(payload + 60U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE &&
           read_u32(payload + 64U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32 &&
           read_u64(payload + 68U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A64 &&
           read_u32(payload + 76U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DESCRIPTOR_FNV1A32 &&
           read_u32(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_STREAM_FNV1A32 &&
           read_u32(payload + 84U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_FNV1A32 &&
           read_u32(payload + 88U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_BG_PALETTE_FNV1A32 &&
           read_u32(payload + 92U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SPRITE_PALETTE_FNV1A32 &&
           read_u32(payload + 96U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_FNV1A32 &&
           read_u32(payload + 100U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_FNV1A32 &&
           read_u32(payload + 104U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_FNV1A32 &&
           read_u32(payload + 108U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_FNV1A32 &&
           read_u32(payload + 112U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_FNV1A32 &&
           read_u32(payload + 116U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           read_u32(payload + 120U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32 &&
           read_u16(payload + 124U) == 0xDCA8U &&
           read_u16(payload + 126U) == 0xB600U &&
           read_u16(payload + 128U) == 0xB5F0U &&
           read_u16(payload + 130U) == 0xBA1FU &&
           payload[132U] == 5U && payload[133U] == 0U &&
           payload[134U] == 0xCAU && payload[135U] == 0xFAU &&
           payload[136U] == 0xCAU && payload[137U] == 0x71U &&
           payload[138U] == 0x72U && payload[139U] == 0x73U &&
           payload[140U] == 0U && payload[141U] == 16U &&
           payload[142U] == 4U &&
           payload[143U] ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS &&
           payload[144U] == 0x22U && payload[145U] == 16U &&
           payload[146U] == 120U && payload[147U] == 0U &&
           memcmp(payload + 148U, violation_referee_rev1_sha256,
                  sizeof(violation_referee_rev1_sha256)) == 0 &&
           bytes_are_zero(
               payload + 180U,
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_HEADER_SIZE -
                   180U);
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    size_t index;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT; ++index) {
        const TecmoGameplayViolationRefereeExpectedSource *expected =
            &tecmo_gameplay_violation_referee_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->payload_offset ||
            read_u32(record + 16U) != expected->fingerprint ||
            read_u16(record + 20U) != (uint16_t)index ||
            !bytes_are_zero(record + 22U, 10U) ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint) {
            return false;
        }
    }
    return fnv1a32(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_OFFSET,
               TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT *
                   TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCE_STRIDE) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SOURCES_FNV1A32 &&
           fnv1a32(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_OFFSET,
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_SIZE) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_RAW_FNV1A32 &&
           bytes_are_zero(payload + 3205U, 11U) &&
           bytes_are_zero(payload + 4436U, 12U) &&
           bytes_are_zero(payload + 4508U, 4U);
}

static bool validate_messages(const uint8_t *payload)
{
    size_t index;
    const uint8_t *records = payload +
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET;
    if (fnv1a32(
            records,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE) !=
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_FNV1A32) {
        return false;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT; ++index) {
        const uint8_t *record = records + index *
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE;
        uint16_t address = (uint16_t)(((uint16_t)record[4U] << 8U) |
                                      record[5U]);
        size_t nametable_offset;
        if (record[0U] != index + 1U || record[1U] != index + 1U ||
            record[2U] != expected_sequence_id[index] ||
            record[3U] != expected_message_length[index] ||
            record[4U] != 0x22U ||
            record[5U] != expected_message_ppu_low[index] ||
            !bytes_are_value(
                record + 6U + record[3U],
                TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_MESSAGE_TILES -
                    record[3U],
                0xFFU) ||
            !bytes_are_zero(record + 26U, 2U) ||
            address < 0x2000U) {
            return false;
        }
        nametable_offset = (size_t)(address - 0x2000U);
        if (nametable_offset >= 960U ||
            record[3U] > 960U - nametable_offset) {
            return false;
        }
    }
    return true;
}

static bool validate_sequences(const uint8_t *payload)
{
    const uint8_t *records = payload +
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET;
    size_t index;
    if (fnv1a32(
            records,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE) !=
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_FNV1A32) {
        return false;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT; ++index) {
        const uint8_t *record = records + index *
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE;
        size_t slot;
        if (record[0U] != index || record[1U] == 0U ||
            record[1U] >
                TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS ||
            !bytes_are_value(
                record + 2U + record[1U],
                TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS -
                    record[1U],
                0xFFU) ||
            !bytes_are_zero(record + 10U, 2U)) {
            return false;
        }
        for (slot = 0U; slot < record[1U]; ++slot) {
            if (record[2U + slot] >=
                TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT) {
                return false;
            }
        }
    }
    return true;
}

static bool validate_groups(const uint8_t *payload, size_t payload_size,
                            size_t chr_size)
{
    const TecmoGameplayViolationRefereeExpectedSource *source =
        &tecmo_gameplay_violation_referee_expected_sources[9U];
    const uint8_t *raw = payload + source->payload_offset;
    const uint8_t *records = payload +
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET;
    const uint8_t selectors[4] = {0xCAU,0x71U,0x72U,0x73U};
    size_t index;
    if (fnv1a32(
            records,
            TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE) !=
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_FNV1A32) {
        return false;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT; ++index) {
        const uint8_t *record = records + index *
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE;
        uint16_t pointer = (uint16_t)((uint16_t)raw[index] |
                            ((uint16_t)raw[15U + index] << 8U));
        size_t raw_record_offset;
        uint32_t piece_offset;
        uint16_t piece_size;
        size_t piece;
        if (pointer < 0xB33FU) return false;
        raw_record_offset = (size_t)(pointer - 0xB33FU);
        piece_offset = read_u32(record + 6U);
        piece_size = read_u16(record + 10U);
        if (record[0U] != index || record[1U] == 0U ||
            record[1U] > TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_PIECES ||
            read_u16(record + 4U) != pointer ||
            piece_size != (uint16_t)(record[1U] * 4U) ||
            raw_record_offset > source->byte_count - 3U ||
            raw[raw_record_offset] != record[1U] ||
            raw[raw_record_offset + 1U] != record[2U] ||
            raw[raw_record_offset + 2U] != record[3U] ||
            piece_offset != source->payload_offset +
                                raw_record_offset + 3U ||
            !range_ok(piece_offset, piece_size, payload_size) ||
            !bytes_are_zero(record + 12U, 4U)) {
            return false;
        }
        for (piece = 0U; piece < record[1U]; ++piece) {
            const uint8_t *piece_record = payload + piece_offset + piece * 4U;
            uint8_t tile = (uint8_t)(piece_record[1U] + 0x3CU);
            uint8_t selector = selectors[(tile >> 6U) & 3U];
            size_t chr_offset = (size_t)selector * 1024U +
                                (size_t)(tile & 0x3FU) * 16U;
            if ((piece_record[2U] & 0x3CU) != 0U ||
                chr_offset + 16U > chr_size) {
                return false;
            }
        }
    }
    return true;
}

static bool validate_dependencies(const uint8_t *chr, size_t chr_size,
                                  const uint8_t *penalties,
                                  size_t penalties_size)
{
    return chr != NULL &&
           chr_size ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE &&
           fnv1a32(chr, chr_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32 &&
           fnv1a64(chr, chr_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A64 &&
           penalties != NULL &&
           penalties_size == TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           memcmp(penalties, "TPNL", 4U) == 0 &&
           read_u16(penalties + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VERSION &&
           read_u32(penalties + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           fnv1a32(penalties, penalties_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
}

bool tecmo_gameplay_violation_referee_parse(
    TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *chr,
    size_t chr_size,
    const uint8_t *penalties,
    size_t penalties_size)
{
    uint8_t *storage;
    size_t index;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_violation_referee_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets,
                      "TGVR-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_FNV1A32 ||
        !validate_sources(payload, payload_size) ||
        fnv1a32(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_FNV1A32 ||
        !validate_messages(payload) || !validate_sequences(payload) ||
        !validate_groups(payload, payload_size, chr_size)) {
        return reject(assets,
                      "TGVR-1 canonical source/semantic contract rejected");
    }
    if (!validate_dependencies(chr, chr_size, penalties, penalties_size)) {
        return reject(
            assets,
            "TGVR-1 same-pack chr/all and gameplay/penalties dependencies rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGVR-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT; ++index) {
        const TecmoGameplayViolationRefereeExpectedSource *expected =
            &tecmo_gameplay_violation_referee_expected_sources[index];
        TecmoGameplayViolationRefereeSourceSpan *source =
            &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT; ++index) {
        const uint8_t *record = storage +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_STRIDE;
        TecmoGameplayViolationRefereeMessage *target =
            &assets->messages[index];
        target->selector = record[0U];
        target->violation = (TecmoGameplayViolation)record[1U];
        target->sequence_id = record[2U];
        target->tile_count = record[3U];
        target->ppu_address = (uint16_t)(((uint16_t)record[4U] << 8U) |
                                         record[5U]);
        target->tiles = record + 6U;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT; ++index) {
        const uint8_t *record = storage +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCES_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_STRIDE;
        TecmoGameplayViolationRefereeSequence *target =
            &assets->sequences[index];
        target->id = record[0U];
        target->group_count = record[1U];
        memcpy(target->groups, record + 2U, sizeof(target->groups));
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT; ++index) {
        const uint8_t *record = storage +
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUPS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_GROUP_STRIDE;
        TecmoGameplayViolationRefereeGroup *target = &assets->groups[index];
        target->id = record[0U];
        target->piece_count = record[1U];
        target->base_y = record[2U];
        target->base_x = record[3U];
        target->record_cpu = read_u16(record + 4U);
        target->pieces = storage + read_u32(record + 6U);
    }
    assets->decoded_screen = storage +
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_DECODED_OFFSET;
    assets->background_palette = storage +
        tecmo_gameplay_violation_referee_expected_sources[5U].payload_offset;
    assets->sprite_palette = storage +
        tecmo_gameplay_violation_referee_expected_sources[7U].payload_offset;
    memcpy(assets->background_chr_selectors, storage + 134U, 2U);
    memcpy(assets->sprite_chr_selectors, storage + 136U, 4U);
    assets->chr_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32;
    assets->penalty_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGVR-1 native violation referee cutaway assetpack");
    return true;
}

bool tecmo_gameplay_violation_referee_load(
    TecmoGameplayViolationRefereeAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint8_t *penalties = NULL;
    uint64_t payload_size = 0U;
    uint64_t chr_size = 0U;
    uint64_t penalties_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_violation_referee_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_ID,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGVR-1 gameplay/violation-referee entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE,
            &chr, &chr_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
            &penalties, &penalties_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(chr);
        tecmo_asset_pack_free(penalties);
        return reject(
            assets, "TGVR-1 same-pack dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_violation_referee_parse(
        assets, payload, (size_t)payload_size, chr, (size_t)chr_size,
        penalties, (size_t)penalties_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    tecmo_asset_pack_free(penalties);
    return loaded;
}

static bool assets_valid(
    const TecmoGameplayViolationRefereeAssets *assets)
{
    return assets != NULL &&
           assets->lifecycle_tag ==
               TECMO_GAMEPLAY_VIOLATION_REFEREE_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_FNV1A32 &&
           assets->decoded_screen != NULL &&
           assets->background_palette != NULL &&
           assets->sprite_palette != NULL &&
           assets->chr_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32 &&
           assets->penalty_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32;
}

const TecmoGameplayViolationRefereeSourceSpan *
tecmo_gameplay_violation_referee_find_source(
    const TecmoGameplayViolationRefereeAssets *assets,
    TecmoGameplayViolationRefereeSourceKind kind)
{
    size_t index;
    if (!assets_valid(assets)) return NULL;
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

static const TecmoGameplayViolationRefereeMessage *message_for_violation(
    const TecmoGameplayViolationRefereeAssets *assets,
    TecmoGameplayViolation violation)
{
    size_t index;
    if (!assets_valid(assets) ||
        violation < TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
        violation > TECMO_GAMEPLAY_VIOLATION_GOALTENDING) {
        return NULL;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT; ++index) {
        if (assets->messages[index].violation == violation) {
            return &assets->messages[index];
        }
    }
    return NULL;
}

static bool group_for_sequence_frame(
    const TecmoGameplayViolationRefereeAssets *assets,
    uint8_t sequence_id,
    uint16_t presentation_frames,
    uint16_t phase_frame,
    uint8_t *group_id)
{
    const TecmoGameplayViolationRefereeSequence *sequence;
    size_t slot;
    uint8_t selected;
    if (!assets_valid(assets) || group_id == NULL ||
        phase_frame >= presentation_frames ||
        sequence_id >= TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT) {
        return false;
    }
    selected = 0U;
    if (phase_frame >=
        TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_START_FRAME) {
        sequence = &assets->sequences[sequence_id];
        if (!assets_valid(assets) || sequence->id != sequence_id ||
            sequence->group_count == 0U ||
            sequence->group_count >
                TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS) {
            return false;
        }
        slot = (size_t)(phase_frame -
                        TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_START_FRAME) /
               TECMO_GAMEPLAY_VIOLATION_REFEREE_FADE_STEP_FRAMES;
        if (slot >= sequence->group_count) slot = sequence->group_count - 1U;
        selected = sequence->groups[slot];
    }
    if (selected >= TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT) {
        return false;
    }
    *group_id = selected;
    return true;
}

bool tecmo_gameplay_violation_referee_group_for_frame(
    const TecmoGameplayViolationRefereeAssets *assets,
    TecmoGameplayViolation violation,
    uint16_t phase_frame,
    uint8_t *group_id)
{
    const TecmoGameplayViolationRefereeMessage *message =
        message_for_violation(assets, violation);
    if (message == NULL) return false;
    return group_for_sequence_frame(
        assets, message->sequence_id,
        TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES,
        phase_frame, group_id);
}

bool tecmo_gameplay_violation_referee_foul_group_for_frame(
    const TecmoGameplayViolationRefereeAssets *assets,
    uint16_t phase_frame,
    uint8_t *group_id)
{
    /* Fixed $E95E: selector $22 -> Bank04 $BA1F. $BA88 maps its foul
       selector 0 to $B317 sequence 0, whose group stream is 1,2,2,2. */
    return group_for_sequence_frame(
        assets, 0U, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES,
        phase_frame, group_id);
}

static bool framebuffer_valid(const TecmoFramebuffer *framebuffer,
                              int origin_x, int origin_y, int scale)
{
    return framebuffer != NULL && framebuffer->pixels != NULL && scale > 0 &&
           origin_x >= 0 && origin_y >= 0 &&
           framebuffer->width >= origin_x + 256 * scale &&
           framebuffer->height >= origin_y + 240 * scale &&
           framebuffer->pitch_pixels >= framebuffer->width;
}

static void fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                      int width, int height, uint32_t color)
{
    int row;
    for (row = 0; row < height; ++row) {
        uint32_t *pixels = framebuffer->pixels +
            (size_t)(y + row) * (size_t)framebuffer->pitch_pixels +
            (size_t)x;
        int column;
        for (column = 0; column < width; ++column) pixels[column] = color;
    }
}

static uint8_t faded_color(uint8_t color, uint8_t brightness)
{
    uint8_t original = (uint8_t)(color & 0x3FU);
    uint8_t cap = (uint8_t)(brightness << 4U);
    if ((original & 0x30U) > cap) {
        return (uint8_t)((original & 0x0FU) | cap);
    }
    return original;
}

static bool foul_overlay_valid(
    const TecmoGameplayViolationRefereeFoulOverlay *overlay,
    size_t chr_size)
{
    size_t index;
    if (overlay == NULL) return true;
    if (overlay->cell_count >
        TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_FOUL_OVERLAY_CELLS) {
        return false;
    }
    for (index = 0U; index < overlay->cell_count; ++index) {
        const TecmoGameplayViolationRefereeOverlayCell *item =
            &overlay->cells[index];
        size_t previous;
        if (item->ppu_address < 0x2000U ||
            (size_t)(item->ppu_address - 0x2000U) >= 960U ||
            item->chr_offset > chr_size || chr_size - item->chr_offset < 16U) {
            return false;
        }
        for (previous = 0U; previous < index; ++previous) {
            if (overlay->cells[previous].ppu_address == item->ppu_address) {
                return false;
            }
        }
    }
    return true;
}

static const TecmoGameplayViolationRefereeOverlayCell *
foul_overlay_cell_at(const TecmoGameplayViolationRefereeFoulOverlay *overlay,
                     size_t screen_offset)
{
    size_t index;
    if (overlay == NULL) return NULL;
    for (index = 0U; index < overlay->cell_count; ++index) {
        const TecmoGameplayViolationRefereeOverlayCell *item =
            &overlay->cells[index];
        if ((size_t)(item->ppu_address - 0x2000U) == screen_offset) {
            return item;
        }
    }
    return NULL;
}

static bool draw_referee_presentation(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    const TecmoGameplayViolationRefereeMessage *message,
    const TecmoGameplayViolationRefereeFoulOverlay *foul_overlay,
    uint8_t sequence_id,
    uint16_t presentation_frames,
    uint16_t phase_frame)
{
    const TecmoGameplayViolationRefereeGroup *group;
    uint8_t group_id;
    uint8_t brightness;
    size_t message_offset;
    size_t cell;
    if (!assets_valid(assets) || chr == NULL ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE ||
        fnv1a32(chr, chr_size) != assets->chr_fingerprint ||
        !framebuffer_valid(framebuffer, origin_x, origin_y, scale) ||
        !foul_overlay_valid(foul_overlay, chr_size) ||
        !group_for_sequence_frame(
            assets, sequence_id, presentation_frames,
            phase_frame, &group_id)) {
        return false;
    }
    if (phase_frame < TECMO_GAMEPLAY_VIOLATION_REFEREE_BLACK_FRAMES) {
        fill_rect(framebuffer, origin_x, origin_y,
                  256 * scale, 240 * scale,
                  tecmo_nes_2c02_rgba(0x0FU));
        return true;
    }
    message_offset = 0U;
    if (message != NULL) {
        if (message->ppu_address < 0x2000U) return false;
        message_offset = (size_t)(message->ppu_address - 0x2000U);
        if (message_offset >= 960U ||
            message->tile_count > 960U - message_offset) {
            return false;
        }
    }
    brightness = (uint8_t)((phase_frame -
                            TECMO_GAMEPLAY_VIOLATION_REFEREE_BLACK_FRAMES) /
                           TECMO_GAMEPLAY_VIOLATION_REFEREE_FADE_STEP_FRAMES);
    if (brightness > 3U) brightness = 3U;
    fill_rect(framebuffer, origin_x, origin_y, 256 * scale, 240 * scale,
              tecmo_nes_2c02_rgba(
                  faded_color(assets->background_palette[0U], brightness)));
    for (cell = 0U; cell < 960U; ++cell) {
        size_t row = cell / 32U;
        size_t column = cell % 32U;
        size_t attribute_offset = 960U + (row / 4U) * 8U + column / 4U;
        uint8_t tile = assets->decoded_screen[cell];
        const TecmoGameplayViolationRefereeOverlayCell *overlay_cell;
        uint8_t palette_index;
        uint8_t selector;
        size_t chr_offset;
        uint32_t palette[4] = {0U,0U,0U,0U};
        size_t color;
        if (message != NULL && cell >= message_offset &&
            cell < message_offset + message->tile_count) {
            tile = message->tiles[cell - message_offset];
        }
        overlay_cell = foul_overlay_cell_at(foul_overlay, cell);
        palette_index = tecmo_nes_attribute_palette_index(
            assets->decoded_screen[attribute_offset],
            (int)row, (int)column);
        if (palette_index > 3U) return false;
        if (overlay_cell != NULL) {
            chr_offset = overlay_cell->chr_offset;
        } else {
            selector = assets->background_chr_selectors[(tile >> 7U) & 1U];
            chr_offset = (size_t)selector * 1024U +
                         (size_t)(tile & 0x7FU) * 16U;
        }
        if (chr_offset + 16U > chr_size) return false;
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(faded_color(
                assets->background_palette[
                    (size_t)palette_index * 4U + color],
                brightness));
        }
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, chr, chr_size, chr_offset,
            origin_x + (int)column * 8 * scale,
            origin_y + (int)row * 8 * scale,
            scale, palette, false, false);
    }
    group = &assets->groups[group_id];
    if (group->id != group_id || group->piece_count == 0U ||
        group->piece_count > TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_PIECES) {
        return false;
    }
    for (cell = group->piece_count; cell-- > 0U;) {
        const uint8_t *piece = group->pieces + cell * 4U;
        uint8_t tile = (uint8_t)(piece[1U] + 0x3CU);
        uint8_t selector = assets->sprite_chr_selectors[(tile >> 6U) & 3U];
        size_t chr_offset = (size_t)selector * 1024U +
                            (size_t)(tile & 0x3FU) * 16U;
        uint8_t attributes = piece[2U];
        bool flip_horizontal = (attributes & 0x40U) != 0U;
        bool flip_vertical = (attributes & 0x80U) != 0U;
        uint32_t palette[4] = {0U,0U,0U,0U};
        int x = origin_x +
            (int)(uint8_t)(group->base_x + piece[3U]) * scale;
        int y = origin_y +
            (int)(uint8_t)(group->base_y + piece[0U] + 1U) * scale;
        size_t color;
        if (chr_offset + 16U > chr_size ||
            (attributes & 0x3CU) != 0U) {
            return false;
        }
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(faded_color(
                assets->sprite_palette[
                    (size_t)(attributes & 3U) * 4U + color],
                brightness));
        }
        /* Bank04 $B33F records advance both axes and tile numbers in 8-pixel
           cells. Their tile selector uses all six low bits; pairing and
           clearing bit zero here incorrectly treated two adjacent 8x8 cells
           as one 8x16 sprite and stacked an extra arm/body tile. */
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, chr, chr_size, chr_offset, x, y, scale,
            palette, flip_horizontal, flip_vertical);
    }
    return true;
}

bool tecmo_gameplay_violation_referee_draw(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    TecmoGameplayViolation violation,
    uint16_t phase_frame)
{
    const TecmoGameplayViolationRefereeMessage *message =
        message_for_violation(assets, violation);
    if (message == NULL) return false;
    return draw_referee_presentation(
        assets, chr, chr_size, framebuffer, origin_x, origin_y, scale,
        message, NULL, message->sequence_id,
        TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES, phase_frame);
}

bool tecmo_gameplay_violation_referee_draw_foul(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint16_t phase_frame)
{
    return tecmo_gameplay_violation_referee_draw_foul_overlay(
        assets, chr, chr_size, framebuffer, origin_x, origin_y, scale,
        NULL, phase_frame);
}

bool tecmo_gameplay_violation_referee_draw_foul_overlay(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    const TecmoGameplayViolationRefereeFoulOverlay *overlay,
    uint16_t phase_frame)
{
    return draw_referee_presentation(
        assets, chr, chr_size, framebuffer, origin_x, origin_y, scale,
        NULL, overlay, 0U, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES,
        phase_frame);
}

bool tecmo_gameplay_violation_referee_self_test(
    const char *asset_pack_path, char *message, size_t message_size)
{
    TecmoGameplayViolationRefereeAssets assets;
    uint8_t *chr = NULL;
    uint64_t chr_size = 0U;
    uint32_t *pixels_a = NULL;
    uint32_t *pixels_b = NULL;
    TecmoFramebuffer framebuffer_a;
    TecmoFramebuffer framebuffer_b;
    const TecmoGameplayViolationRefereeSourceSpan *controller;
    const TecmoGameplayViolationRefereeSourceSpan *sequence_tables;
    uint8_t group_id;
    bool ok = false;
    tecmo_gameplay_violation_referee_init(&assets);
    if (!tecmo_gameplay_violation_referee_load(&assets, asset_pack_path) ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE,
            &chr, &chr_size) != 0) {
        (void)snprintf(message, message_size, "%s", assets.status);
        goto cleanup;
    }
    controller = tecmo_gameplay_violation_referee_find_source(
        &assets, TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_CONTROLLER);
    sequence_tables = tecmo_gameplay_violation_referee_find_source(
        &assets,
        TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SEQUENCE_TABLES);
    if (controller == NULL || sequence_tables == NULL ||
        controller->bank != 4U || controller->fixed_bank ||
        controller->cpu_start != 0xBA1FU || controller->byte_count != 211U ||
        sequence_tables->bank != 4U || sequence_tables->fixed_bank ||
        sequence_tables->cpu_start != 0xB317U ||
        sequence_tables->byte_count != 40U ||
        assets.sequences[0U].id != 0U ||
        assets.sequences[0U].group_count != 4U ||
        assets.sequences[0U].groups[0U] != 1U ||
        assets.sequences[0U].groups[1U] != 2U ||
        assets.sequences[0U].groups[2U] != 2U ||
        assets.sequences[0U].groups[3U] != 2U) {
        (void)snprintf(message, message_size,
                       "TGVR-1 Bank04 foul selector-0 provenance failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_group_for_frame(
            &assets, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 22U,
            &group_id) || group_id != 0U ||
        !tecmo_gameplay_violation_referee_group_for_frame(
            &assets, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 23U,
            &group_id) || group_id != 9U ||
        !tecmo_gameplay_violation_referee_group_for_frame(
            &assets, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 27U,
            &group_id) || group_id != 10U ||
        !tecmo_gameplay_violation_referee_group_for_frame(
            &assets, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 23U,
            &group_id) || group_id != 3U) {
        (void)snprintf(message, message_size,
                       "TGVR-1 selector-specific group schedule failed");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, 22U, &group_id) || group_id != 0U ||
        !tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, 23U, &group_id) || group_id != 1U ||
        !tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, 27U, &group_id) || group_id != 2U ||
        !tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, 31U, &group_id) || group_id != 2U ||
        !tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES - 1U,
            &group_id) || group_id != 2U ||
        tecmo_gameplay_violation_referee_foul_group_for_frame(
            &assets, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES,
            &group_id)) {
        (void)snprintf(message, message_size,
                       "TGVR-1 Bank04 foul selector-0 schedule failed");
        goto cleanup;
    }
    pixels_a = (uint32_t *)calloc(256U * 240U, sizeof(*pixels_a));
    pixels_b = (uint32_t *)calloc(256U * 240U, sizeof(*pixels_b));
    if (pixels_a == NULL || pixels_b == NULL) {
        (void)snprintf(message, message_size,
                       "TGVR-1 render self-test allocation failed");
        goto cleanup;
    }
    framebuffer_a.pixels = pixels_a;
    framebuffer_a.width = 256;
    framebuffer_a.height = 240;
    framebuffer_a.pitch_pixels = 256;
    framebuffer_b = framebuffer_a;
    framebuffer_b.pixels = pixels_b;
    if (!tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_a,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 23U) ||
        !tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_b,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 27U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) == 0) {
        (void)snprintf(message, message_size,
                       "TGVR-1 shot-clock referee frames did not animate");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_a,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 23U) ||
        !tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_b,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 27U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) == 0) {
        (void)snprintf(message, message_size,
                       "TGVR-1 out-of-bounds groups 3 and 4 did not animate");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_a,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 31U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) == 0) {
        (void)snprintf(message, message_size,
                       "TGVR-1 out-of-bounds groups 4 and 5 did not animate");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_draw(
            &assets, chr, (size_t)chr_size, &framebuffer_b,
            0, 0, 1, TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 39U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) != 0) {
        (void)snprintf(message, message_size,
                       "TGVR-1 out-of-bounds terminal group 5 did not hold");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_draw_foul(
            &assets, chr, (size_t)chr_size, &framebuffer_a,
            0, 0, 1, 23U) ||
        !tecmo_gameplay_violation_referee_draw_foul(
            &assets, chr, (size_t)chr_size, &framebuffer_b,
            0, 0, 1, 27U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) == 0) {
        (void)snprintf(message, message_size,
                       "TGVR-1 foul referee groups 1 and 2 did not animate");
        goto cleanup;
    }
    if (!tecmo_gameplay_violation_referee_draw_foul(
            &assets, chr, (size_t)chr_size, &framebuffer_a,
            0, 0, 1, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES - 1U) ||
        memcmp(pixels_a, pixels_b,
               256U * 240U * sizeof(*pixels_a)) != 0 ||
        tecmo_gameplay_violation_referee_draw_foul(
            &assets, chr, (size_t)chr_size, &framebuffer_b,
            0, 0, 1, TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES)) {
        (void)snprintf(message, message_size,
                       "TGVR-1 foul terminal group did not hold/reject");
        goto cleanup;
    }
    ok = true;
    (void)snprintf(message, message_size,
                   "TGVR-1 native violation referee self-test passed");
cleanup:
    free(pixels_a);
    free(pixels_b);
    tecmo_asset_pack_free(chr);
    tecmo_gameplay_violation_referee_destroy(&assets);
    return ok;
}
