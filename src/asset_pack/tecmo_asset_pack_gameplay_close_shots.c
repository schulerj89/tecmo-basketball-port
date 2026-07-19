#include "tecmo_asset_pack_gameplay_close_shots.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define CLOSE_SHOT_PRG_BANK_COUNT 8U
#define CLOSE_SHOT_POSE_TABLE_ENTRY_COUNT 40U
#define CLOSE_SHOT_VARIANT0_PHASE_CPU 0x85D3U
#define CLOSE_SHOT_VARIANT2_PHASE_CPU 0x8685U
#define CLOSE_SHOT_POSE_LOW_CPU 0x8CEDU
#define CLOSE_SHOT_POSE_HIGH_CPU 0x8D15U

const TecmoGameplayCloseShotExpectedSource
    tecmo_gameplay_close_shot_expected_sources[
        TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_8542_8694,
         0x8542U, 339U, 672U, 0x7FF62153U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_919C_91BB,
         0x919CU, 32U, 1011U, 0x8FBAE6F7U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_98E1_9A5F,
         0x98E1U, 383U, 1043U, 0x0A2F945AU},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_A214_A25E,
         0xA214U, 75U, 1426U, 0x4FC82BF8U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_A503_A6ED,
         0xA503U, 491U, 1501U, 0x0EBCAB02U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_AB36_AC09,
         0xAB36U, 212U, 1992U, 0x161533A4U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B100_B13E,
         0xB100U, 63U, 2204U, 0x1B3B5F1DU},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B32C_B521,
         0xB32CU, 502U, 2267U, 0x82A40926U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B678_B6E4,
         0xB678U, 109U, 2769U, 0x026830BDU},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B775_B7AC,
         0xB775U, 56U, 2878U, 0x2E13AB9DU},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BDEF_BDF6,
         0xBDEFU, 8U, 2934U, 0x9D46B0CDU},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BFC2_BFC8,
         0xBFC2U, 7U, 2942U, 0xD2AABFC3U},
        {TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE,
         0x8CEDU, 80U, 2949U, 0x9BFCCE7CU}
    };

const uint8_t tecmo_gameplay_close_shot_expected_variant0_phases[
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT] = {
        1U, 2U, 3U, 3U, 3U, 4U, 4U, 4U,
        4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U,
        6U, 6U, 6U, 6U, 6U, 6U, 6U, 5U,
        5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U
    };

const uint8_t tecmo_gameplay_close_shot_expected_variant2_phases[
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT] = {
        0U, 1U, 2U, 3U, 3U, 4U, 4U, 4U,
        5U, 5U, 5U, 5U, 5U, 5U, 5U, 5U
    };

/* Layout is [supported variant slot][profile][direction]. */
const uint16_t tecmo_gameplay_close_shot_expected_pose_bases[
    TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT] = {
        637U, 609U, 623U, 630U, 616U, 595U, 644U, 602U,
        693U, 665U, 679U, 686U, 672U, 651U, 700U, 658U,
        807U, 783U, 795U, 801U, 789U, 771U, 813U, 777U,
        855U, 831U, 843U, 849U, 837U, 819U, 861U, 825U
    };

static bool range_ok(uint64_t offset, uint64_t size, uint64_t total)
{
    return offset <= total && size <= total - offset;
}

static uint64_t bank05_cpu_offset(uint64_t prg_offset, uint16_t cpu)
{
    return prg_offset +
           (uint64_t)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_BANK *
               TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int resolve_pose_bases(const uint8_t *pose_table,
                              uint16_t bases[
                                  TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT],
                              char *message,
                              size_t message_size)
{
    static const uint8_t starts[4] = {0U, 8U, 24U, 32U};
    size_t output = 0U;
    if (pose_table == NULL || bases == NULL) return -1;
    for (size_t group = 0U; group < 4U; ++group) {
        for (size_t direction = 0U;
             direction < TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT;
             ++direction) {
            size_t index = (size_t)starts[group] + direction;
            uint16_t byte_offset = (uint16_t)(
                (uint16_t)pose_table[index] |
                ((uint16_t)pose_table[
                    CLOSE_SHOT_POSE_TABLE_ENTRY_COUNT + index] << 8U));
            uint16_t pointer_index;
            if ((byte_offset & 1U) != 0U) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TGCS-1 pose table contains an odd actor-pointer offset.");
                return -1;
            }
            pointer_index = (uint16_t)(byte_offset >> 1U);
            if (pointer_index !=
                    tecmo_gameplay_close_shot_expected_pose_bases[output]) {
                tecmo_asset_pack_set_messagef(
                    message, message_size,
                    "TGCS-1 pose base %u mismatch (got %u).",
                    (unsigned)output, (unsigned)pointer_index);
                return -1;
            }
            bases[output++] = pointer_index;
        }
    }
    return output == TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT ? 0 : -1;
}

