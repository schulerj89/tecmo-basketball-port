#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_jump_shots.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_close_shots.h"
#include "asset_pack/tecmo_asset_pack_gameplay_jump_shots.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_JUMP_SHOTS_LIFECYCLE_TAG 0x534A4754U
#define TECMO_GAMEPLAY_JUMP_SHOTS_POSE_LOW_CPU 0x8D3DU
#define TECMO_GAMEPLAY_JUMP_SHOTS_POSE_HIGH_CPU 0x8D5DU
#define TECMO_GAMEPLAY_JUMP_SHOTS_ACTOR_RECORD_CPU 0x8000U
#define TECMO_GAMEPLAY_JUMP_SHOTS_ACTOR_RECORD_END_CPU 0xA5B9U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    for (unsigned index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
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

static bool reject(
    TecmoGameplayJumpShotAssets *candidate,
    TecmoGameplayJumpShotAssets *destination,
    const char *message)
{
    bool publish_fresh_status = destination != NULL &&
        !destination->available && destination->storage == NULL;
    free(candidate->storage);
    candidate->storage = NULL;
    candidate->storage_size = 0U;
    candidate->pose_indices = NULL;
    candidate->gameplay_core_fingerprint = 0U;
    candidate->close_shots_fingerprint = 0U;
    memset(&candidate->constants, 0, sizeof(candidate->constants));
    memset(candidate->sources, 0, sizeof(candidate->sources));
    candidate->available = false;
    (void)snprintf(candidate->status, sizeof(candidate->status), "%s",
                   message != NULL ? message : "TGJS-2 rejected");
    if (publish_fresh_status) {
        (void)snprintf(destination->status, sizeof(destination->status), "%s",
                       candidate->status);
    }
    return false;
}

void tecmo_gameplay_jump_shots_init(TecmoGameplayJumpShotAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_JUMP_SHOTS_LIFECYCLE_TAG;
}

