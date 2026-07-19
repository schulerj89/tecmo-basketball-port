#include "tecmo_asset_pack_gameplay_jump_shots.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_gameplay_close_shots.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define JUMP_SHOT_PRG_BANK_COUNT 8U
#define JUMP_SHOT_POSE_LOW_CPU 0x8D3DU
#define JUMP_SHOT_POSE_HIGH_CPU 0x8D5DU

const TecmoGameplayJumpShotExpectedSource
    tecmo_gameplay_jump_shot_expected_sources[
        TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_FAMILY_BASES,
         0x8469U, 2U, 512U, 0x01767E9DU},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ANIMATION_COUNTER,
         0x8999U, 40U, 514U, 0xE2DEFE13U},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_INITIAL_VELOCITY,
         0x8D92U, 65U, 554U, 0x7C0668A1U},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_PHASE_DECREMENT,
         0x9C29U, 23U, 619U, 0xE4D07131U},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ROUTE1_FOLLOW_RELEASE,
         0xAD41U, 481U, 642U, 0x5C57F1D9U},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_ROUTE10,
         0xB6E5U, 144U, 1123U, 0x6AD67C6AU},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_BOUNCE_MOTION_COLLISION,
         0xB7C1U, 187U, 1267U, 0x7AC650D8U},
        {TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_MADE_SETTLEMENT,
         0xBA65U, 92U, 1454U, 0x130C585CU}
    };

const uint8_t tecmo_gameplay_jump_shot_expected_constants[
    TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE] = {
        0x40U,
        0x1EU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x00U,
        0x30U, 0x31U, 0x04U, 0x05U, 0x56U,
        0x12U, 0x01U, 0x05U, 0x17U, 0x10U, 0x00U,
        0x80U, 0x0BU, 0x0CU,
        0x28U, 0x00U, 0xF6U, 0x80U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U
    };

/* [family][profile bit][direction], normalized from the even byte offsets in
   Bank05's $8D3D/$8D5D low/high selector tables. */