int tecmo_asset_pack_build_gameplay_close_shots(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCloseShotProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint16_t pose_bases[TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT];
    const uint8_t *variant0_phases;
    const uint8_t *variant2_phases;
    const uint8_t *pose_table;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE ||
        prg_banks != CLOSE_SHOT_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCS-1 import requires the exact Rev1 ROM contract.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCloseShotExpectedSource *expected =
            &tecmo_gameplay_close_shot_expected_sources[index];
        uint64_t source_offset = bank05_cpu_offset(prg_offset,
                                                   expected->cpu_start);
        uint32_t fingerprint;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (expected->cpu_start < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
            cpu_end >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
            !range_ok(source_offset, expected->byte_count, rom_size) ||
            expected->payload_offset > payload_size ||
            expected->byte_count > payload_size - expected->payload_offset) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGCS-1 source range is outside the Rev1 ROM or payload.");
            return -1;
        }
        fingerprint = tecmo_asset_pack_fnv1a32(
            rom + (size_t)source_offset, expected->byte_count);
        if (fingerprint != expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGCS-1 Bank05 $%04X-$%04X fingerprint mismatch (got %08X, expected %08X).",
                (unsigned)expected->cpu_start, (unsigned)cpu_end,
                fingerprint, expected->fingerprint);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_BANK;
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
            payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGCS-1 raw aggregate mismatch.");
        return -1;
    }

    variant0_phases = payload +
        tecmo_gameplay_close_shot_expected_sources[0].payload_offset +
        (CLOSE_SHOT_VARIANT0_PHASE_CPU - 0x8542U);
    variant2_phases = payload +
        tecmo_gameplay_close_shot_expected_sources[0].payload_offset +
        (CLOSE_SHOT_VARIANT2_PHASE_CPU - 0x8542U);
    pose_table = payload +
        tecmo_gameplay_close_shot_expected_sources[12].payload_offset;
    if (memcmp(variant0_phases,
               tecmo_gameplay_close_shot_expected_variant0_phases,
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) != 0 ||
        memcmp(variant2_phases,
               tecmo_gameplay_close_shot_expected_variant2_phases,
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCS-1 phase semantic contract mismatch.");
        return -1;
    }
    if (resolve_pose_bases(pose_table, pose_bases,
                           message, message_size) != 0) {
        return -1;
    }

    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET,
           variant0_phases, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT);
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET,
           variant2_phases, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT);
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT; ++index) {
        tecmo_asset_pack_store_u16(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET +
                index * 2U,
            pose_bases[index]);
    }

    memcpy(payload, "TGCS", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT);
    tecmo_asset_pack_store_u32(
        payload + 36U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 48U, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 50U, TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 52U, TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 54U, TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT);
    tecmo_asset_pack_store_u32(payload + 56U,
                               TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(payload + 60U,
                               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 64U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 68U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 72U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 76U, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PHASES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 80U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 84U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 88U,
        TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE);
    tecmo_asset_pack_store_u32(
        payload + 92U,
        TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE);
    tecmo_asset_pack_store_u16(payload + 96U,
                               CLOSE_SHOT_VARIANT0_PHASE_CPU);
    tecmo_asset_pack_store_u16(payload + 98U,
                               CLOSE_SHOT_VARIANT2_PHASE_CPU);
    tecmo_asset_pack_store_u16(payload + 100U, CLOSE_SHOT_POSE_LOW_CPU);
    tecmo_asset_pack_store_u16(payload + 102U, CLOSE_SHOT_POSE_HIGH_CPU);
    payload[104U] = 0U;
    payload[105U] = 2U;
    tecmo_asset_pack_store_u32(payload + 108U,
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_COUNT);

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCS-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGCS-1 close-shot asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_close_shots_self_test(char *message,
                                                     size_t message_size)
{
    uint8_t pose_base_bytes[TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT * 2U];
    size_t raw_size = 0U;
    uint32_t expected_offset = TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCloseShotExpectedSource *source =
            &tecmo_gameplay_close_shot_expected_sources[index];
        if (source->payload_offset != expected_offset ||
            source->kind != (TecmoGameplayCloseShotSourceKind)(index + 1U)) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGCS-1 source layout self-test failed.");
            return -1;
        }
        expected_offset += source->byte_count;
        raw_size += source->byte_count;
    }
    if (raw_size != TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE ||
        expected_offset != TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_OFFSET ||
        expected_offset + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET +
                TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET +
                TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET +
                TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT * 2U !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_close_shot_expected_variant0_phases,
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_close_shot_expected_variant2_phases,
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCS-1 semantic layout self-test failed.");
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT; ++index) {
        tecmo_asset_pack_store_u16(pose_base_bytes + index * 2U,
            tecmo_gameplay_close_shot_expected_pose_bases[index]);
    }
    if (tecmo_asset_pack_fnv1a32(pose_base_bytes,
                                 sizeof(pose_base_bytes)) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCS-1 semantic layout self-test failed.");
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT; ++index) {
        unsigned phase_limit = index < 16U
            ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_POSE_PHASE_COUNT
            : TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_POSE_PHASE_COUNT;
        if ((unsigned)tecmo_gameplay_close_shot_expected_pose_bases[index] +
                phase_limit - 1U >= 1179U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGCS-1 resolved pose exceeds the TGPL pointer contract.");
            return -1;
        }
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGCS-1 layout self-test passed.");
    return 0;
}