void tecmo_gameplay_jump_shots_destroy(TecmoGameplayJumpShotAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_JUMP_SHOTS_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_jump_shots_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE &&
           memcmp(payload, "TGJS", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET &&
           read_u32(payload + 32U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET &&
           read_u16(payload + 40U) ==
               TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT &&
           read_u16(payload + 42U) == 2U &&
           read_u32(payload + 44U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u32(payload + 48U) == TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           read_u32(payload + 52U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE &&
           read_u32(payload + 56U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32 &&
           read_u32(payload + 60U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32 &&
           read_u32(payload + 64U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32 &&
           read_u32(payload + 68U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32 &&
           read_u32(payload + 72U) ==
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32 &&
           read_u16(payload + 76U) ==
               TECMO_GAMEPLAY_JUMP_SHOTS_POSE_LOW_CPU &&
           read_u16(payload + 78U) ==
               TECMO_GAMEPLAY_JUMP_SHOTS_POSE_HIGH_CPU &&
           read_u16(payload + 80U) ==
               TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT &&
           read_u16(payload + 82U) ==
               TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT &&
           read_u16(payload + 84U) ==
               TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT &&
           read_u16(payload + 86U) == 0U &&
           bytes_are_zero(
               payload + 88U,
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_HEADER_SIZE - 88U);
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    uint32_t prior_cpu_end = 0U;
    uint32_t prior_payload_end =
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayJumpShotExpectedSource *expected =
            &tecmo_gameplay_jump_shot_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        uint32_t payload_end = expected->payload_offset +
                               expected->byte_count;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_BANK ||
            record[3U] != 0U ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->payload_offset ||
            read_u32(record + 16U) != expected->fingerprint ||
            read_u16(record + 20U) != (uint16_t)index ||
            read_u64(record + 22U) != expected->fingerprint_fnv1a64 ||
            !bytes_are_zero(record + 30U, 2U) ||
            expected->cpu_start < 0x8000U || cpu_end >= 0xC000U ||
            (index != 0U && expected->cpu_start <= prior_cpu_end) ||
            expected->payload_offset != prior_payload_end ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint ||
            fnv1a64(payload + expected->payload_offset,
                    expected->byte_count) !=
                expected->fingerprint_fnv1a64) {
            return false;
        }
        prior_cpu_end = cpu_end;
        prior_payload_end = payload_end;
    }
    return prior_payload_end ==
           TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_OFFSET;
}

static const TecmoGameplayExpectedSource *gameplay_rule_state_source(void)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        if (tecmo_gameplay_expected_sources[index].kind ==
            TECMO_GAMEPLAY_SOURCE_RULE_STATE) {
            return &tecmo_gameplay_expected_sources[index];
        }
    }
    return NULL;
}

static bool validate_pose_record(const uint8_t *gameplay_core,
                                 uint16_t pointer_index)
{
    const uint8_t *pointers = gameplay_core +
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET;
    const uint8_t *records = gameplay_core +
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET;
    uint16_t target;
    size_t record_offset;
    uint8_t dimensions;
    unsigned width;
    unsigned height;
    size_t piece_count;
    if (pointer_index >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) return false;
    target = read_u16(pointers + (size_t)pointer_index * 2U);
    if (target < TECMO_GAMEPLAY_JUMP_SHOTS_ACTOR_RECORD_CPU ||
        target >= TECMO_GAMEPLAY_JUMP_SHOTS_ACTOR_RECORD_END_CPU) {
        return false;
    }
    record_offset = (size_t)(target -
                             TECMO_GAMEPLAY_JUMP_SHOTS_ACTOR_RECORD_CPU);
    if (record_offset >= TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE) {
        return false;
    }
    dimensions = records[record_offset];
    width = dimensions & 0x0FU;
    height = dimensions >> 4U;
    piece_count = (size_t)width * height;
    return width != 0U && height != 0U &&
           piece_count <= TECMO_GAMEPLAY_RESOLVED_PIECE_MAX &&
           record_offset + 4U <=
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE &&
           piece_count <= TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE -
                              record_offset - 4U;
}

static bool validate_dependencies_and_poses(const uint8_t *payload,
                                            const uint8_t *gameplay_core,
                                            size_t gameplay_core_size,
                                            const uint8_t *close_shots,
                                            size_t close_shots_size)
{
    const TecmoGameplayExpectedSource *state_source =
        gameplay_rule_state_source();
    const uint8_t *pose_source;
    const uint8_t *poses = payload +
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET;
    size_t source_offset;
    if (gameplay_core == NULL ||
        gameplay_core_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        memcmp(gameplay_core, "TGPL", 4U) != 0 ||
        read_u16(gameplay_core + 4U) != TECMO_ASSET_PACK_GAMEPLAY_VERSION ||
        read_u16(gameplay_core + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE ||
        read_u32(gameplay_core + 8U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        fnv1a32(gameplay_core, gameplay_core_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        close_shots == NULL ||
        close_shots_size !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE ||
        memcmp(close_shots, "TGCS", 4U) != 0 ||
        read_u16(close_shots + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VERSION ||
        read_u32(close_shots + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE ||
        fnv1a32(close_shots, close_shots_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32 ||
        state_source == NULL ||
        TECMO_GAMEPLAY_JUMP_SHOTS_POSE_LOW_CPU < state_source->cpu_start ||
        TECMO_GAMEPLAY_JUMP_SHOTS_POSE_HIGH_CPU !=
            TECMO_GAMEPLAY_JUMP_SHOTS_POSE_LOW_CPU +
                TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT) {
        return false;
    }
    source_offset = state_source->payload_offset +
        (TECMO_GAMEPLAY_JUMP_SHOTS_POSE_LOW_CPU - state_source->cpu_start);
    if (!range_ok(source_offset,
                  TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U,
                  gameplay_core_size)) {
        return false;
    }
    pose_source = gameplay_core + source_offset;
    if (fnv1a32(pose_source,
                TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U) !=
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT; ++index) {
        uint16_t raw_offset = (uint16_t)(pose_source[index] |
            ((uint16_t)pose_source[
                TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT + index] << 8U));
        uint16_t pointer_index = read_u16(poses + index * 2U);
        if ((raw_offset & 1U) != 0U ||
            (uint16_t)(raw_offset >> 1U) != pointer_index ||
            pointer_index !=
                tecmo_gameplay_jump_shot_expected_pose_indices[index] ||
            !validate_pose_record(gameplay_core, pointer_index)) {
            return false;
        }
    }
    return true;
}

static bool validate_constants(const uint8_t *payload)
{
    const uint8_t *constants = payload +
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET;
    const uint8_t *poses = payload +
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET;
    if (memcmp(constants, tecmo_gameplay_jump_shot_expected_constants,
               TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE) != 0 ||
        fnv1a32(constants,
                TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32 ||
        fnv1a32(poses, TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT * 2U) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT; ++index) {
        if (read_u16(poses + index * 2U) !=
            tecmo_gameplay_jump_shot_expected_pose_indices[index]) {
            return false;
        }
    }
    return true;
}

static void load_constants(TecmoGameplayJumpShotConstants *constants,
                           const uint8_t *bytes)
{
    constants->nes_b_mask = bytes[0U];
    constants->actor_state_gather = bytes[1U];
    constants->actor_state_prepared = bytes[2U];
    constants->actor_state_held = bytes[3U];
    constants->actor_state_airborne = bytes[4U];
    constants->actor_state_recovery = bytes[5U];
    constants->actor_state_neutral = bytes[6U];
    constants->phase_seed_gather = bytes[7U];
    constants->phase_seed_prepared = bytes[8U];
    constants->phase_seed_airborne = bytes[9U];
    constants->phase_seed_recovery = bytes[10U];
    constants->phase_seed_recovery_counter = bytes[11U];
    constants->ball_state_launch = bytes[12U];
    constants->ball_state_route1 = bytes[13U];
    constants->ball_state_route5 = bytes[14U];
    constants->ball_state_route17 = bytes[15U];
    constants->ball_state_route10 = bytes[16U];
    constants->ball_state_neutral = bytes[17U];
    constants->outcome_flag_mask = bytes[18U];
    constants->crowd_sfx = bytes[19U];
    constants->side_result_base = bytes[20U];
    constants->gravity_q8 = read_u16(bytes + 21U);
    constants->floor_wrap_clamp = bytes[23U];
    constants->bounce_decay_q8 = read_u16(bytes + 24U);
    constants->made_state = bytes[26U];
    constants->made_initial_timer = bytes[27U];
    constants->made_repeat_timer = bytes[28U];
    constants->made_terminal_stage = bytes[29U];
    constants->made_update_count = bytes[30U];
    constants->flight_state = bytes[31U];
    constants->result_state = bytes[32U];
    constants->flight_count_limit = bytes[33U];
    constants->flight_result_altitude_threshold = bytes[34U];
    constants->made_complete_state = bytes[35U];
}

bool tecmo_gameplay_jump_shots_parse(
    TecmoGameplayJumpShotAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *close_shots,
    size_t close_shots_size)
{
    TecmoGameplayJumpShotAssets candidate;
    TecmoGameplayJumpShotAssets previous;
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_JUMP_SHOTS_LIFECYCLE_TAG) {
        return false;
    }
    /* Parse into independent storage and publish only after every dependency,
       source span, and pose pointer has passed validation. */
    tecmo_gameplay_jump_shots_init(&candidate);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(&candidate, assets,
                      "TGJS-2 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_FNV1A32) {
        return reject(&candidate, assets,
                      "TGJS-2 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_SIZE) ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32 ||
        !validate_constants(payload)) {
        return reject(&candidate, assets,
                      "TGJS-2 source/semantic contract rejected");
    }
    if (!validate_dependencies_and_poses(
            payload, gameplay_core, gameplay_core_size,
            close_shots, close_shots_size)) {
        return reject(&candidate, assets,
                      "TGJS-2 same-pack TGPL-1/TGCS-1 dependencies rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) {
        return reject(&candidate, assets, "TGJS-2 allocation failed");
    }
    memcpy(storage, payload, payload_size);
    candidate.storage = storage;
    candidate.storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayJumpShotExpectedSource *expected =
            &tecmo_gameplay_jump_shot_expected_sources[index];
        TecmoGameplayJumpShotSourceSpan *source = &candidate.sources[index];
        source->kind = expected->kind;
        source->bank = TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_BANK;
        source->fixed_bank = false;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->fingerprint_fnv1a64 = expected->fingerprint_fnv1a64;
        source->bytes = storage + expected->payload_offset;
    }
    load_constants(
        &candidate.constants,
        storage + TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET);
    candidate.pose_indices = storage +
        TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET;
    candidate.gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    candidate.close_shots_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32;
    candidate.available = true;
    (void)snprintf(candidate.status, sizeof(candidate.status),
                   "TGJS-2 gameplay jump-shot assetpack");
    previous = *assets;
    *assets = candidate;
    free(previous.storage);
    return true;
}

bool tecmo_gameplay_jump_shots_load(TecmoGameplayJumpShotAssets *assets,
                                    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *close_shots = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t close_shots_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_JUMP_SHOTS_LIFECYCLE_TAG) {
        return false;
    }
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_ID,
            TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE,
            &payload, &payload_size) != 0) {
        if (!assets->available) {
            (void)snprintf(assets->status, sizeof(assets->status),
                           "TGJS-2 gameplay/jump-shots entry missing or wrong-sized");
        }
        return false;
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        if (!assets->available) {
            (void)snprintf(assets->status, sizeof(assets->status),
                           "TGJS-2 gameplay/core dependency missing or wrong-sized");
        }
        return false;
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_ID,
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE,
            &close_shots, &close_shots_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        if (!assets->available) {
            (void)snprintf(assets->status, sizeof(assets->status),
                           "TGJS-2 gameplay/close-shots dependency missing or wrong-sized");
        }
        return false;
    }
    loaded = tecmo_gameplay_jump_shots_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        close_shots, (size_t)close_shots_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(close_shots);
    return loaded;
}

const TecmoGameplayJumpShotSourceSpan *tecmo_gameplay_jump_shots_find_source(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) return &assets->sources[index];
    }
    return NULL;
}

bool tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFamily family,
    TecmoGameplayJumpShotProfile profile,
    TecmoGameplayJumpShotDirection direction,
    uint16_t *pointer_index)
{
    size_t index;
    uint16_t value;
    if (assets == NULL || !assets->available || pointer_index == NULL ||
        (unsigned)family >= TECMO_GAMEPLAY_JUMP_SHOT_FAMILY_COUNT ||
        (unsigned)profile >= TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT ||
        (unsigned)direction >= TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT) {
        return false;
    }
    index = (((size_t)family * TECMO_GAMEPLAY_JUMP_SHOT_PROFILE_COUNT +
              (size_t)profile) *
                 TECMO_GAMEPLAY_JUMP_SHOT_DIRECTION_COUNT) +
            (size_t)direction;
    value = read_u16(assets->pose_indices + index * 2U);
    if (value >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) return false;
    *pointer_index = value;
    return true;
}

bool tecmo_gameplay_jump_shots_resolve_phase_pose_pointer_index(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFamily family,
    TecmoGameplayJumpShotProfile profile,
    TecmoGameplayJumpShotDirection direction,
    uint8_t animation_byte,
    uint16_t *pointer_index)
{
    uint16_t base;
    uint16_t resolved;
    if (pointer_index == NULL ||
        !tecmo_gameplay_jump_shots_resolve_pose_pointer_index(
            assets, family, profile, direction, &base)) {
        return false;
    }
    resolved = (uint16_t)(base + (animation_byte & 0x0FU));
    if (resolved >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) return false;
    *pointer_index = resolved;
    return true;
}

bool tecmo_gameplay_jump_shots_step_q8(
    const TecmoGameplayJumpShotAssets *assets,
    uint16_t *altitude_q8,
    uint16_t *velocity_q8,
    bool *landed)
{
    uint16_t next_velocity;
    uint16_t next_altitude;
    uint8_t height;
    if (assets == NULL || !assets->available || altitude_q8 == NULL ||
        velocity_q8 == NULL || landed == NULL ||
        assets->constants.gravity_q8 == 0U ||
        assets->constants.floor_wrap_clamp == 0U) {
        return false;
    }
    next_velocity = (uint16_t)(*velocity_q8 -
                               assets->constants.gravity_q8);
    next_altitude = (uint16_t)(*altitude_q8 + next_velocity);
    height = (uint8_t)(next_altitude >> 8U);
    if (height == 0U ||
        height >= assets->constants.floor_wrap_clamp) {
        next_altitude = 0U;
        next_velocity = 0U;
        *landed = true;
    } else {
        *landed = false;
    }
    *altitude_q8 = next_altitude;
    *velocity_q8 = next_velocity;
    return true;
}

bool tecmo_gameplay_jump_shots_step_distance_flight(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotFlightState *state)
{
    TecmoGameplayJumpShotFlightState next;
    bool landed = false;
    if (assets == NULL || !assets->available || state == NULL ||
        state->object_state != assets->constants.flight_state ||
        state->remaining_updates > assets->constants.flight_count_limit) {
        return false;
    }
    next = *state;
    if (next.remaining_updates == 0U) {
        if ((next.altitude_q8 >> 8U) <
            assets->constants.flight_result_altitude_threshold) {
            next.object_state = assets->constants.result_state;
            *state = next;
            return true;
        }
    } else {
        next.world_x_q6 =
            (uint16_t)(next.world_x_q6 + next.velocity_x_q6);
        next.world_y_q6 =
            (uint16_t)(next.world_y_q6 + next.velocity_y_q6);
        --next.remaining_updates;
    }
    if (!tecmo_gameplay_jump_shots_step_q8(
            assets, &next.altitude_q8,
            &next.altitude_velocity_q8, &landed)) {
        return false;
    }
    *state = next;
    return true;
}

bool tecmo_gameplay_jump_shots_begin_distance_flight(
    const TecmoGameplayJumpShotAssets *assets,
    uint16_t start_x,
    uint8_t start_y,
    uint16_t target_x,
    uint8_t target_y,
    uint16_t start_altitude_q8,
    uint8_t context,
    TecmoGameplayJumpShotFlightState *state)
{
    static const uint8_t altitude_bases[4] = {0x40U, 0x40U, 0x40U, 0xC8U};
    const TecmoGameplayJumpShotSourceSpan *table;
    TecmoGameplayJumpShotFlightState next;
    int32_t dx;
    int32_t dy;
    uint32_t absolute_dx;
    uint32_t count;
    int altitude_delta;
    int altitude_eighth;
    uint8_t altitude_factor;
    if (assets == NULL || !assets->available || state == NULL ||
        context >= 4U) {
        return false;
    }
    table = tecmo_gameplay_jump_shots_find_source(
        assets, TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_DISTANCE_TABLE);
    if (table == NULL || table->byte_count != 256U ||
        table->bytes == NULL) {
        return false;
    }
    dx = (int16_t)(target_x - start_x);
    dy = (int16_t)((uint16_t)target_y - (uint16_t)start_y);
    absolute_dx = target_x >= start_x
        ? (uint32_t)(target_x - start_x)
        : (uint32_t)(start_x - target_x);
    count = table->bytes[start_y];
    if (absolute_dx >= 0x20U) {
        count = (uint16_t)((uint16_t)absolute_dx + (uint16_t)count);
        count >>= 1U;
    }
    if (count > assets->constants.flight_count_limit) {
        count = assets->constants.flight_count_limit;
    }
    if (count == 0U) return false;
    altitude_delta = (int)(start_altitude_q8 >> 8U) -
                     (int)altitude_bases[context];
    altitude_eighth = altitude_delta >= 0
        ? altitude_delta / 8
        : -(((-altitude_delta) + 7) / 8);
    altitude_factor = (uint8_t)(0x14 - altitude_eighth);
    memset(&next, 0, sizeof(next));
    next.world_x_q6 = (uint16_t)(start_x << 6U);
    next.world_y_q6 = (uint16_t)((uint16_t)start_y << 6U);
    next.velocity_x_q6 = (uint16_t)(
        (int32_t)(int16_t)((uint16_t)dx << 6U) / (int32_t)count);
    next.velocity_y_q6 = (uint16_t)(
        (int32_t)(int16_t)((uint16_t)dy << 6U) / (int32_t)count);
    next.altitude_q8 = start_altitude_q8;
    next.altitude_velocity_q8 =
        (uint16_t)(count * altitude_factor);
    next.remaining_updates = (uint16_t)count;
    next.object_state = assets->constants.flight_state;
    *state = next;
    return true;
}

bool tecmo_gameplay_jump_shots_made_settlement_begin(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotMadeSettlement *settlement)
{
    TecmoGameplayJumpShotMadeSettlement next;
    if (assets == NULL || !assets->available || settlement == NULL ||
        assets->constants.made_state == 0U ||
        assets->constants.made_initial_timer == 0U ||
        assets->constants.made_repeat_timer == 0U ||
        assets->constants.made_terminal_stage == 0U ||
        assets->constants.made_update_count == 0U) {
        return false;
    }
    memset(&next, 0, sizeof(next));
    next.state = assets->constants.made_state;
    next.timer = assets->constants.made_initial_timer;
    *settlement = next;
    return true;
}

bool tecmo_gameplay_jump_shots_made_settlement_step(
    const TecmoGameplayJumpShotAssets *assets,
    TecmoGameplayJumpShotMadeSettlement *settlement,
    bool stalled)
{
    TecmoGameplayJumpShotMadeSettlement next;
    uint8_t expected_stage;
    uint8_t expected_timer;
    if (assets == NULL || !assets->available || settlement == NULL ||
        settlement->state != assets->constants.made_state ||
        settlement->complete ||
        settlement->timer == 0U ||
        settlement->stage >= assets->constants.made_terminal_stage ||
        settlement->updates >= assets->constants.made_update_count) {
        return false;
    }
    if (settlement->updates < assets->constants.made_initial_timer) {
        expected_stage = 0U;
        expected_timer = (uint8_t)(
            assets->constants.made_initial_timer - settlement->updates);
    } else {
        uint8_t repeated = (uint8_t)(
            settlement->updates - assets->constants.made_initial_timer);
        expected_stage = (uint8_t)(1U +
            repeated / assets->constants.made_repeat_timer);
        expected_timer = (uint8_t)(
            assets->constants.made_repeat_timer -
            repeated % assets->constants.made_repeat_timer);
    }
    if (settlement->stage != expected_stage ||
        settlement->timer != expected_timer) {
        return false;
    }
    next = *settlement;
    if (stalled) return true;
    --next.timer;
    ++next.updates;
    if (next.timer == 0U) {
        ++next.stage;
        if (next.stage >= assets->constants.made_terminal_stage) {
            next.complete = true;
            next.state = assets->constants.made_complete_state;
        } else {
            next.timer = assets->constants.made_repeat_timer;
        }
    }
    if (next.complete !=
        (next.updates == assets->constants.made_update_count)) {
        return false;
    }
    *settlement = next;
    return true;
}
