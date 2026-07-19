#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_close_shots.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_close_shots.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_CLOSE_SHOTS_LIFECYCLE_TAG 0x53434754U
#define TECMO_GAMEPLAY_CLOSE_SHOTS_POINTER_COUNT 1179U
#define TECMO_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_ENTRY_COUNT 40U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint32_t fnv1a32_continue(uint32_t hash,
                                 uint8_t low,
                                 uint8_t high)
{
    hash ^= low;
    hash *= 16777619U;
    hash ^= high;
    hash *= 16777619U;
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool semantic_kind_from_numeric(
    uint8_t numeric_variant,
    TecmoGameplayCloseShotSemanticKind *semantic_kind)
{
    if (semantic_kind == NULL) return false;
    if (numeric_variant == 0U) {
        *semantic_kind = TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_DUNK;
        return true;
    }
    if (numeric_variant == 2U) {
        *semantic_kind = TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_LAYUP;
        return true;
    }
    *semantic_kind = TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_UNKNOWN;
    return false;
}

static bool reject(TecmoGameplayCloseShotAssets *assets, const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->variant0_phases = NULL;
    assets->variant2_phases = NULL;
    assets->pose_bases = NULL;
    assets->gameplay_core_fingerprint = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGCS-1 rejected");
    return false;
}

void tecmo_gameplay_close_shots_init(TecmoGameplayCloseShotAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_CLOSE_SHOTS_LIFECYCLE_TAG;
}

void tecmo_gameplay_close_shots_destroy(TecmoGameplayCloseShotAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CLOSE_SHOTS_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_close_shots_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE &&
           memcmp(payload, "TGCS", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET &&
           read_u32(payload + 32U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET &&
           read_u32(payload + 40U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET &&
           read_u16(payload + 48U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_COUNT &&
           read_u16(payload + 50U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT &&
           read_u16(payload + 52U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT &&
           read_u16(payload + 54U) ==
               TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT &&
           read_u32(payload + 56U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u32(payload + 60U) == TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           read_u32(payload + 64U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32 &&
           read_u32(payload + 68U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_FNV1A32 &&
           read_u32(payload + 72U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_FNV1A32 &&
           read_u32(payload + 76U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PHASES_FNV1A32 &&
           read_u32(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_FNV1A32 &&
           read_u32(payload + 84U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_FNV1A32 &&
           read_u32(payload + 88U) ==
               (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT |
                TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE) &&
           read_u32(payload + 92U) ==
               (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC |
                TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY |
                TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE) &&
           read_u16(payload + 96U) == 0x85D3U &&
           read_u16(payload + 98U) == 0x8685U &&
           read_u16(payload + 100U) == 0x8CEDU &&
           read_u16(payload + 102U) == 0x8D15U &&
           payload[104U] == 0U && payload[105U] == 2U &&
           read_u16(payload + 106U) == 0U &&
           read_u32(payload + 108U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_COUNT &&
           bytes_are_zero(payload + 112U,
                          TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_HEADER_SIZE -
                              112U);
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCloseShotExpectedSource *expected =
            &tecmo_gameplay_close_shot_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_BANK ||
            record[3U] != 0U ||
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
    return true;
}

static bool validate_semantics(const uint8_t *payload)
{
    static const uint8_t group_starts[4] = {0U, 8U, 24U, 32U};
    const uint8_t *variant0 = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET;
    const uint8_t *variant2 = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET;
    const uint8_t *pose_bases = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET;
    const uint8_t *raw_first = payload +
        tecmo_gameplay_close_shot_expected_sources[0].payload_offset;
    const uint8_t *raw_pose = payload +
        tecmo_gameplay_close_shot_expected_sources[12].payload_offset;
    size_t base_index = 0U;
    uint32_t resolved_hash = 2166136261U;
    TecmoGameplayCloseShotSemanticKind variant0_kind;
    TecmoGameplayCloseShotSemanticKind variant2_kind;

    /* TGCS-1 keeps numeric IDs at bytes 104..105. Semantic names are derived
       from this exact, fingerprinted ordering and never accepted from loose
       metadata or capture files. */
    if (!semantic_kind_from_numeric(payload[104U], &variant0_kind) ||
        !semantic_kind_from_numeric(payload[105U], &variant2_kind) ||
        variant0_kind != TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_DUNK ||
        variant2_kind != TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_LAYUP ||
        memcmp(variant0,
               tecmo_gameplay_close_shot_expected_variant0_phases,
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) != 0 ||
        memcmp(variant2,
               tecmo_gameplay_close_shot_expected_variant2_phases,
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) != 0 ||
        memcmp(variant0, raw_first + (0x85D3U - 0x8542U),
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) != 0 ||
        memcmp(variant2, raw_first + (0x8685U - 0x8542U),
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) != 0 ||
        fnv1a32(variant0, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_FNV1A32 ||
        fnv1a32(variant2, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_FNV1A32 ||
        fnv1a32(variant0,
                TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT +
                    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PHASES_FNV1A32 ||
        fnv1a32(pose_bases,
                TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT * 2U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_FNV1A32 ||
        fnv1a32(raw_pose, 80U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_FNV1A32) {
        return false;
    }

    for (size_t group = 0U; group < 4U; ++group) {
        for (size_t direction = 0U;
             direction < TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT;
             ++direction) {
            size_t raw_index = (size_t)group_starts[group] + direction;
            uint16_t byte_offset = (uint16_t)(raw_pose[raw_index] |
                ((uint16_t)raw_pose[
                    TECMO_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_ENTRY_COUNT +
                    raw_index] << 8U));
            uint16_t base = read_u16(pose_bases + base_index * 2U);
            unsigned phase_count = base_index < 16U
                ? TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_POSE_PHASE_COUNT
                : TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_POSE_PHASE_COUNT;
            if ((byte_offset & 1U) != 0U ||
                (uint16_t)(byte_offset >> 1U) != base ||
                base != tecmo_gameplay_close_shot_expected_pose_bases[
                            base_index] ||
                (unsigned)base + phase_count >
                    TECMO_GAMEPLAY_CLOSE_SHOTS_POINTER_COUNT) {
                return false;
            }
            for (unsigned phase = 0U; phase < phase_count; ++phase) {
                uint16_t pointer = (uint16_t)(base + phase);
                resolved_hash = fnv1a32_continue(
                    resolved_hash, (uint8_t)(pointer & 0xFFU),
                    (uint8_t)(pointer >> 8U));
            }
            ++base_index;
        }
    }
    return base_index == TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT &&
           resolved_hash ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_FNV1A32;
}

static bool validate_gameplay_core(const uint8_t *gameplay_core,
                                   size_t gameplay_core_size)
{
    return gameplay_core != NULL &&
           gameplay_core_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(gameplay_core, "TGPL", 4U) == 0 &&
           read_u16(gameplay_core + 4U) == TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           read_u16(gameplay_core + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE &&
           read_u32(gameplay_core + 8U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           fnv1a32(gameplay_core, gameplay_core_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
}

bool tecmo_gameplay_close_shots_parse(
    TecmoGameplayCloseShotAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CLOSE_SHOTS_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_close_shots_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets, "TGCS-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32) {
        return reject(assets,
                      "TGCS-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_SIZE) ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32 ||
        !validate_semantics(payload)) {
        return reject(assets, "TGCS-1 source/semantic contract rejected");
    }
    if (!validate_gameplay_core(gameplay_core, gameplay_core_size)) {
        return reject(assets,
                      "TGCS-1 same-pack gameplay/core dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGCS-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCloseShotExpectedSource *expected =
            &tecmo_gameplay_close_shot_expected_sources[index];
        TecmoGameplayCloseShotSourceSpan *source = &assets->sources[index];
        source->kind = expected->kind;
        source->bank = TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_BANK;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    assets->variant0_phases = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET;
    assets->variant2_phases = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET;
    assets->pose_bases = storage +
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET;
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGCS-1 gameplay close-shot assetpack");
    return true;
}

bool tecmo_gameplay_close_shots_load(TecmoGameplayCloseShotAssets *assets,
                                     const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_CLOSE_SHOTS_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_close_shots_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_ID,
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE,
            &payload, &payload_size) != 0) {
        return reject(assets,
                      "TGCS-1 gameplay/close-shots entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(assets,
                      "TGCS-1 gameplay/core dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_close_shots_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    return loaded;
}

const TecmoGameplayCloseShotSourceSpan *tecmo_gameplay_close_shots_find_source(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) return &assets->sources[index];
    }
    return NULL;
}

bool tecmo_gameplay_close_shots_get_variant_info(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    TecmoGameplayCloseShotVariantInfo *info)
{
    if (assets == NULL || !assets->available || info == NULL) return false;
    memset(info, 0, sizeof(*info));
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0) {
        info->numeric_variant = 0U;
        if (!semantic_kind_from_numeric(info->numeric_variant,
                                        &info->semantic_kind)) {
            return false;
        }
        info->family_flags =
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE;
        info->step_count = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT;
        info->pose_phase_count =
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_POSE_PHASE_COUNT;
        return true;
    }
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2) {
        info->numeric_variant = 2U;
        if (!semantic_kind_from_numeric(info->numeric_variant,
                                        &info->semantic_kind)) {
            return false;
        }
        info->family_flags =
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY |
            TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE;
        info->step_count = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT;
        info->pose_phase_count =
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_POSE_PHASE_COUNT;
        return true;
    }
    return false;
}

bool tecmo_gameplay_close_shots_phase_for_step(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    uint8_t step,
    uint8_t *phase)
{
    if (assets == NULL || !assets->available || phase == NULL) return false;
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0 &&
        step < TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT) {
        *phase = assets->variant0_phases[step];
        return true;
    }
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2 &&
        step < TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT) {
        *phase = assets->variant2_phases[step];
        return true;
    }
    return false;
}

bool tecmo_gameplay_close_shots_resolve_pose_pointer_index(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    TecmoGameplayCloseShotProfile profile,
    TecmoGameplayCloseShotDirection direction,
    uint8_t phase,
    uint16_t *pointer_index)
{
    size_t variant_slot;
    unsigned phase_count;
    size_t base_index;
    uint16_t base;
    if (assets == NULL || !assets->available || pointer_index == NULL ||
        (unsigned)profile >= TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT ||
        (unsigned)direction >= TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT) {
        return false;
    }
    if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0) {
        variant_slot = 0U;
        phase_count = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_POSE_PHASE_COUNT;
    } else if (variant == TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2) {
        variant_slot = 1U;
        phase_count = TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_POSE_PHASE_COUNT;
    } else {
        return false;
    }
    if ((unsigned)phase >= phase_count) return false;
    base_index = ((variant_slot * TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT +
                   (unsigned)profile) *
                      TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT) +
                 (unsigned)direction;
    base = read_u16(assets->pose_bases + base_index * 2U);
    if ((unsigned)base + phase >= TECMO_GAMEPLAY_CLOSE_SHOTS_POINTER_COUNT) {
        return false;
    }
    *pointer_index = (uint16_t)(base + phase);
    return true;
}