const uint16_t tecmo_gameplay_jump_shot_expected_pose_indices[
    TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT] = {
        245U, 213U, 229U, 237U, 221U, 197U, 253U, 205U,
        309U, 277U, 293U, 301U, 285U, 261U, 317U, 269U,
        915U, 883U, 899U, 907U, 891U, 867U, 923U, 875U,
        979U, 947U, 963U, 971U, 955U, 931U, 987U, 939U
    };

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t bank05_cpu_offset(uint64_t prg_offset, uint16_t cpu)
{
    return prg_offset +
           (uint64_t)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_BANK *
               TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int validate_pose_source(const uint8_t *rom,
                                uint64_t rom_size,
                                uint64_t prg_offset,
                                char *message,
                                size_t message_size)
{
    uint64_t low_offset = bank05_cpu_offset(prg_offset,
                                            JUMP_SHOT_POSE_LOW_CPU);
    uint8_t source[TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U];
    uint8_t normalized[TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U];

    if (!range_ok(low_offset, sizeof(source), rom_size)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGJS-1 pose-selector dependency is outside the Rev1 ROM.");
        return -1;
    }
    memcpy(source, rom + (size_t)low_offset, sizeof(source));
    if (tecmo_asset_pack_fnv1a32(source, sizeof(source)) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGJS-1 pose-selector dependency fingerprint mismatch.");
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT; ++index) {
        uint16_t byte_offset = (uint16_t)(source[index] |
            ((uint16_t)source[TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT + index]
                << 8U));
        uint16_t pointer_index;
        if ((byte_offset & 1U) != 0U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGJS-1 pose-selector contains an odd pointer offset.");
            return -1;
        }
        pointer_index = (uint16_t)(byte_offset >> 1U);
        if (pointer_index !=
                tecmo_gameplay_jump_shot_expected_pose_indices[index] ||
            pointer_index >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGJS-1 pose selector %u mismatch (got %u).",
                (unsigned)index, (unsigned)pointer_index);
            return -1;
        }
        tecmo_asset_pack_store_u16(normalized + index * 2U,
                                   pointer_index);
    }
    if (tecmo_asset_pack_fnv1a32(normalized, sizeof(normalized)) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGJS-1 normalized pose-selector fingerprint mismatch.");
        return -1;
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_jump_shots(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayJumpShotProvenance *provenance,
    char *message,
    size_t message_size)
{
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE ||
        prg_banks != JUMP_SHOT_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGJS-1 import requires the exact Rev1 ROM contract.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayJumpShotExpectedSource *expected =
            &tecmo_gameplay_jump_shot_expected_sources[index];
        uint64_t source_offset = bank05_cpu_offset(prg_offset,
                                                   expected->cpu_start);
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        uint32_t fingerprint;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCE_STRIDE;
        if (expected->cpu_start < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
            cpu_end >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
            !range_ok(source_offset, expected->byte_count, rom_size) ||
            expected->payload_offset > payload_size ||
            expected->byte_count > payload_size - expected->payload_offset) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGJS-1 source range is outside the Rev1 ROM or payload.");
            return -1;
        }
        fingerprint = tecmo_asset_pack_fnv1a32(
            rom + (size_t)source_offset, expected->byte_count);
        if (fingerprint != expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGJS-1 Bank05 $%04X-$%04X fingerprint mismatch (got %08X, expected %08X).",
                (unsigned)expected->cpu_start, (unsigned)cpu_end,
                fingerprint, expected->fingerprint);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_BANK;
        record[3U] = 0U;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, expected->fingerprint);
        tecmo_asset_pack_store_u16(record + 20U, (uint16_t)index);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)source_offset, expected->byte_count);
        provenance->source_offsets[index] = source_offset;
    }
    if (tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32 ||
        validate_pose_source(rom, rom_size, prg_offset,
                             message, message_size) != 0) {
        return -1;
    }

    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET,
           tecmo_gameplay_jump_shot_expected_constants,
           TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE);
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT; ++index) {
        tecmo_asset_pack_store_u16(
            payload + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET +
                index * 2U,
            tecmo_gameplay_jump_shot_expected_pose_indices[index]);
    }

    memcpy(payload, "TGJS", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 40U, TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT);
    tecmo_asset_pack_store_u16(payload + 42U, 2U);
    tecmo_asset_pack_store_u32(payload + 44U,
                               TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(payload + 48U,
                               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 56U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 60U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 64U,
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 68U, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 72U,
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32);
    tecmo_asset_pack_store_u16(payload + 76U, JUMP_SHOT_POSE_LOW_CPU);
    tecmo_asset_pack_store_u16(payload + 78U, JUMP_SHOT_POSE_HIGH_CPU);
    tecmo_asset_pack_store_u16(payload + 80U,
                               TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT);
    tecmo_asset_pack_store_u16(payload + 82U,
                               TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT);
    tecmo_asset_pack_store_u16(payload + 84U,
                               TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT);

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGJS-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGJS-1 jump-shot asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_jump_shots_self_test(char *message,
                                                    size_t message_size)
{
    uint8_t pose_bytes[TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U];
    uint32_t expected_offset =
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET;
    size_t raw_size = 0U;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayJumpShotExpectedSource *source =
            &tecmo_gameplay_jump_shot_expected_sources[index];
        if (source->kind !=
                (TecmoGameplayJumpShotSourceKind)(index + 1U) ||
            source->payload_offset != expected_offset) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGJS-1 source layout self-test failed.");
            return -1;
        }
        expected_offset += source->byte_count;
        raw_size += source->byte_count;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT; ++index) {
        if (tecmo_gameplay_jump_shot_expected_pose_indices[index] >=
                TECMO_GAMEPLAY_ASSET_POINTER_COUNT) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGJS-1 pose index exceeds the TGPL pointer contract.");
            return -1;
        }
        tecmo_asset_pack_store_u16(
            pose_bytes + index * 2U,
            tecmo_gameplay_jump_shot_expected_pose_indices[index]);
    }
    if (raw_size != TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE ||
        expected_offset !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_OFFSET ||
        expected_offset + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET +
                sizeof(pose_bytes) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_jump_shot_expected_constants,
            sizeof(tecmo_gameplay_jump_shot_expected_constants)) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(pose_bytes, sizeof(pose_bytes)) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGJS-1 semantic layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGJS-1 layout self-test passed.");
    return 0;
}
