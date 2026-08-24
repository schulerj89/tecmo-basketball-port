#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_cpu_global_latch.h"
#include "tecmo_gameplay_cpu_a9da_assignment.h"
#include "tecmo_gameplay_cpu_a0f3_launch.h"
#include "tecmo_gameplay_fixed_rng.h"
#include "tecmo_gameplay_defense_interaction.h"
#include "tecmo_gameplay_cpu_a8e9_velocity.h"
#include "tecmo_gameplay_cpu_opcode15_selection.h"

#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_CPU_STEERING_LIFECYCLE_TAG 0x49414754U
#define CPU_STEERING_REV1_ROM_SIZE 393232U
#define CPU_STEERING_REV1_ROM_FNV1A32 0x0650F5B0U
#define CPU_STEERING_COMMAND_STREAM_SIZE \
    (TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT * \
     TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)
#define CPU_STEERING_ROUTE_SHORT_OFFSET 0x007DU
#define CPU_STEERING_ROUTE_LONG_OFFSET 0x00D7U

static const uint16_t cpu_steering_command_counts[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    98U,143U,150U,171U,2U,1U,1U,2U,8U,12U,1U,2U,
    1U,2U,2U,2U,2U,64U,0U,0U,2U,6U,7U,1U
};

static const uint8_t cpu_steering_fixed_link[
    TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT] = {
    5U,6U,7U,8U,9U,0U,1U,2U,3U,4U
};

static const TecmoGameplayCpuSteeringEffectKind
    cpu_steering_effect_kind[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_RELATIVE_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GOTO,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ABSOLUTE_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_WAIT_COUNTDOWN,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_TRANSITION_RESET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_STATE_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_BOUNDARY_BRANCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_STATE_ANIMATION,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_PROXIMITY,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_RELATIVE_POSE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_FOLLOW_UP,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GROUP_RESEED,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_PRIMARY_DEFENDER_SWITCH,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_POINTER_ACTOR_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_CONDITIONAL_ADVANCE,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TIMERS,
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE_ALT
};

static const TecmoGameplayCpuSteeringAdvancePolicy
    cpu_steering_effect_policy[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    /* 0 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 1 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_JUMP,
    /* 2 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 3 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_WAIT,
    /* 4 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 5 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 6 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_NONE,
    /* 7 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_BRANCH_PLUS_FIVE,
    /* 8 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 9 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 10 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_BA_NONE_OR_FIVE_OR_RETRY_CANCEL,
    /* 11 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 12 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_OPCODE12_ZERO_FIVE_OR_TEN,
    /* 13 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 14 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 15 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 16 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
    /* 17 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 18 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 19 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 20 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 21 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_FIVE_OR_TEN,
    /* 22 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE,
    /* 23 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE
};

static const uint8_t cpu_steering_effect_jumps[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0U,1U,0U,0U,0U,0U,0U,1U,1U,0U,1U,0U,
    1U,0U,0U,1U,0U,0U,0U,0U,0U,1U,0U,0U
};

/* The native executor only claims source-bounded effects where the required
   RAM/workspace inputs are represented by the public contract. The remaining
   entries retain handler metadata and advance policy but defer their effect. */
static const uint8_t cpu_steering_effect_exact[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    1U,1U,1U,1U,1U,1U,1U,1U,1U,1U,1U,1U,
    1U,1U,1U,0U,1U,1U,1U,1U,1U,1U,1U,1U
};

static const uint8_t cpu_steering_effect_deferred[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0U,0U,0U,0U,0U,0U,1U,0U,0U,0U,0U,0U,
    0U,0U,0U,1U,0U,0U,0U,0U,0U,0U,0U,1U
};

static const uint8_t cpu_steering_effect_approximation[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U
};

static const uint8_t cpu_steering_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

#define CPU_STEERING_OPCODE15_DEFENDER_REQUIRED_RAW \
    (TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK & \
     ~((uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_LINKS | \
       (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT))
#define CPU_STEERING_OPCODE15_PRIMARY_REQUIRED_RAW \
    ((uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_04B0 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FLAGS_007E | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_0308 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_OFFENSE_SIDE_030A | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SIDE_SELECTION_000E | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_LIFECYCLE | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DIRECTION_0463 | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_LINKS | \
     (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT)

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
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

static bool reject(TecmoGameplayCpuSteeringAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->handler_cpu, 0, sizeof(assets->handler_cpu));
    memset(assets->command_count_by_opcode, 0,
           sizeof(assets->command_count_by_opcode));
    memset(assets->effect_metadata, 0, sizeof(assets->effect_metadata));
    memset(assets->direction_map, 0, sizeof(assets->direction_map));
    memset(assets->opcode15_pose_low_0442, 0,
           sizeof(assets->opcode15_pose_low_0442));
    memset(assets->opcode15_pose_high_044d, 0,
           sizeof(assets->opcode15_pose_high_044d));
    memset(assets->fixed_link, 0, sizeof(assets->fixed_link));
    memset(assets->formation_stream_offsets, 0,
           sizeof(assets->formation_stream_offsets));
    memset(assets->formation_source_pinned, 0,
           sizeof(assets->formation_source_pinned));
    assets->command_base_cpu = 0U;
    assets->command_record_count = 0U;
    assets->formation_start_count = 0U;
    assets->formation_source_pinned_count = 0U;
    assets->movement_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGAI-3 rejected");
    return false;
}

void tecmo_gameplay_cpu_steering_assets_init(
    TecmoGameplayCpuSteeringAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_CPU_STEERING_LIFECYCLE_TAG;
}

void tecmo_gameplay_cpu_steering_assets_destroy(
    TecmoGameplayCpuSteeringAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_cpu_steering_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    static const uint8_t direction_map[8] = {3U,6U,4U,7U,0U,1U,2U,5U};
    if (payload_size != TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        memcmp(payload, "TGAI", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        read_u16(payload + 12U) !=
            TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET ||
        read_u32(payload + 20U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE ||
        read_u32(payload + 24U) !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 ||
        read_u32(payload + 28U) != CPU_STEERING_REV1_ROM_SIZE ||
        read_u32(payload + 32U) != CPU_STEERING_REV1_ROM_FNV1A32 ||
        memcmp(payload + 36U, cpu_steering_rev1_sha256,
               sizeof(cpu_steering_rev1_sha256)) != 0 ||
        read_u16(payload + 68U) != 0x9F2EU ||
        read_u16(payload + 70U) !=
            TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE ||
        read_u16(payload + 72U) !=
            TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT ||
        read_u16(payload + 74U) !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT ||
        read_u16(payload + 76U) !=
            TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT ||
        read_u16(payload + 78U) != 10U ||
        payload[80U] != 4U || payload[81U] != 6U ||
        payload[82U] != 7U || payload[83U] != 4U ||
        read_u16(payload + 84U) != 0x00C7U ||
        !bytes_are_zero(payload + 86U, 2U) ||
        memcmp(payload +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECTION_OFFSET,
               direction_map, sizeof(direction_map)) != 0 ||
        read_u32(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_EFFECT_MASK_OFFSET) !=
            0x00113415U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECT_DIRECTION_OPCODE_OFFSET] !=
            5U ||
        !bytes_are_zero(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECT_DIRECTION_OPCODE_OFFSET +
                1U,
            3U) ||
        read_u16(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET) !=
            15U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                2U] !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_BANK ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                3U] != 0U ||
        read_u16(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_CPU_START ||
        read_u16(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE ||
        read_u32(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32 ||
        read_u16(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                 12U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET ||
        read_u16(payload +
                     TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
                 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE ||
        fnv1a32(payload +
                    TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32 ||
        !bytes_are_zero(payload +
                            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
                            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE,
                        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_EFFECT_MASK_OFFSET -
                            (TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
                             TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE)) ||
        !bytes_are_zero(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_EFFECT_MASK_OFFSET +
                sizeof(uint32_t),
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECT_DIRECTION_OPCODE_OFFSET -
                (TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_EFFECT_MASK_OFFSET +
                 sizeof(uint32_t))) ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_METADATA_END_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE -
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_METADATA_END_OFFSET)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        const uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_STRIDE;
        if (read_u32(descriptor) != expected->payload_offset ||
            read_u32(descriptor + 4U) != expected->byte_count ||
            read_u32(descriptor + 8U) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE;
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u32(record + 16U) != expected->payload_offset ||
            !bytes_are_zero(record + 20U, 12U) ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_padding(const uint8_t *payload)
{
    size_t previous_end =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
        TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT *
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *source =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        if (source->payload_offset < previous_end ||
            !bytes_are_zero(payload + previous_end,
                            source->payload_offset - previous_end)) {
            return false;
        }
        previous_end = source->payload_offset + source->byte_count;
    }
    return previous_end <= TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE &&
           bytes_are_zero(
               payload + previous_end,
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE - previous_end);
}

static bool validate_handlers_and_commands(const uint8_t *payload)
{
    const uint8_t *dispatch = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_OFFSET;
    const uint8_t *commands = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        uint16_t handler = read_u16(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HANDLER_OFFSET +
                index * 2U);
        if (dispatch[(0x8BB1U - 0x8B90U) + index] !=
                (uint8_t)(handler & 0xFFU) ||
            dispatch[(0x8BC9U - 0x8B90U) + index] !=
                (uint8_t)(handler >> 8U)) {
            return false;
        }
    }
    for (size_t offset = 0U;
         offset < TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE;
         offset += TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) {
        if (commands[offset] >=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
            return false;
        }
    }
    return memcmp(commands, "\x04\x0A\x00\x00\x00", 5U) == 0 &&
           memcmp(commands + 0x007DU,
                  "\x03\x08\x00\x00\x00", 5U) == 0 &&
           memcmp(commands + 0x00D7U,
                  "\x02\xB4\x00\x96\x00", 5U) == 0 &&
           memcmp(commands +
                      TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE -
                      TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE,
                  "\x01\x80\x0C\x00\x00", 5U) == 0;
}

static const char *opcode15_contract_error(const uint8_t *payload)
{
    const uint8_t *handlers = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET;
    const uint8_t *commands = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET;
    const uint8_t *helper = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET;
    static const uint8_t record[5] = {0x0FU,0U,0U,0U,0U};
    size_t handler_offset =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_CPU_START -
        0x8BE1U;
    if (!range_ok(handler_offset,
                  TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_SIZE,
                  TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_SIZE)) {
        return "TGAI-3 opcode-15 handler range rejected";
    }
    if (fnv1a32(handlers + handler_offset,
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_FNV1A32) {
        return "TGAI-3 opcode-15 full handler anchor rejected";
    }
    if (memcmp(handlers + handler_offset,
               "\xA0\x09\x84\xA4\xE4\xA4\xF0\x16"
               "\xB9\xB0\x04\x29\x10\xF0\x0F"
               "\xA9\x23\x99\x47\x05\xA9\x00"
               "\x99\x51\x05\xA9\x04\x99\x7C\x05",
               30U) != 0) {
        return "TGAI-3 neighboring opcode-14 anchor rejected";
    }
    if (memcmp(handlers + (0x9172U - 0x8BE1U),
               "\xAD\x99\x04\xC9\x46\xB0\x01\x60"
               "\xBD\xB0\x04\x29\x10\xD0\x41",
               15U) != 0) {
        return "TGAI-3 opcode-15 gate anchor rejected";
    }
    if (memcmp(handlers + (0x9181U - 0x8BE1U),
               "\xA5\x7E\x29\x04\xD0\xF2", 6U) != 0) {
        return "TGAI-3 opcode-15 $9185->$9179 return rejected";
    }
    if (fnv1a32(handlers + (0x9187U - 0x8BE1U), 0x3BU) !=
            0xB33C7281U) {
        return "TGAI-3 opcode-15 primary lifecycle anchor rejected";
    }
    if (memcmp(handlers + (0x91C2U - 0x8BE1U),
               "\xA5\x7E\x29\x08\xD0\xB1", 6U) != 0) {
        return "TGAI-3 opcode-15 $91C6->$9179 return rejected";
    }
    if (memcmp(handlers + (0x91C8U - 0x8BE1U),
               "\xAC\x09\x03\x8E\x09\x03\xA9\x04"
               "\x99\x7C\x05\xA9\x5A\x99\x47\x05"
               "\xA9\x00\x99\x51\x05\xA9\x00\x99\x6E\x04",
               26U) != 0) {
        return "TGAI-3 opcode-15 defender-write anchor rejected";
    }
    if (memcmp(handlers + (0x9208U - 0x8BE1U),
               "\xA9\x07\x9D\x7C\x05\x8E\x9E\x05"
               "\x8A\xA8\xA9\x04\x4C\x11\xC7", 15U) != 0 ||
        fnv1a32(handlers + (0x9208U - 0x8BE1U),
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_FINAL_TAIL_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_FINAL_TAIL_FNV1A32) {
        return "TGAI-3 opcode-15 canonical Rev1 tail anchor rejected";
    }
    if (memcmp(helper,
               "\xBC\x63\x04\xB9\xCA\x88\x9D\x42\x04"
               "\xB9\xD2\x88\x9D\x4D\x04\xA9\xC1\x9D"
               "\x79\x04\xA9\x30\x9D\x58\x04\x60"
               "\x0C\x0A\x10\x0C\x0A\x0E\x0C\x0A"
               "\x04\x04\x04\x04\x04\x04\x04\x04",
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE) != 0) {
        return "TGAI-3 opcode-15 $88B0 helper raw anchor rejected";
    }
    if (memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET,
               record, sizeof(record)) != 0 ||
        memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET,
               record, sizeof(record)) != 0 ||
        fnv1a32(commands +
                    TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET,
                sizeof(record)) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32 ||
        fnv1a32(commands +
                    TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET,
                sizeof(record)) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32) {
        return "TGAI-3 canonical opcode-15 record anchor rejected";
    }
    return NULL;
}

static bool validate_opcode15_contract(const uint8_t *payload)
{
    return opcode15_contract_error(payload) == NULL;
}

static bool validate_lifecycle_command_corpus(const uint8_t *payload)
{
    const uint8_t *commands = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET;
    uint16_t counts[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {0U};
    for (size_t offset = 0U;
         offset < CPU_STEERING_COMMAND_STREAM_SIZE;
         offset += TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) {
        uint8_t opcode = commands[offset];
        if (opcode >= TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
            return false;
        }
        ++counts[opcode];
        if (opcode == 1U) {
            uint16_t target = (uint16_t)commands[offset + 1U] |
                ((uint16_t)commands[offset + 2U] << 8U);
            if (target >= CPU_STEERING_COMMAND_STREAM_SIZE ||
                target % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE != 0U) {
                return false;
            }
        }
    }
    return memcmp(counts, cpu_steering_command_counts,
                  sizeof(counts)) == 0;
}

static bool decode_formation_native_fields(
    TecmoGameplayCpuSteeringAssets *assets)
{
    const TecmoGameplayCpuSteeringSourceSpan *source =
        &assets->sources[
            TECMO_GAMEPLAY_CPU_STEERING_SOURCE_FORMATION_STREAM_SELECT - 1U];
    size_t pinned_count = 0U;
    if (source->bytes == NULL || source->cpu_start != 0x938BU ||
        source->cpu_end != 0x9620U || source->byte_count != 662U) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT;
         ++index) {
        size_t depth = index / 12U;
        size_t x_bucket = index % 12U;
        uint16_t low_start = (uint16_t)(0x94A1U + depth * 0x30U +
                                        x_bucket * 4U);
        uint16_t high_start = (uint16_t)(0x9561U + depth * 0x30U +
                                         x_bucket * 4U);
        bool pinned = true;
        for (size_t actor = 0U;
             actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
            uint16_t low_address = (uint16_t)(low_start + actor);
            uint16_t high_address = (uint16_t)(high_start + actor);
            size_t low_offset = (size_t)(low_address - source->cpu_start);
            size_t high_offset = (size_t)(high_address - source->cpu_start);
            uint16_t stream_offset = 0U;
            if (!range_ok(low_offset, 1U, source->byte_count) ||
                !range_ok(high_offset, 1U, source->byte_count)) {
                pinned = false;
                continue;
            }
            stream_offset = (uint16_t)source->bytes[low_offset] |
                ((uint16_t)source->bytes[high_offset] << 8U);
            assets->formation_stream_offsets[index][actor] = stream_offset;
            if (stream_offset >= CPU_STEERING_COMMAND_STREAM_SIZE ||
                stream_offset % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE !=
                    0U) {
                pinned = false;
            }
        }
        assets->formation_source_pinned[index] = pinned ? 1U : 0U;
        if (pinned) ++pinned_count;
    }
    assets->formation_source_pinned_count = (uint16_t)pinned_count;
    /* There are 48 theoretical starts (4 depths x 12 buckets), but the
       source span ends at $9620. Rows 46 and 47 cross that boundary and are
       intentionally unavailable to the native selector. */
    return pinned_count ==
        TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT;
}

static bool validate_dependency(const uint8_t *movement,
                                size_t movement_size)
{
    return movement != NULL &&
           movement_size == TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE &&
           memcmp(movement, "TGMO", 4U) == 0 &&
           read_u16(movement + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_VERSION &&
           read_u16(movement + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_HEADER_SIZE &&
           read_u32(movement + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE &&
           fnv1a32(movement, movement_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
}

bool tecmo_gameplay_cpu_steering_assets_parse(
    TecmoGameplayCpuSteeringAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *movement,
    size_t movement_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_cpu_steering_assets_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(
            assets, "TGAI-3 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32) {
        return reject(
            assets, "TGAI-3 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size)) {
        return reject(assets, "TGAI-3 source descriptor/raw contract rejected");
    }
    if (!validate_padding(payload)) {
        return reject(assets, "TGAI-3 payload padding contract rejected");
    }
    if (!validate_handlers_and_commands(payload) ||
        !validate_lifecycle_command_corpus(payload)) {
        return reject(assets, "TGAI-3 command corpus contract rejected");
    }
    if (!validate_opcode15_contract(payload)) {
        return reject(assets, opcode15_contract_error(payload));
    }
    if (!validate_dependency(movement, movement_size)) {
        return reject(
            assets, "TGAI-3 same-pack TGMO-1 dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGAI-3 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        TecmoGameplayCpuSteeringSourceSpan *source =
            &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)(
            (uint32_t)expected->cpu_start + expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    assets->command_base_cpu = read_u16(storage + 68U);
    assets->command_record_count = read_u16(storage + 72U);
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        assets->handler_cpu[index] = read_u16(
            storage +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HANDLER_OFFSET +
                index * 2U);
        assets->command_count_by_opcode[index] =
            cpu_steering_command_counts[index];
        assets->effect_metadata[index].kind =
            cpu_steering_effect_kind[index];
        assets->effect_metadata[index].handler_cpu =
            assets->handler_cpu[index];
        assets->effect_metadata[index].corpus_count =
            cpu_steering_command_counts[index];
        assets->effect_metadata[index].exact_bounded =
            cpu_steering_effect_exact[index] != 0U;
        assets->effect_metadata[index].deferred_inputs =
            cpu_steering_effect_deferred[index] != 0U;
        assets->effect_metadata[index].native_approximation =
            cpu_steering_effect_approximation[index] != 0U;
        assets->effect_metadata[index].can_jump =
            cpu_steering_effect_jumps[index] != 0U;
        assets->effect_metadata[index].intent_inferred =
            index != 1U && index != 17U && index != 18U && index != 19U &&
            index != 21U && index != 22U;
        assets->effect_metadata[index].advance_policy =
            cpu_steering_effect_policy[index];
    }
    memcpy(assets->direction_map,
           storage +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECTION_OFFSET,
           sizeof(assets->direction_map));
    memcpy(assets->opcode15_pose_low_0442,
           storage +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_POSE_LOW_OFFSET,
           sizeof(assets->opcode15_pose_low_0442));
    memcpy(assets->opcode15_pose_high_044d,
           storage +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_POSE_HIGH_OFFSET,
           sizeof(assets->opcode15_pose_high_044d));
    memcpy(assets->fixed_link, cpu_steering_fixed_link,
           sizeof(assets->fixed_link));
    assets->formation_start_count =
        TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT;
    if (!decode_formation_native_fields(assets)) {
        return reject(assets, "TGAI-3 formation semantic decode rejected");
    }
    assets->movement_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGAI-3 CPU steering evidence assetpack (exact planar route kernel available; LIVE route integration deferred)");
    return true;
}

bool tecmo_gameplay_cpu_steering_assets_load(
    TecmoGameplayCpuSteeringAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *movement = NULL;
    uint64_t payload_size = 0U;
    uint64_t movement_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_cpu_steering_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ID,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGAI-3 gameplay/cpu-steering entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            &movement, &movement_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets, "TGAI-3 same-pack TGMO-1 dependency missing");
    }
    loaded = tecmo_gameplay_cpu_steering_assets_parse(
        assets, payload, (size_t)payload_size,
        movement, (size_t)movement_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(movement);
    return loaded;
}

const TecmoGameplayCpuSteeringSourceSpan *
tecmo_gameplay_cpu_steering_find_source(
    const TecmoGameplayCpuSteeringAssets *assets,
    TecmoGameplayCpuSteeringSourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

static TecmoGameplayCpuSteeringCommandKind command_kind(uint8_t opcode)
{
    switch (opcode) {
    case 0U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_RELATIVE_TARGET;
    case 2U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ABSOLUTE_TARGET;
    case 4U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ACTOR_TARGET;
    case 5U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_DIRECT_DIRECTION;
    case 10U:
    case 12U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_LINKED_TARGET;
    case 13U:
    case 20U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_GLOBAL_TARGET;
    case 16U:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_POINTER_ACTOR_TARGET;
    default:
        return TECMO_GAMEPLAY_CPU_STEERING_COMMAND_CONTROL;
    }
}

bool tecmo_gameplay_cpu_steering_decode_command(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint16_t stream_offset,
    TecmoGameplayCpuSteeringCommand *command_out)
{
    const TecmoGameplayCpuSteeringSourceSpan *source;
    TecmoGameplayCpuSteeringCommand command;
    uint32_t cpu_address;
    if (assets == NULL || !assets->available || command_out == NULL ||
        stream_offset % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE != 0U ||
        stream_offset >=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE ||
        TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE -
                stream_offset) {
        return false;
    }
    source = tecmo_gameplay_cpu_steering_find_source(
        assets, TECMO_GAMEPLAY_CPU_STEERING_SOURCE_PLAY_COMMANDS);
    if (source == NULL) return false;
    memset(&command, 0, sizeof(command));
    command.stream_offset = stream_offset;
    cpu_address = (uint32_t)assets->command_base_cpu + stream_offset;
    if (cpu_address > 0xFFFFU) return false;
    command.cpu_address = (uint16_t)cpu_address;
    command.opcode = source->bytes[stream_offset];
    if (command.opcode >= TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
        return false;
    }
    memcpy(command.arguments, source->bytes + stream_offset + 1U,
           sizeof(command.arguments));
    command.handler_cpu = assets->handler_cpu[command.opcode];
    command.kind = command_kind(command.opcode);
    command.effect = assets->effect_metadata[command.opcode].kind;
    *command_out = command;
    return true;
}

const TecmoGameplayCpuSteeringEffectMetadata *
tecmo_gameplay_cpu_steering_effect_metadata(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t opcode)
{
    if (assets == NULL || !assets->available ||
        opcode >= TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
        return NULL;
    }
    return &assets->effect_metadata[opcode];
}

const char *tecmo_gameplay_cpu_steering_effect_name(
    TecmoGameplayCpuSteeringEffectKind kind)
{
    switch (kind) {
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_RELATIVE_TARGET:
        return "relative-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GOTO:
        return "goto";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ABSOLUTE_TARGET:
        return "absolute-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_WAIT_COUNTDOWN:
        return "wait-countdown";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_TARGET:
        return "actor-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE:
        return "direction-pose";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_TRANSITION_RESET:
        return "transition-reset";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_ACTOR_STATE_BRANCH:
        return "actor-state-branch";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_BOUNDARY_BRANCH:
        return "boundary-branch";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_STATE_ANIMATION:
        return "state-animation";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_PROXIMITY:
        return "fixed-link-proximity";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_RELATIVE_POSE:
        return "fixed-link-relative-pose";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_FOLLOW_UP:
        return "fixed-link-follow-up";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_SCRATCH_TARGET:
        return "global-scratch-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GROUP_RESEED:
        return "group-reseed";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_PRIMARY_DEFENDER_SWITCH:
        return "primary-defender-switch";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_POINTER_ACTOR_TARGET:
        return "pointer-actor-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER:
        return "aggregation-barrier";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET:
        return "global-target";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_CONDITIONAL_ADVANCE:
        return "conditional-advance";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TIMERS:
        return "global-timers";
    case TECMO_GAMEPLAY_CPU_STEERING_EFFECT_DIRECTION_POSE_ALT:
        return "direction-pose-alt";
    default:
        return "unknown";
    }
}

const char *tecmo_gameplay_cpu_steering_opcode15_branch_name(
    TecmoGameplayCpuSteeringOpcode15Branch branch)
{
    switch (branch) {
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_GATE_NOOP:
        return "gate-noop";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW:
        return "deferred-missing-raw";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_BIT2_RETURN:
        return "primary-bit2-return-9179";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_REPLACED:
        return "primary-replaced";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_QUALIFIED_BIT3_RETURN:
        return "qualified-bit3-return-9179";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_INVALID_DIRECTION:
        return "deferred-invalid-direction";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED:
        return "defender-replaced";
    default:
        return "none";
    }
}

const char *tecmo_gameplay_cpu_steering_deferred_reason_name(
    TecmoGameplayCpuSteeringDeferredReason reason)
{
    switch (reason) {
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE:
        return "none";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_INVALID_TARGET_OBJECT:
        return "invalid-target-object";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS:
        return "unimplemented-handler";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF:
        return "missing-special-actor-07df";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT:
        return "missing-linked-actor-branch-context";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE:
        return "missing-linked-relative-workspace";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE:
        return "missing-pointer-workspace";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE:
        return "missing-actor-046e-probe";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA:
        return "missing-ba-lifecycle";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE21_GATE_INPUTS:
        return "missing-opcode21-gates";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE:
        return "missing-opcode15-raw-lifecycle";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_NATIVE_TARGET_OUTSIDE_COURT:
        return "native-target-outside-court";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET:
        return "missing-global-target";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE6_CONTEXT:
        return "missing-opcode6-context";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE6_CONTROLLED_BRANCH:
        return "opcode6-controlled-branch";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE23_CONTEXT:
        return "missing-opcode23-context";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE23_CONTROLLED_BRANCH:
        return "opcode23-controlled-branch";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE12_CONTEXT:
        return "missing-opcode12-context";
    case TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT:
        return "opcode12-unsafe-context";
    default:
        return "invalid-defer-reason";
    }
}

static bool play_stream_offset_valid(uint16_t offset)
{
    return offset < CPU_STEERING_COMMAND_STREAM_SIZE &&
           offset % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE == 0U;
}

static int16_t play_signed_u16(uint8_t low, uint8_t high)
{
    return (int16_t)((uint16_t)low | ((uint16_t)high << 8U));
}

static int16_t play_clamp_int16(int32_t value)
{
    if (value < INT16_MIN) return INT16_MIN;
    if (value > INT16_MAX) return INT16_MAX;
    return (int16_t)value;
}

static bool play_opcode5_direction(uint8_t source_direction,
                                   uint8_t orientation,
                                   uint8_t *direction_out)
{
    static const uint8_t mirror[8U] = {1U,0U,2U,4U,3U,5U,7U,6U};
    if (direction_out == NULL || source_direction >= 8U || orientation > 1U) {
        return false;
    }
    *direction_out = orientation != 0U
        ? mirror[source_direction] : source_direction;
    return true;
}

static uint16_t play_next_offset(uint16_t offset, uint16_t amount)
{
    uint32_t next = (uint32_t)offset + amount;
    return next < CPU_STEERING_COMMAND_STREAM_SIZE
        ? (uint16_t)next
        : offset;
}

static bool play_valid_actor(uint8_t actor)
{
    return actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
}

static bool play_valid_target_object(uint8_t object)
{
    return object < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
           object == TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
}

static bool play_valid_positions(
    const TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT])
{
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (!tecmo_gameplay_court_coordinate_valid(&positions[actor])) {
            return false;
        }
    }
    return true;
}

static TecmoGameplayCpuSteeringDeferredReason
play_missing_live_input_reason(
    const TecmoGameplayCpuSteeringCommand *command,
    const TecmoGameplayCpuSteeringPlayInput *input)
{
    /* Bank06 $8B90-$9237 dispatches handlers whose caller-owned RAM is not
       part of every native scene snapshot.  Check availability before the
       handler mutates its play state: a valid zero remains distinct from an
       unavailable byte/workspace. */
    switch (command->opcode) {
    case 6U:
        if (!input->opcode6_context_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE6_CONTEXT;
        }
        return input->opcode6_automatic
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE6_CONTROLLED_BRANCH;
    case 23U:
        if (!input->opcode23_context_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE23_CONTEXT;
        }
        return input->opcode23_uncontrolled
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE23_CONTROLLED_BRANCH;
    case 0U:
    case 2U:
        /* $92CA consumes $BA after these handlers. */
        return input->common_tail_ba_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    case 20U:
        /* `$9032-$9052` reads the same persistent raw target latch, but
           jumps directly to `$901A`; it never consumes the BA lifecycle. */
        return input->global_target_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET;
    case 13U:
        /* $9125 reads the persistent $038D-$0390 latch first, then reaches
           the same $92CA BA gate as the other target handlers. */
        if (!input->global_target_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET;
        }
        return input->common_tail_ba_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    case 7U:
        /* $8F11 probes $046E,C8 before selecting either stream branch. */
        return input->actor_046e_probe_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE;
    case 10U:
        /* $8CD0 compares X with the exceptional $07DF actor before it can
           enter the linked-relative helper at $8D59. */
        if (input->linked_actor_resolved_valid) {
            if (!play_valid_actor(input->linked_actor)) {
                return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT;
            }
        } else if (!input->special_actor_07df_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF;
        }
        if (!input->linked_actor_resolved_valid &&
            !input->linked_actor_branch_context_available) {
            return
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT;
        }
        if (!input->linked_relative_valid) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE;
        }
        return input->common_tail_ba_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    case 12U:
        if (!input->opcode12_context_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE12_CONTEXT;
        }
        if (!input->opcode12_automatic_offense ||
            !input->opcode12_actor_eligible ||
            !play_valid_actor(input->opcode12_linked_actor_06cb)) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT;
        }
        if (!input->linked_actor_resolved_valid ||
            !play_valid_actor(input->linked_actor)) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT;
        }
        if (!input->linked_relative_valid) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE;
        }
        if (!input->common_tail_ba_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
        }
        return (input->flags_ba & 0x03U) == 0U
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT;
    case 15U:
        /* $9172-$9216 owns a wider raw lifecycle than this LIVE contract. */
        return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE;
    case 16U:
        /* $9085 resolves $0309 through $036E/$0370; $92CA then needs $BA. */
        if (!input->pointer_workspace_valid) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE;
        }
        return input->common_tail_ba_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    case 21U:
        /* $8BF6-$8C17 branches on $058A/$0357/$0358/$007E as one gate. */
        return input->opcode21_gate_inputs_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE21_GATE_INPUTS;
    default:
        return TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    }
}

static int16_t play_wrap_add_i16(int16_t left, int16_t right)
{
    return (int16_t)((uint16_t)left + (uint16_t)right);
}

static bool play_proximity_axis(int16_t current, int16_t target)
{
    int16_t delta = (int16_t)((uint16_t)current - (uint16_t)target);
    return delta >= -8 && delta <= 7;
}

static void play_apply_opcode11_pose(
    TecmoGameplayCpuSteeringPlayState *state,
    TecmoGameplayCpuSteeringPlayResult *result,
    const TecmoGameplayCpuSteeringPlayInput *input,
    uint8_t actor, uint8_t linked)
{
    static const uint8_t pose_low[4U] = {0x0AU,0x0CU,0x0EU,0x10U};
    uint16_t actor_x = (uint16_t)input->actor_position[actor].x;
    uint16_t linked_x = (uint16_t)input->actor_position[linked].x;
    uint16_t actor_depth =
        (uint16_t)(uint8_t)input->actor_position[actor].y;
    uint16_t linked_depth =
        (uint16_t)(uint8_t)input->actor_position[linked].y;
    bool actor_left = actor_x < linked_x;
    bool actor_above = actor_depth < linked_depth;
    uint16_t horizontal = actor_left
        ? (uint16_t)(linked_x - actor_x)
        : (uint16_t)(actor_x - linked_x);
    uint16_t depth = actor_above
        ? (uint16_t)(linked_depth - actor_depth)
        : (uint16_t)(actor_depth - linked_depth);
    uint8_t pose_index = horizontal >= depth
        ? (actor_left ? 1U : 0U)
        : (actor_above ? 3U : 2U);
    state->pose[actor] = pose_low[pose_index];
    state->action[actor] = 0x30U;
    state->actor_state[actor] = 0x04U;
    result->raw_pose_valid = true;
    result->raw_pose_low_0442 = pose_low[pose_index];
    result->raw_pose_high_044d = 0x04U;
    result->raw_packed_action_0458 = 0x30U;
}

static TecmoGameplayCourtCoordinate play_target_position_for_object(
    const TecmoGameplayCpuSteeringPlayInput *input,
    uint8_t target_object)
{
    return target_object == TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT
        ? input->ball_position
        : input->actor_position[target_object];
}

static int16_t play_opcode4_x_delta(
    const TecmoGameplayCourtCoordinate *target,
    const TecmoGameplayCourtCoordinate *actor)
{
    /* $8FFD-$900A: SEC; target X low/high minus actor X low/high. Cast to
       uint16_t preserves the source borrow and wraps exactly before the
       signed result is observed by $88DA. */
    return (int16_t)((uint16_t)target->x - (uint16_t)actor->x);
}

static int16_t play_opcode4_depth_delta(
    const TecmoGameplayCourtCoordinate *target,
    const TecmoGameplayCourtCoordinate *actor)
{
    uint8_t target_depth = (uint8_t)target->y;
    uint8_t actor_depth = (uint8_t)actor->y;
    uint8_t low = (uint8_t)(target_depth - actor_depth);
    /* $900C-$9018: SEC depth subtraction, then LDA #0/SBC #0. This creates
       0x0000 or 0xFF00 above the low byte according to the borrow. */
    uint16_t high = target_depth < actor_depth ? 0xFF00U : 0x0000U;
    return (int16_t)(high | low);
}

static void play_set_target_from_object(
    TecmoGameplayCpuSteeringPlayState *state,
    const TecmoGameplayCpuSteeringPlayInput *input,
    uint8_t actor,
    uint8_t target_object)
{
    TecmoGameplayCourtCoordinate target = play_target_position_for_object(
        input, target_object);
    state->target_object[actor] = target_object;
    state->target_x[actor] = target.x;
    state->target_depth[actor] = target.y;
}

bool tecmo_gameplay_cpu_steering_formation_select(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t formation_index,
    TecmoGameplayCpuSteeringFormationResult *result_out)
{
    TecmoGameplayCpuSteeringFormationResult result;
    if (assets == NULL || !assets->available || result_out == NULL ||
        formation_index >= assets->formation_start_count ||
        assets->formation_source_pinned[formation_index] == 0U) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_FORMATION_RESULT_TAG;
    result.formation_index = formation_index;
    result.actor_count = TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    memcpy(result.stream_offset,
           assets->formation_stream_offsets[formation_index],
           sizeof(result.stream_offset));
    result.source_pinned = true;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_route_select(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringRouteInput *input,
    TecmoGameplayCpuSteeringRouteResult *result_out)
{
    static const uint8_t table_9709[2] = {0x00U,0x80U};
    TecmoGameplayCpuSteeringRouteResult result;
    bool use_long_route;
    if (assets == NULL || !assets->available || input == NULL ||
        result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_ROUTE_INPUT_TAG ||
        input->route_slot >=
            TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT ||
        !play_valid_actor(input->actor) ||
        input->table_index_035A >= sizeof(table_9709)) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_ROUTE_RESULT_TAG;
    result.route_slot = input->route_slot;
    result.actor = input->actor;
    if (input->control_flags[input->route_slot] == 0U) {
        *result_out = result;
        return true;
    }
    use_long_route = (uint8_t)(input->global_0373 & 0x80U) ==
        table_9709[input->table_index_035A] ||
        input->flag_0095 != 0U || input->age_0094 >= 0x28U;
    result.stream_offset = use_long_route
        ? CPU_STEERING_ROUTE_LONG_OFFSET
        : CPU_STEERING_ROUTE_SHORT_OFFSET;
    result.actor_state = 0x04U;
    result.wrote_route = true;
    result.used_long_route = use_long_route;
    *result_out = result;
    return true;
}

static bool route_motion_state_valid(
    const TecmoGameplayCpuSteeringRouteMotionState *state)
{
    return state != NULL &&
           state->contract_tag ==
               TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
}

static bool route_launch_position_valid(
    const TecmoGameplayCourtCoordinate *position)
{
    /* Bank06 seeds the horizontal Q6 accumulator from the packed 10-bit
       $0073/$00E8 position and depth from the full $00F3 byte. This route
       has no TGCT/TGMO court clamp. */
    return position != NULL && position->x >= 0 && position->x <= 0x03FF &&
           position->y >= 0 && position->y <= 0x00FF;
}

/* Exact unsigned-divisor/signed-dividend behavior of Bank06 $9BD8-$9C6E.
   The route caller uses a positive divisor in ordinary court-reachable
   cases, but retaining the bytewise normalization keeps synthetic vectors
   deterministic without relying on implementation-defined signed overflow. */
static uint16_t route_divide_9bd8(uint16_t divisor, uint16_t dividend_bits)
{
    uint16_t dividend;
    uint16_t quotient = 0U;
    uint8_t divisor_shifts = 0U;
    uint8_t quotient_bits = 0U;
    bool negative = (dividend_bits & 0x8000U) != 0U;
    if (divisor == 0U) return 0U;
    while ((divisor & 0xFF00U) != 0U) {
        divisor >>= 1U;
        ++divisor_shifts;
    }
    dividend = negative
        ? (uint16_t)(0U - dividend_bits)
        : dividend_bits;
    if (dividend < divisor) return 0U;
    for (;;) {
        uint16_t prior = divisor;
        ++quotient_bits;
        divisor = (uint16_t)(divisor << 1U);
        if ((prior & 0x8000U) != 0U || dividend < divisor) {
            divisor = prior;
            break;
        }
    }
    while (quotient_bits != 0U) {
        bool subtract = dividend >= divisor;
        if (subtract) dividend = (uint16_t)(dividend - divisor);
        quotient = (uint16_t)((quotient << 1U) | (subtract ? 1U : 0U));
        divisor >>= 1U;
        --quotient_bits;
    }
    quotient >>= divisor_shifts;
    return negative ? (uint16_t)(0U - quotient) : quotient;
}

bool tecmo_gameplay_cpu_steering_route_launch(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringRouteLaunchInput *input,
    TecmoGameplayCpuSteeringRouteLaunchResult *result_out)
{
    TecmoGameplayCpuSteeringRouteLaunchResult result;
    uint16_t horizontal_magnitude;
    uint16_t depth_magnitude;
    uint16_t major;
    uint16_t minor;
    uint16_t approximate_magnitude;
    uint16_t duration;
    uint8_t movement_amount;
    uint8_t divisor;
    uint8_t direction;
    if (assets == NULL || !assets->available || input == NULL ||
        result_out == NULL || (const void *)input == (const void *)result_out ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_INPUT_TAG ||
        !route_launch_position_valid(&input->actor_position)) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_RESULT_TAG;
    result.motion.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    result.direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    if (input->horizontal_delta == 0 && input->depth_delta == 0) {
        *result_out = result;
        return true;
    }
    if (!tecmo_gameplay_cpu_steering_direction_for_delta(
            assets, input->horizontal_delta, input->depth_delta,
            &direction)) {
        return false;
    }

    /* $88DA-$8930 forms max(abs(dx),abs(dy)) + min(...)/2 before calling
       $8A96. Every operation is wrapping 16-bit 6502 arithmetic. */
    horizontal_magnitude = input->horizontal_delta < 0
        ? (uint16_t)(0U - (uint16_t)input->horizontal_delta)
        : (uint16_t)input->horizontal_delta;
    depth_magnitude = input->depth_delta < 0
        ? (uint16_t)(0U - (uint16_t)input->depth_delta)
        : (uint16_t)input->depth_delta;
    major = horizontal_magnitude >= depth_magnitude
        ? horizontal_magnitude : depth_magnitude;
    minor = horizontal_magnitude >= depth_magnitude
        ? depth_magnitude : horizontal_magnitude;
    approximate_magnitude = (uint16_t)(major + (minor >> 1U));

    /* $8AB2-$8AD3 derives max(8, $06E7 + high_nibble($7C48) - 6)
       and then adds three for the projection divisor. */
    movement_amount = (uint8_t)(
        input->movement_value_06e7 + (input->condition_7c48 >> 4U) - 6U);
    if (movement_amount < 8U) movement_amount = 8U;
    divisor = (uint8_t)(movement_amount + 3U);
    duration = (uint16_t)(route_divide_9bd8(
        divisor, (uint16_t)(approximate_magnitude << 4U)) + 1U);

    result.motion.horizontal_accumulator_q6 =
        (uint16_t)((uint16_t)input->actor_position.x << 6U);
    result.motion.depth_accumulator_q6 =
        (uint16_t)((uint16_t)input->actor_position.y << 6U);
    result.motion.horizontal_velocity_q6 = (int16_t)route_divide_9bd8(
        duration, (uint16_t)((uint16_t)input->horizontal_delta << 6U));
    result.motion.depth_velocity_q6 = (int16_t)route_divide_9bd8(
        duration, (uint16_t)((uint16_t)input->depth_delta << 6U));
    result.motion.remaining_timer = duration;
    result.motion.active = true;
    result.direction = direction;
    result.duration = duration;
    result.launched = true;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_route_step(
    uint8_t actor,
    uint8_t completion_side_bit_0359,
    const TecmoGameplayCpuSteeringRouteMotionState *state_in,
    TecmoGameplayCpuSteeringRouteMotionState *state_out,
    TecmoGameplayCpuSteeringRouteStepResult *result_out)
{
    TecmoGameplayCpuSteeringRouteMotionState next;
    TecmoGameplayCpuSteeringRouteStepResult result;
    if (actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        completion_side_bit_0359 > 1U ||
        !route_motion_state_valid(state_in) || !state_in->active ||
        state_out == NULL || result_out == NULL || state_out == state_in ||
        (const void *)state_out == (const void *)result_out ||
        (const void *)result_out == (const void *)state_in) {
        return false;
    }
    next = *state_in;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_ROUTE_STEP_RESULT_TAG;
    result.actor = actor;
    result.timer_before = next.remaining_timer;

    /* Bank06 $8AF4-$8B55 integrates before testing the route timer. */
    next.horizontal_accumulator_q6 = (uint16_t)(
        next.horizontal_accumulator_q6 +
        (uint16_t)next.horizontal_velocity_q6);
    next.depth_accumulator_q6 = (uint16_t)(
        next.depth_accumulator_q6 + (uint16_t)next.depth_velocity_q6);
    result.horizontal_position =
        (uint16_t)(next.horizontal_accumulator_q6 >> 6U);
    result.depth_position =
        (uint8_t)(next.depth_accumulator_q6 >> 6U);

    if (next.remaining_timer == 0U) {
        next.active = false;
    } else {
        --next.remaining_timer;
        if (next.remaining_timer == 0U) {
            /* $8B75-$8B8F completes the low half only for side bit one and
               the high half only for side bit zero. The opposite half keeps
               state 5 for one extra integration with a zero timer. */
            bool actor_low_half = actor <
                TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT;
            if ((actor_low_half && completion_side_bit_0359 != 0U) ||
                (!actor_low_half && completion_side_bit_0359 == 0U)) {
                next.active = false;
            }
        }
    }
    result.timer_after = next.remaining_timer;
    result.finished = !next.active;
    *state_out = next;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_play_state_initialize(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t formation_index,
    TecmoGameplayCpuSteeringPlayState *state_out)
{
    TecmoGameplayCpuSteeringPlayState state;
    if (assets == NULL || !assets->available || state_out == NULL ||
        formation_index >= assets->formation_start_count ||
        assets->formation_source_pinned[formation_index] == 0U) {
        return false;
    }
    memset(&state, 0, sizeof(state));
    state.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_STATE_TAG;
    memcpy(state.stream_offset,
           assets->formation_stream_offsets[formation_index],
           sizeof(state.stream_offset));
    memcpy(state.fixed_link, assets->fixed_link,
           sizeof(state.fixed_link));
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        state.actor_state[actor] = 0x04U;
        state.direction[actor] = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        state.target_object[actor] = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        state.fixed_link_target[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        state.route_motion[actor].contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    }
    /* Bank04 $ACE4-$ACF0 and $AD11/$AD16 startup seeds. */
    state.primary_actor = 4U;
    state.defender_actor = 9U;
    state.matchup_seed[0U] = 2U;
    state.matchup_seed[1U] = 7U;
    state.candidate_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    *state_out = state;
    return true;
}

static bool opcode15_record_is_canonical(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint16_t stream_offset)
{
    TecmoGameplayCpuSteeringCommand command;
    if (stream_offset !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET &&
        stream_offset !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET ||
        !tecmo_gameplay_cpu_steering_decode_command(
            assets, stream_offset, &command)) {
        return false;
    }
    return command.opcode == 15U && command.handler_cpu == 0x9172U &&
           command.arguments[0U] == 0U && command.arguments[1U] == 0U &&
           command.arguments[2U] == 0U && command.arguments[3U] == 0U;
}

static bool opcode15_raw_input_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringOpcode15RawInput *input)
{
    uint32_t observed;
    if (assets == NULL || !assets->available || input == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_INPUT_TAG ||
        !play_valid_actor(input->actor_x) ||
        !opcode15_record_is_canonical(assets, input->command_record_offset)) {
        return false;
    }
    observed = input->observed_mask;
    if ((observed &
         ~TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK) != 0U ||
        ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_0308) !=
             0U &&
         !play_valid_actor(input->raw_0308_primary)) ||
        ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENDER_0309) !=
             0U &&
         !play_valid_actor(input->raw_0309_defender)) ||
        ((observed &
          TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_OFFENSE_SIDE_030A) != 0U &&
         input->raw_030a_offense_side >=
             TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) ||
        ((observed &
          TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENSE_SIDE_030B) != 0U &&
         input->raw_030b_defense_side >=
             TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) ||
        ((observed &
          TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT) != 0U &&
         (input->formation_output.contract_tag !=
              TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_FORMATION_TAG ||
          (input->formation_output.assigned_actor_mask & ~0x03FFU) != 0U ||
          (input->formation_output.assigned_actor_mask &
           (uint16_t)(1U << input->actor_x)) != 0U ||
          input->formation_output.raw_06df_after != 0U ||
          input->formation_output.raw_06e1_after != 0U))) {
        return false;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT) != 0U) {
        for (uint8_t actor = 0U;
             actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
             ++actor) {
            if ((input->formation_output.assigned_actor_mask &
                 (uint16_t)(1U << actor)) != 0U &&
                input->formation_output.actor_state[actor] != 4U) {
                return false;
            }
        }
    }
    return true;
}

static void opcode15_result_capture_before(
    const TecmoGameplayCpuSteeringOpcode15RawInput *input,
    TecmoGameplayCpuSteeringOpcode15RawResult *result)
{
    uint32_t observed = input->observed_mask;
    result->observed_mask = observed;
    result->missing_raw_mask =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK & ~observed;
    result->command_record_offset = input->command_record_offset;
    result->opcode = 15U;
    result->actor_x = input->actor_x;
    result->raw_0499_slot10 = input->raw_0499_slot10;
    result->raw_04b0_actor_x = input->raw_04b0_actor_x;
    result->raw_007e = input->raw_007e;
    result->raw_0308_before = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    result->raw_0308_after = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    result->raw_0309_before = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    result->raw_0309_after = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    result->new_actor_state_before = 0U;
    result->new_actor_state_after = 0U;
    if ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_0308) !=
        0U) {
        result->raw_0308_before = input->raw_0308_primary;
        result->raw_0308_after = input->raw_0308_primary;
    }
    if ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_DEFENDER_0309) !=
        0U) {
        result->raw_0309_before = input->raw_0309_defender;
        result->raw_0309_after = input->raw_0309_defender;
        if ((observed &
             TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_LIFECYCLE) !=
            0U) {
            result->defender_stream_before =
                input->actor[input->raw_0309_defender]
                    .raw_0547_0551_stream_offset;
            result->defender_stream_after =
                result->defender_stream_before;
            result->defender_state_before =
                input->actor[input->raw_0309_defender].raw_057c_state;
            result->defender_state_after = result->defender_state_before;
        }
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D5) != 0U) {
        result->raw_06d5_before = input->raw_06d5;
        result->raw_06d5_after = input->raw_06d5;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_06D6) != 0U) {
        result->raw_06d6_before = input->raw_06d6;
        result->raw_06d6_after = input->raw_06d6;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E) != 0U) {
        result->raw_059e_before = input->raw_059e;
        result->raw_059e_after = input->raw_059e;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_LINKS) != 0U &&
        input->raw_030a_offense_side <
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) {
        result->raw_037f_before = input->raw_037f_0380_primary_link[
            input->raw_030a_offense_side];
        result->raw_037f_after = result->raw_037f_before;
        result->raw_06da_before = input->raw_06da;
        result->raw_06da_after = input->raw_06da;
        result->raw_06db_before = input->raw_06db;
        result->raw_06db_after = input->raw_06db;
        result->raw_058b_before = input->raw_058b;
        result->raw_058c_before = input->raw_058c;
        result->raw_058b_after = input->raw_058b;
        result->raw_058c_after = input->raw_058c;
        result->raw_06df_after = input->raw_06df;
        result->raw_06e1_after = input->raw_06e1;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_LIFECYCLE) != 0U) {
        result->new_actor_state_before =
            input->actor[input->actor_x].raw_057c_state;
        result->new_actor_state_after = result->new_actor_state_before;
    }
}

bool tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringOpcode15RawInput *input,
    TecmoGameplayCpuSteeringOpcode15RawInput *output,
    TecmoGameplayCpuSteeringOpcode15RawResult *result_out)
{
    TecmoGameplayCpuSteeringOpcode15RawInput candidate;
    TecmoGameplayCpuSteeringOpcode15RawResult result;
    uint32_t observed;
    uint8_t old_primary;
    uint8_t old_defender;
    uint8_t direction;
    if (input == NULL || output == NULL || result_out == NULL ||
        (const void *)output == (const void *)input ||
        (const void *)result_out == (const void *)input ||
        (const void *)result_out == (const void *)output ||
        !opcode15_raw_input_valid(assets, input)) {
        return false;
    }
    candidate = *input;
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_RESULT_TAG;
    opcode15_result_capture_before(input, &result);
    observed = input->observed_mask;

    /* $9172-$9179: the altitude/object-plane gate returns before every
       other handler input. A below-$46 observation is therefore a complete
       source no-op even if later raw owners were not captured. */
    if ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499) ==
        0U) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        *output = candidate;
        *result_out = result;
        return true;
    }
    if (input->raw_0499_slot10 < 0x46U) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_GATE_NOOP;
        result.returned_9179_without_advance = true;
        *output = candidate;
        *result_out = result;
        return true;
    }
    if ((observed &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_ACTOR_04B0) == 0U) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        *output = candidate;
        *result_out = result;
        return true;
    }
    if ((input->raw_04b0_actor_x & 0x10U) == 0U) {
        if ((observed &
             TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FLAGS_007E) == 0U) {
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        } else if ((input->raw_007e & 0x04U) != 0U) {
            /* Canonical `$9185 D0 F2` targets `$9179 RTS`; it does not retry
               the altitude gate and does not reach the stream advance. */
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_BIT2_RETURN;
            result.returned_9179_without_advance = true;
        } else if ((observed & CPU_STEERING_OPCODE15_PRIMARY_REQUIRED_RAW) !=
                   CPU_STEERING_OPCODE15_PRIMARY_REQUIRED_RAW) {
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        } else {
            old_primary = input->raw_0308_primary;
            direction = input->actor[old_primary].raw_0463_direction;
            if (direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
                result.branch = TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_INVALID_DIRECTION;
                *output = candidate;
                *result_out = result;
                return true;
            }

            /* `$9187-$91C1`: the source first replaces `$0308`, resets the
               old primary through `$88B0`, publishes the old-primary links,
               invalidates `$058B/$058C` through `$C060`, then consumes the
               explicitly captured `$943B->$938B` reassignment outputs. */
            candidate.raw_0308_primary = input->actor_x;
            candidate.actor[old_primary].raw_0442_pose_low =
                assets->opcode15_pose_low_0442[direction];
            candidate.actor[old_primary].raw_044d_pose_high =
                assets->opcode15_pose_high_044d[direction];
            candidate.actor[old_primary].raw_0479_sprite_flags = 0xC1U;
            candidate.actor[old_primary].raw_0458_action = 0x30U;
            candidate.raw_037f_0380_primary_link[
                input->raw_030a_offense_side] = old_primary;
            candidate.raw_06da = old_primary;
            candidate.raw_06db = 9U;
            result.c060_invalidated_058b_058c_to_ff = true;
            result.raw_058b_after_c060 = 0xFFU;
            result.raw_058c_after_c060 = 0xFFU;
            candidate.raw_058b = 0xFFU;
            candidate.raw_058c = 0xFFU;
            candidate.raw_058b = input->formation_output.raw_058b_after;
            candidate.raw_058c = input->formation_output.raw_058c_after;
            candidate.raw_06df = input->formation_output.raw_06df_after;
            candidate.raw_06e1 = input->formation_output.raw_06e1_after;
            for (uint8_t actor = 0U;
                 actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
                 ++actor) {
                if ((input->formation_output.assigned_actor_mask &
                     (uint16_t)(1U << actor)) != 0U) {
                    candidate.actor[actor].raw_0547_0551_stream_offset =
                        input->formation_output.actor_stream_offset[actor];
                    candidate.actor[actor].raw_057c_state =
                        input->formation_output.actor_state[actor];
                }
            }
            /* `$9208` is an observable intermediate handoff. The following
               primary-only tail immediately overwrites state 7 with zero. */
            candidate.actor[input->actor_x].raw_057c_state = 7U;
            candidate.raw_059e = input->actor_x;
            result.primary_state7_handoff_observed = true;
            result.c711_selector = 4U;
            result.c711_x_actor = input->actor_x;
            result.c711_y_actor = input->actor_x;
            result.c711_selector_observed_unexecuted = true;
            candidate.actor[input->actor_x].raw_046e_timer = 0x1BU;
            candidate.actor[input->actor_x].raw_057c_state = 0U;
            candidate.raw_000e_000f_selected_actor[
                input->raw_030a_offense_side] = input->actor_x;

            result.raw_0308_after = candidate.raw_0308_primary;
            result.new_actor_state_after =
                candidate.actor[input->actor_x].raw_057c_state;
            result.raw_059e_after = candidate.raw_059e;
            result.raw_037f_after = candidate.raw_037f_0380_primary_link[
                input->raw_030a_offense_side];
            result.raw_06da_after = candidate.raw_06da;
            result.raw_06db_after = candidate.raw_06db;
            result.raw_058b_after = candidate.raw_058b;
            result.raw_058c_after = candidate.raw_058c;
            result.raw_06df_after = candidate.raw_06df;
            result.raw_06e1_after = candidate.raw_06e1;
            result.formation_assigned_actor_mask =
                input->formation_output.assigned_actor_mask;
            result.committed = true;
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_REPLACED;
        }
        *output = candidate;
        *result_out = result;
        return true;
    }
    if ((observed & TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FLAGS_007E) ==
        0U) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        *output = candidate;
        *result_out = result;
        return true;
    }
    if ((input->raw_007e & 0x08U) != 0U) {
        /* Canonical `$91C6 D0 B1` targets `$9179 RTS`; it does not branch to
           opcode 14's `$9146` mark-other loop or advance the stream. */
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_QUALIFIED_BIT3_RETURN;
        result.returned_9179_without_advance = true;
        *output = candidate;
        *result_out = result;
        return true;
    }
    if ((observed & CPU_STEERING_OPCODE15_DEFENDER_REQUIRED_RAW) !=
        CPU_STEERING_OPCODE15_DEFENDER_REQUIRED_RAW) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        *output = candidate;
        *result_out = result;
        return true;
    }

    old_defender = input->raw_0309_defender;
    direction = input->actor[old_defender].raw_0463_direction;
    if (direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_INVALID_DIRECTION;
        *output = candidate;
        *result_out = result;
        return true;
    }

    /* Exact $91C8-$9216 selected-defender branch. $88B0 acts on the old
       defender after Y->$X; the final $0479/$057C/$059E writes act on the
       new X. Bank07 $C711 itself remains observed, not executed. */
    candidate.raw_0309_defender = input->actor_x;
    candidate.actor[old_defender].raw_057c_state = 0x04U;
    candidate.actor[old_defender].raw_0547_0551_stream_offset = 0x005AU;
    candidate.actor[old_defender].raw_046e_timer = 0U;
    candidate.actor[old_defender].raw_0442_pose_low =
        assets->opcode15_pose_low_0442[direction];
    candidate.actor[old_defender].raw_044d_pose_high =
        assets->opcode15_pose_high_044d[direction];
    candidate.actor[old_defender].raw_0479_sprite_flags = 0xC1U;
    candidate.actor[old_defender].raw_0458_action = 0x30U;
    candidate.actor[input->actor_x].raw_0479_sprite_flags = 0x81U;
    if (input->actor_x == input->raw_06d5) {
        candidate.raw_06d5 = old_defender;
    }
    candidate.raw_06d6 = 0x09U;
    candidate.raw_000e_000f_selected_actor[
        input->raw_030b_defense_side] = input->actor_x;
    candidate.actor[input->actor_x].raw_057c_state = 0x07U;
    candidate.raw_059e = input->actor_x;

    result.raw_0309_after = candidate.raw_0309_defender;
    result.defender_stream_after =
        candidate.actor[old_defender].raw_0547_0551_stream_offset;
    result.defender_state_after = candidate.actor[old_defender].raw_057c_state;
    result.new_actor_state_after =
        candidate.actor[input->actor_x].raw_057c_state;
    result.raw_06d5_after = candidate.raw_06d5;
    result.raw_06d6_after = candidate.raw_06d6;
    result.raw_059e_after = candidate.raw_059e;
    result.c711_selector = 0x04U;
    result.c711_x_actor = input->actor_x;
    result.c711_y_actor = input->actor_x;
    result.c711_selector_observed_unexecuted = true;
    result.committed = true;
    result.branch =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED;
    *output = candidate;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_play_step(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringPlayState *state_in,
    const TecmoGameplayCpuSteeringPlayInput *input,
    TecmoGameplayCpuSteeringPlayState *state_out,
    TecmoGameplayCpuSteeringPlayResult *result_out)
{
    TecmoGameplayCpuSteeringPlayState next_state;
    TecmoGameplayCpuSteeringPlayResult result;
    static const uint8_t aggregation_bitmask[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {
        0x01U,0x02U,0x04U,0x08U,0x10U,
        0x01U,0x02U,0x04U,0x08U,0x10U
    };
    uint8_t actor;
    bool goto_chain_active = true;
    if (assets == NULL || !assets->available || state_in == NULL ||
        input == NULL || state_out == NULL || result_out == NULL ||
        state_out == state_in ||
        (const void *)state_out == (const void *)input ||
        (const void *)result_out == (const void *)state_in ||
        (const void *)result_out == (const void *)input ||
        (const void *)state_out == (const void *)result_out ||
        state_in->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_PLAY_STATE_TAG ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG ||
        !play_valid_actor(input->actor) || input->step_budget == 0U ||
        input->step_budget > TECMO_GAMEPLAY_CPU_STEERING_PLAY_STEP_BUDGET ||
        input->orientation_035a > 1U ||
        (input->opcode12_context_available &&
         !play_valid_actor(input->opcode12_linked_actor_06cb)) ||
        (!input->special_actor_07df_available &&
         input->special_actor_07df != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) ||
        (input->special_actor_07df_available &&
         input->special_actor_07df != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
         !play_valid_actor(input->special_actor_07df)) ||
        !play_valid_positions(input->actor_position) ||
        !tecmo_gameplay_court_coordinate_valid(&input->ball_position) ||
        state_in->primary_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        state_in->defender_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        state_in->matchup_seed[0U] != 2U ||
        state_in->matchup_seed[1U] != 7U ||
        memcmp(state_in->fixed_link, cpu_steering_fixed_link,
               sizeof(state_in->fixed_link)) != 0) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (!play_stream_offset_valid(state_in->stream_offset[actor]) ||
            (state_in->target_object[actor] !=
                 TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
             !play_valid_target_object(state_in->target_object[actor])) ||
            (state_in->fixed_link_target[actor] !=
                 TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
             !play_valid_actor(state_in->fixed_link_target[actor]))) {
            return false;
        }
        if (!route_motion_state_valid(&state_in->route_motion[actor])) {
            return false;
        }
    }

    next_state = *state_in;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_RESULT_TAG;
    result.actor = input->actor;
    result.step_budget = input->step_budget;
    actor = input->actor;

    while (result.steps_executed < input->step_budget) {
        TecmoGameplayCpuSteeringCommand command;
        TecmoGameplayCpuSteeringDeferredReason missing_live_input;
        uint16_t current_offset = next_state.stream_offset[actor];
        uint16_t following_offset = current_offset;
        bool command_advanced = false;
        /* Bank06 state index 6 dispatches through $82B6/$82C4 to
           $9053-$905D.  That handler always performs the wrapping byte DEC
           on $04E7,X: an entry value of zero becomes $FF and remains in state
           6, while only a decrement result of zero returns $057C,X to state
           4.  The state owns this lifecycle; a stale nonzero $04E7 byte in a
           different actor state must not suppress that state's dispatch. */
        if (next_state.actor_state[actor] == 0x06U) {
            next_state.wait_counter[actor] =
                (uint8_t)(next_state.wait_counter[actor] - 1U);
            if (next_state.wait_counter[actor] == 0U) {
                next_state.actor_state[actor] = 0x04U;
            }
            result.steps_executed = 1U;
            result.waiting = next_state.wait_counter[actor] != 0U;
            result.previous_offset = current_offset;
            result.next_offset = current_offset;
            goto_chain_active = false;
            break;
        }
        if (!tecmo_gameplay_cpu_steering_decode_command(
                assets, current_offset, &command)) {
            return false;
        }
        result.command = command;
        result.effect = command.effect;
        result.previous_offset = current_offset;
        result.fetched = true;
        ++result.steps_executed;
        missing_live_input = play_missing_live_input_reason(&command, input);
        if (missing_live_input != TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE) {
            /* Do not let an absent RAM plane impersonate a zero-valued source
               input.  Bank06 dispatch has fetched the record, but LIVE leaves
               both stream/lifecycle state intact until an owner exists. */
            result.deferred = true;
            result.deferred_reason = missing_live_input;
            result.next_offset = current_offset;
            goto_chain_active = false;
            break;
        }
        ++next_state.step_serial;

        switch (command.opcode) {
        case 0U: {
            int16_t horizontal = play_signed_u16(
                command.arguments[0U], command.arguments[1U]);
            if (input->orientation_035a != 0U) {
                horizontal = (int16_t)(uint16_t)(0U -
                    (uint16_t)horizontal);
            }
            next_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            next_state.target_x[actor] = play_clamp_int16(
                (int32_t)input->actor_position[actor].x +
                horizontal);
            next_state.target_depth[actor] = play_clamp_int16(
                (int32_t)input->actor_position[actor].y +
                play_signed_u16(command.arguments[2U],
                                command.arguments[3U]));
            break;
        }
        case 1U:
            following_offset = (uint16_t)command.arguments[0U] |
                ((uint16_t)command.arguments[1U] << 8U);
            if (!play_stream_offset_valid(following_offset)) {
                return false;
            }
            result.jump_offset = following_offset;
            result.jumped = true;
            next_state.stream_offset[actor] = following_offset;
            result.next_offset = following_offset;
            continue;
        case 2U: {
            uint16_t absolute_x = (uint16_t)command.arguments[0U] |
                ((uint16_t)command.arguments[1U] << 8U);
            if (input->orientation_035a != 0U) {
                absolute_x = (uint16_t)(0x0300U - absolute_x);
            }
            next_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            next_state.target_x[actor] =
                (int16_t)absolute_x;
            next_state.target_depth[actor] =
                play_signed_u16(command.arguments[2U], command.arguments[3U]);
            break;
        }
        case 3U:
            next_state.wait_counter[actor] = command.arguments[0U];
            next_state.actor_state[actor] =
                command.arguments[0U] != 0U ? 0x06U : 0x04U;
            break;
        case 4U:
            if (play_valid_target_object(command.arguments[0U])) {
                TecmoGameplayCourtCoordinate target =
                    play_target_position_for_object(
                        input, command.arguments[0U]);
                play_set_target_from_object(
                    &next_state, input, actor, command.arguments[0U]);
                result.target_horizontal_delta = play_opcode4_x_delta(
                    &target, &input->actor_position[actor]);
                result.target_depth_delta = play_opcode4_depth_delta(
                    &target, &input->actor_position[actor]);
                /* Canonical Bank06 $8FFA-$9031 ORs the full X and
                   sign-extended depth vectors; its zero-vector branch only
                   when every byte is zero. */
                result.target_vector_zero =
                    result.target_horizontal_delta == 0 &&
                    result.target_depth_delta == 0;
                if (result.target_vector_zero) {
                    /* $902A writes state 4 before it takes the no-direction
                       tail. The direction field itself is intentionally
                       untouched. */
                    next_state.actor_state[actor] = 0x04U;
                }
            } else {
                /* Malformed source data outside the bounded 0..10 object
                   contract preserves the source record transport but does
                   not create an unchecked RAM lookup. */
                result.deferred = true;
                result.deferred_reason =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_INVALID_TARGET_OBJECT;
            }
            break;
        case 11U: {
            uint8_t linked = next_state.fixed_link[actor];
            play_apply_opcode11_pose(
                &next_state, &result, input, actor, linked);
            /* `$0479=$C1` is deliberately not retained: this bounded state
               has no faithful sprite-plane owner. */
            break;
        }
        case 8U: {
            uint16_t redirect_offset = (uint16_t)command.arguments[0U] |
                ((uint16_t)command.arguments[1U] << 8U);
            uint16_t ball_x = (uint16_t)input->ball_position.x;
            bool redirect = input->orientation_035a == 0U
                ? ball_x < 0x0140U
                : ball_x >= 0x01C0U;
            next_state.actor_state[actor] = 0x04U;
            /* `$8ED7-$8F11` also clears `$0588&7`; that global observation
               is deliberately unretained by the actor-local play state. */
            if (redirect) {
                if (!play_stream_offset_valid(redirect_offset)) return false;
                following_offset = redirect_offset;
                result.jump_offset = redirect_offset;
                result.jumped = true;
            } else {
                following_offset = play_next_offset(current_offset, 5U);
            }
            break;
        }
        case 5U: {
            uint8_t direction;
            if (!play_opcode5_direction(
                    command.arguments[0U], input->orientation_035a,
                    &direction)) {
                return false;
            }
            /* `$8F92-$8FBC` also copies an external pose source into `$0458`;
               that observation is not substituted into the typed play state. */
            next_state.direction[actor] = direction;
            next_state.actor_state[actor] = 0x04U;
            next_state.action_state_046e[actor] = 0x18U;
            break;
        }
        case 6U:
            /* Automatic branch `$8F2D-$8F4C`: `$0743=0` and `$0588^=1`
               remain unretained observations. The bounded writes are
               object-slot-10 `$0478=$13` and current actor `$046E=$10`.
               Source returns without advancing or changing actor state. */
            next_state.action_state_046e[actor] = 0x10U;
            result.opcode6_action10_written = true;
            result.opcode6_object10_state_written = true;
            result.opcode6_object10_state = 0x13U;
            break;
        case 23U:
            /* `$8F72-$8F91` selected/uncontrolled ownership is zero, so the
               handler reaches `$8FD9` without reading `$6A`, defender depth,
               or writing direction. */
            break;
        case 13U: {
            uint16_t horizontal_delta = (uint16_t)(
                input->global_target.x -
                (uint16_t)input->actor_position[actor].x);
            uint16_t depth_delta = (uint16_t)(
                input->global_target.depth -
                (uint16_t)(uint8_t)input->actor_position[actor].y);
            uint8_t direction;
            next_state.target_object[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
            next_state.target_x[actor] = (int16_t)input->global_target.x;
            next_state.target_depth[actor] =
                (int16_t)input->global_target.depth;
            result.raw_target_valid = true;
            result.raw_target_x = input->global_target.x;
            result.raw_target_depth = input->global_target.depth;
            result.target_horizontal_delta = (int16_t)horizontal_delta;
            result.target_depth_delta = (int16_t)depth_delta;
            result.target_vector_zero =
                result.target_horizontal_delta == 0 &&
                result.target_depth_delta == 0;
            if (!result.target_vector_zero) {
                if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                        assets, result.target_horizontal_delta,
                        result.target_depth_delta, &direction)) {
                    return false;
                }
                next_state.direction[actor] = direction;
            }
            break;
        }
        case 20U: {
            uint16_t horizontal_delta = (uint16_t)(
                input->global_target.x -
                (uint16_t)input->actor_position[actor].x);
            uint16_t depth_delta = (uint16_t)(
                input->global_target.depth -
                (uint16_t)(uint8_t)input->actor_position[actor].y);
            uint8_t direction;
            /* `$9032-$9052` exposes the raw subtraction to `$901A` but does
               not write the actor target-coordinate/object planes. */
            result.raw_target_valid = true;
            result.raw_target_x = input->global_target.x;
            result.raw_target_depth = input->global_target.depth;
            result.target_horizontal_delta = (int16_t)horizontal_delta;
            result.target_depth_delta = (int16_t)depth_delta;
            result.target_vector_zero =
                result.target_horizontal_delta == 0 &&
                result.target_depth_delta == 0;
            if (result.target_vector_zero) {
                next_state.actor_state[actor] = 0x04U;
            } else {
                if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                        assets, result.target_horizontal_delta,
                        result.target_depth_delta, &direction)) {
                    return false;
                }
                next_state.direction[actor] = direction;
            }
            break;
        }
        case 10U: {
            uint8_t linked = input->linked_actor_resolved_valid
                ? input->linked_actor
                : (actor == input->special_actor_07df
                    ? next_state.primary_actor
                    : next_state.fixed_link_target[actor]);
            int16_t target_x;
            int16_t target_depth;
            if (!input->linked_relative_valid || !play_valid_actor(linked)) {
                result.deferred = true;
                result.deferred_reason =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE;
                break;
            }
            target_x = play_wrap_add_i16(
                input->actor_position[linked].x,
                input->linked_relative_x);
            target_depth = play_wrap_add_i16(
                input->actor_position[linked].y,
                input->linked_relative_depth);
            next_state.target_object[actor] = linked;
            next_state.target_x[actor] = target_x;
            next_state.target_depth[actor] = target_depth;
            result.proximity_met = actor != next_state.defender_actor &&
                play_proximity_axis(input->actor_position[actor].x, target_x) &&
                play_proximity_axis(input->actor_position[actor].y, target_depth);
            if (result.proximity_met) {
                following_offset = play_next_offset(current_offset, 5U);
            }
            break;
        }
        case 12U: {
            uint8_t linked = input->linked_actor;
            uint8_t pose_linked = input->opcode12_linked_actor_06cb;
            int16_t target_x;
            int16_t target_depth;
            bool close;
            bool stalled;
            if (next_state.actor_state[actor] != 0x04U ||
                actor == next_state.defender_actor) {
                result.deferred = true;
                result.deferred_reason =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT;
                break;
            }
            target_x = play_wrap_add_i16(
                input->actor_position[linked].x,
                input->linked_relative_x);
            target_depth = play_wrap_add_i16(
                input->actor_position[linked].y,
                input->linked_relative_depth);
            close = play_proximity_axis(
                        input->actor_position[actor].x, target_x) &&
                    play_proximity_axis(
                        input->actor_position[actor].y, target_depth);
            stalled = pose_linked == next_state.primary_actor &&
                next_state.actor_state[pose_linked] == 0x05U;
            result.proximity_met = close;
            result.opcode12_stalled = stalled;

            /* `$8CEF-$8D58` applies the close pose before `$8E98` can stall.
               A non-close path instead publishes the synthesized target via
               `$92A8`; the source target planes are untouched when close. */
            if (close) {
                play_apply_opcode11_pose(
                    &next_state, &result, input, actor, pose_linked);
                following_offset = stalled
                    ? current_offset : play_next_offset(current_offset, 5U);
            } else {
                next_state.target_object[actor] = linked;
                next_state.target_x[actor] = target_x;
                next_state.target_depth[actor] = target_depth;
                following_offset = play_next_offset(
                    current_offset, stalled ? 5U : 10U);
                if (!stalled) {
                    play_apply_opcode11_pose(
                        &next_state, &result, input, actor, pose_linked);
                }
            }
            break;
        }
        case 16U: {
            uint16_t pointer = (uint16_t)command.arguments[0U] |
                ((uint16_t)command.arguments[1U] << 8U);
            uint8_t linked;
            uint16_t target_x;
            uint8_t target_depth;
            if (!input->pointer_workspace_valid || pointer != 0x0309U ||
                !play_valid_actor(next_state.defender_actor)) {
                result.deferred = true;
                result.deferred_reason =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE;
                break;
            }
            linked = next_state.defender_actor;
            target_x = (uint16_t)input->actor_position[linked].x;
            target_depth = (uint8_t)input->actor_position[linked].y;
            if (input->workspace_036e >= input->workspace_0370) {
                target_depth = target_depth >= 0x94U
                    ? (uint8_t)(target_depth - 10U)
                    : (uint8_t)(target_depth + 10U);
            } else if (input->orientation_035a == 0U) {
                target_x = (uint16_t)(target_x + 16U);
            } else {
                target_x = (uint16_t)(target_x - 16U);
            }
            next_state.target_object[actor] = linked;
            next_state.target_x[actor] = (int16_t)target_x;
            next_state.target_depth[actor] = (int16_t)target_depth;
            break;
        }
        case 7U: {
            uint8_t probe_index = command.arguments[0U];
            uint16_t alternate_offset = (uint16_t)command.arguments[2U] |
                ((uint16_t)command.arguments[3U] << 8U);
            if (probe_index >= TECMO_GAMEPLAY_CPU_STEERING_046E_PROBE_COUNT ||
                !play_stream_offset_valid(alternate_offset)) {
                return false;
            }
            next_state.actor_state[actor] = 0x04U;
            if (input->actor_046e_probe[probe_index] !=
                    command.arguments[1U]) {
                result.jump_offset = alternate_offset;
                result.jumped = true;
                /* The handler stores CA/CB and then calls $8FD9. */
                following_offset = play_next_offset(alternate_offset, 5U);
            } else {
                following_offset = play_next_offset(current_offset, 5U);
            }
            break;
        }
        case 9U:
            next_state.actor_state[actor] = command.arguments[0U];
            /* Bank06 opcode 9 copies C9 into actor-local $046E[X]. */
            next_state.action_state_046e[actor] = command.arguments[1U];
            next_state.action[actor] = 0x30U;
            break;
        case 14U:
            for (size_t other = 0U;
                 other < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++other) {
                if (other != actor &&
                    (input->actor_04b0[other] & 0x10U) != 0U) {
                    next_state.stream_offset[other] = 0x0023U;
                    next_state.actor_state[other] = 0x04U;
                }
            }
            next_state.actor_state[actor] = 0x04U;
            break;
        case 15U:
            /* The original switch gates and RAM side effects are distinct
               paths outside this bounded contract. Keep primary/defender
               state caller-owned and defer the handler effect. */
            result.deferred = true;
            result.deferred_reason =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE;
            break;
        case 17U:
        case 18U:
        case 19U:
            next_state.aggregation_06e0 = command.arguments[0U];
            next_state.aggregation_06df =
                (uint8_t)(next_state.aggregation_06df + 1U);
            next_state.aggregation_06e1 |= aggregation_bitmask[actor];
            next_state.actor_state[actor] = 0x0BU;
            break;
        case 21U:
            next_state.actor_state[actor] = 0x04U;
            following_offset = play_next_offset(
                current_offset,
                input->state_058a >= 4U && input->state_0357 == 0U &&
                input->state_0358 >= 4U && (input->flags_007e & 0x02U) == 0U
                    ? 10U : 5U);
            break;
        case 22U:
            next_state.global_0791 = command.arguments[0U];
            next_state.global_0792 = command.arguments[1U];
            next_state.global_0790 |= command.arguments[2U];
            break;
        default:
            break;
        }

        if (command.opcode == 6U ||
            (command.opcode == 10U && !result.proximity_met) ||
            command.opcode == 15U ||
            (command.opcode == 16U && (input->flags_ba & 0x03U) != 0U)) {
            following_offset = current_offset;
        } else if (command.opcode == 8U) {
            /* Redirect stores C8:C9 directly and returns; the complement
               writes state 4 then reaches the normal +5 tail. */
        } else if (command.opcode == 11U || command.opcode == 12U) {
            /* $8C40 itself calls $8FD9 after its source-dependent pose work. */
            if (command.opcode == 11U) {
                following_offset = play_next_offset(current_offset, 5U);
            }
        } else if (command.opcode == 13U || command.opcode == 16U ||
                   command.opcode == 0U || command.opcode == 2U) {
            /* The common $92CA tail skips $8FD9 when BA&3 is nonzero. */
            following_offset = (input->flags_ba & 0x03U) != 0U
                ? current_offset
                : play_next_offset(current_offset, 5U);
        } else if (command.opcode != 7U && command.opcode != 21U) {
            following_offset = play_next_offset(current_offset, 5U);
        }
        if (following_offset == current_offset &&
            command.opcode != 7U && command.opcode != 15U &&
            command.opcode != 6U &&
            command.opcode != 10U && command.opcode != 12U &&
            command.opcode != 20U &&
            command.opcode != 5U &&
             (command.opcode != 13U && command.opcode != 16U &&
             command.opcode != 0U && command.opcode != 2U ||
             (input->flags_ba & 0x03U) == 0U)) {
            result.deferred = true;
            if (result.deferred_reason ==
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE) {
                result.deferred_reason =
                    TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS;
            }
        } else {
            command_advanced = following_offset != current_offset;
            next_state.stream_offset[actor] = following_offset;
        }
        result.advanced = result.advanced || command_advanced;
        result.next_offset = next_state.stream_offset[actor];
        result.waiting = next_state.wait_counter[actor] != 0U;
        /* No non-goto handler chains into another record in this native tick. */
        goto_chain_active = false;
        break;
    }
    if (goto_chain_active && result.steps_executed >= input->step_budget) {
        result.budget_exhausted = true;
    }
    result.target_object = next_state.target_object[actor];
    result.target_x = next_state.target_x[actor];
    result.target_depth = next_state.target_depth[actor];
    *state_out = next_state;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_shot_request(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringShotInput *input,
    TecmoGameplayCpuSteeringShotResult *result_out)
{
    static const uint8_t difficulty_table[
        TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT] = {0x12U,0x1CU,0x28U};
    TecmoGameplayCpuSteeringShotResult result;
    uint8_t timer_sum;
    if (assets == NULL || !assets->available || input == NULL ||
        result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_STEERING_SHOT_INPUT_TAG ||
        input->difficulty >= TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_SHOT_RESULT_TAG;
    result.difficulty_threshold = difficulty_table[input->difficulty];
    timer_sum = (uint8_t)((input->timer_0760 >> 1U) +
                          input->timer_0760 +
                          result.difficulty_threshold);
    result.timer_sum = timer_sum;
    result.rating_bucket = (uint8_t)(input->rating_0533 >> 5U);
    if ((input->state_0588 & 0x01U) != 0U ||
        (input->flags_ba & 0x40U) != 0U ||
        input->target_delta_high != 0U ||
        input->target_delta_low >= 0x10U || input->gate_0478 != 0U ||
        input->timer_0798 == 0U || timer_sum < input->timer_0798 ||
        result.rating_bucket < input->random_byte) {
        *result_out = result;
        return true;
    }
    /* Semantic result of exact $9217->$9270; the C711 transport code is not
       exposed as a basketball action or a runtime address. */
    result.actor_state = 0x0DU;
    result.action = 0x32U;
    result.timer = 0x12U;
    result.handoff_code = 0x03U;
    result.request = true;
    result.wrote_state = true;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_steering_direction_for_delta(
    const TecmoGameplayCpuSteeringAssets *assets,
    int16_t horizontal_delta,
    int16_t depth_delta,
    uint8_t *direction_out)
{
    uint32_t horizontal_magnitude;
    uint32_t depth_magnitude;
    uint16_t doubled_horizontal;
    uint16_t doubled_depth;
    uint8_t map_index;
    if (assets == NULL || !assets->available || direction_out == NULL ||
        (horizontal_delta == 0 && depth_delta == 0)) {
        return false;
    }
    horizontal_magnitude = horizontal_delta < 0
        ? (uint32_t)(-(int32_t)horizontal_delta)
        : (uint32_t)horizontal_delta;
    depth_magnitude = depth_delta < 0
        ? (uint32_t)(-(int32_t)depth_delta)
        : (uint32_t)depth_delta;

    if (horizontal_magnitude >= depth_magnitude) {
        /* $8933-$8940 doubles the other 16-bit magnitude and discards
           overflow before the unsigned comparison. */
        doubled_depth = (uint16_t)(depth_magnitude << 1U);
        if ((uint16_t)horizontal_magnitude >= doubled_depth) {
            map_index = horizontal_delta < 0 ? 5U : 4U;
        } else {
            map_index = (uint8_t)((horizontal_delta < 0 ? 2U : 0U) +
                                  (depth_delta < 0 ? 1U : 0U));
        }
    } else {
        /* $897A-$8989 can re-enter the horizontal path with the already
           doubled magnitude. Preserve that branch and both 16-bit wraps. */
        doubled_horizontal = (uint16_t)(horizontal_magnitude << 1U);
        if ((uint16_t)depth_magnitude >= doubled_horizontal) {
            map_index = depth_delta < 0 ? 7U : 6U;
        } else {
            doubled_depth = (uint16_t)(depth_magnitude << 1U);
            if (doubled_horizontal >= doubled_depth) {
                map_index = horizontal_delta < 0 ? 5U : 4U;
            } else {
                map_index = (uint8_t)((horizontal_delta < 0 ? 2U : 0U) +
                                      (depth_delta < 0 ? 1U : 0U));
            }
        }
    }
    if (map_index >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT ||
        assets->direction_map[map_index] >=
            TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT) {
        return false;
    }
    *direction_out = assets->direction_map[map_index];
    return true;
}

static uint8_t harness_actor_team(uint8_t actor)
{
    return actor < TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT ? 0U : 1U;
}

static uint32_t harness_hash_byte(uint32_t hash, uint8_t value)
{
    hash ^= value;
    hash *= 16777619U;
    return hash;
}

static uint32_t harness_hash_u16(uint32_t hash, uint16_t value)
{
    hash = harness_hash_byte(hash, (uint8_t)(value & 0xFFU));
    return harness_hash_byte(hash, (uint8_t)(value >> 8U));
}

static uint32_t harness_input_fingerprint(
    const TecmoGameplayCpuSteeringHarnessInput *input)
{
    static const uint8_t domain[] = {'T','G','A','H','1'};
    uint32_t hash = 2166136261U;
    size_t actor;
    for (actor = 0U; actor < sizeof(domain); ++actor) {
        hash = harness_hash_byte(hash, domain[actor]);
    }
    hash = harness_hash_byte(hash, input->actor);
    hash = harness_hash_byte(hash, input->possession);
    hash = harness_hash_byte(hash, input->orientation);
    hash = harness_hash_byte(hash, input->ball_holder);
    hash = harness_hash_byte(hash, input->matchup_actor);
    hash = harness_hash_byte(hash, input->difficulty);
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        hash = harness_hash_u16(
            hash, (uint16_t)input->actor_position[actor].x);
        hash = harness_hash_u16(
            hash, (uint16_t)input->actor_position[actor].y);
    }
    if (input->has_explicit_target) {
        /* Keep the legacy CLI/test fingerprint stable when the optional
           native-policy target is not supplied, while domain-separating the
           caller-owned coordinate when it is present. */
        hash = harness_hash_byte(hash, 'X');
        hash = harness_hash_u16(hash, (uint16_t)input->explicit_target.x);
        hash = harness_hash_u16(hash, (uint16_t)input->explicit_target.y);
    }
    return hash;
}

static bool harness_input_valid(
    const TecmoGameplayCpuSteeringHarnessInput *input)
{
    size_t actor;
    uint8_t actor_team;
    if (input == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG ||
        input->actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        input->possession >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        input->orientation >=
            TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT ||
        (input->ball_holder >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT &&
         input->ball_holder != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) ||
        input->matchup_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        input->difficulty >=
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT ||
        (input->ball_holder == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR
             ? !input->has_explicit_target
             : harness_actor_team(input->ball_holder) != input->possession)) {
        return false;
    }
    actor_team = harness_actor_team(input->actor);
    if (harness_actor_team(input->matchup_actor) == actor_team) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (!tecmo_gameplay_court_coordinate_valid(
                &input->actor_position[actor])) {
            return false;
        }
    }
    return !input->has_explicit_target ||
           tecmo_gameplay_court_coordinate_valid(&input->explicit_target);
}

bool tecmo_gameplay_cpu_steering_harness_evaluate(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringHarnessInput *input,
    TecmoGameplayCpuSteeringHarnessResult *result_out)
{
    static const int16_t approach_by_difficulty[
        TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT] = {48,48,40};
    TecmoGameplayCpuSteeringHarnessResult result;
    int32_t horizontal_delta;
    int32_t depth_delta;
    if (assets == NULL || !assets->available || result_out == NULL ||
        !harness_input_valid(input)) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_RESULT_TAG;
    result.input_fingerprint = harness_input_fingerprint(input);
    result.actor = input->actor;
    result.actor_team = harness_actor_team(input->actor);
    result.possession = input->possession;
    result.orientation = input->orientation;
    result.ball_holder = input->ball_holder;
    result.matchup_actor = input->matchup_actor;
    result.difficulty = input->difficulty;
    result.actor_position = input->actor_position[input->actor];
    result.direction = TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;

    if (input->has_explicit_target) {
        result.target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET;
        result.target_actor = input->matchup_actor;
        result.target_position = input->explicit_target;
    } else if (input->actor == input->ball_holder) {
        int16_t approach = approach_by_difficulty[input->difficulty];
        result.target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH;
        result.target_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        result.target_position.x = input->orientation == 0U
            ? (int16_t)(TECMO_GAMEPLAY_COURT_LEFT_HOOP_X + approach)
            : (int16_t)(TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X - approach);
        result.target_position.y = TECMO_GAMEPLAY_COURT_HOOP_Y;
    } else {
        result.target_kind =
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR;
        result.target_actor = input->matchup_actor;
        result.target_position =
            input->actor_position[input->matchup_actor];
    }
    horizontal_delta = (int32_t)result.target_position.x -
                       result.actor_position.x;
    depth_delta = (int32_t)result.target_position.y -
                  result.actor_position.y;
    if (horizontal_delta < INT16_MIN || horizontal_delta > INT16_MAX ||
        depth_delta < INT16_MIN || depth_delta > INT16_MAX) {
        return false;
    }
    result.horizontal_delta = (int16_t)horizontal_delta;
    result.depth_delta = (int16_t)depth_delta;
    if (horizontal_delta != 0 || depth_delta != 0) {
        if (!tecmo_gameplay_cpu_steering_direction_for_delta(
                assets, result.horizontal_delta, result.depth_delta,
                &result.direction)) {
            return false;
        }
        result.writes_direction = true;
    }
    *result_out = result;
    return true;
}

static bool movement_input_for_direction(
    const TecmoGameplayMovementAssets *assets,
    uint8_t direction,
    uint8_t *held_direction_bits_out)
{
    static const uint8_t inputs[
        TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT] = {
        TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT
    };
    uint8_t held_direction_bits;
    if (assets == NULL || !assets->available ||
        direction >= TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT ||
        held_direction_bits_out == NULL) {
        return false;
    }
    held_direction_bits = inputs[direction];
    if (!tecmo_gameplay_movement_input_valid(held_direction_bits) ||
        assets->direction_map[held_direction_bits] != direction) {
        return false;
    }
    *held_direction_bits_out = held_direction_bits;
    return true;
}

bool tecmo_gameplay_cpu_steering_movement_step(
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayCpuSteeringMovementInput *input,
    TecmoGameplayCpuSteeringMovementResult *result_out)
{
    TecmoGameplayCpuSteeringMovementResult result;
    TecmoGameplayMovementStepInput movement_input;
    const TecmoGameplayCourtCoordinate *selected_position;
    if (steering_assets == NULL || !steering_assets->available ||
        movement_assets == NULL || !movement_assets->available ||
        steering_assets->movement_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 ||
        input == NULL || result_out == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG ||
        !tecmo_gameplay_movement_state_valid(
            movement_assets, &input->movement) ||
        input->steering.actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        input->primary_selected_actor !=
            (input->steering.actor == input->steering.ball_holder)) {
        return false;
    }
    selected_position =
        &input->steering.actor_position[input->steering.actor];
    if (selected_position->x != input->movement.position.x ||
        selected_position->y != input->movement.position.y) {
        return false;
    }

    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_RESULT_TAG;
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            steering_assets, &input->steering, &result.steering)) {
        return false;
    }
    result.held_direction_bits = TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    if (result.steering.writes_direction &&
        !movement_input_for_direction(
            movement_assets, result.steering.direction,
            &result.held_direction_bits)) {
        return false;
    }

    result.movement = input->movement;
    memset(&movement_input, 0, sizeof(movement_input));
    movement_input.held_direction_bits = result.held_direction_bits;
    movement_input.player_movement_rating =
        input->player_movement_rating;
    movement_input.condition = input->condition;
    movement_input.speed_value = input->speed_value;
    movement_input.global_object_state = input->global_object_state;
    movement_input.movement_flags = input->movement_flags;
    movement_input.primary_selected_actor = input->primary_selected_actor;
    if (!tecmo_gameplay_movement_step(
            movement_assets, &result.movement, &movement_input)) {
        return false;
    }
    *result_out = result;
    return true;
}

const char *tecmo_gameplay_cpu_steering_direction_name(uint8_t direction)
{
    static const char *const names[8] = {
        "right","left","down","down-right",
        "down-left","up","up-right","up-left"
    };
    return direction < 8U ? names[direction] : "invalid";
}

const char *tecmo_gameplay_cpu_steering_command_kind_name(
    TecmoGameplayCpuSteeringCommandKind kind)
{
    static const char *const names[] = {
        "control","relative-target","absolute-target","actor-target",
        "direct-direction","linked-target","global-target",
        "pointer-actor-target"
    };
    return (unsigned)kind < sizeof(names) / sizeof(names[0])
        ? names[(unsigned)kind]
        : "invalid";
}

const char *tecmo_gameplay_cpu_steering_harness_target_kind_name(
    TecmoGameplayCpuSteeringHarnessTargetKind kind)
{
    static const char *const names[] = {
        "linked-actor","hoop-approach","explicit-coordinate"
    };
    return (unsigned)kind < sizeof(names) / sizeof(names[0])
        ? names[(unsigned)kind]
        : "invalid";
}

static bool direction_vector(
    const TecmoGameplayCpuSteeringAssets *assets,
    int16_t horizontal,
    int16_t depth,
    uint8_t expected)
{
    uint8_t direction = 0xFFU;
    return tecmo_gameplay_cpu_steering_direction_for_delta(
               assets, horizontal, depth, &direction) &&
           direction == expected;
}

static bool harness_rejected_unchanged(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringHarnessInput *input)
{
    TecmoGameplayCpuSteeringHarnessResult result;
    TecmoGameplayCpuSteeringHarnessResult before;
    memset(&before, 0xA5, sizeof(before));
    result = before;
    return !tecmo_gameplay_cpu_steering_harness_evaluate(
               assets, input, &result) &&
           memcmp(&result, &before, sizeof(result)) == 0;
}

static bool movement_step_rejected_unchanged(
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayCpuSteeringMovementInput *input)
{
    TecmoGameplayCpuSteeringMovementResult result;
    TecmoGameplayCpuSteeringMovementResult before;
    memset(&before, 0xA5, sizeof(before));
    result = before;
    return !tecmo_gameplay_cpu_steering_movement_step(
               steering_assets, movement_assets, input, &result) &&
           memcmp(&result, &before, sizeof(result)) == 0;
}

static bool movement_composition_self_test(
    const char *asset_pack_path,
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    char *message,
    size_t message_size)
{
    static const int16_t vectors[
        TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT][2] = {
        {10,0},{-10,0},{0,10},{10,10},
        {-10,10},{0,-10},{10,-10},{-10,-10}
    };
    static const uint8_t held_inputs[
        TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT] = {
        TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT,
        TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT
    };
    static const TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {
            {394,148},{420,120},{440,180},{360,100},{400,200},
            {384,148},{520,110},{540,190},{460,90},{480,210}
        };
    TecmoGameplayMovementAssets movement_assets;
    TecmoGameplayCpuSteeringMovementInput input;
    TecmoGameplayCpuSteeringMovementInput malformed;
    TecmoGameplayCpuSteeringMovementResult result;
    TecmoGameplayCourtCoordinate start;
    tecmo_gameplay_movement_assets_init(&movement_assets);
    if (!tecmo_gameplay_movement_assets_load(
            &movement_assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s",
                       movement_assets.status);
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
    input.steering.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    memcpy(input.steering.actor_position, positions, sizeof(positions));
    input.steering.actor = 5U;
    input.steering.possession = 0U;
    input.steering.ball_holder = 0U;
    input.steering.matchup_actor = 0U;
    input.steering.difficulty = 1U;
    input.player_movement_rating = 20U;
    input.condition = 100U;
    input.speed_value = 1U;
    start = input.steering.actor_position[input.steering.actor];

    for (uint8_t direction = 0U;
         direction < TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT;
         ++direction) {
        input.steering.actor_position[0U].x =
            (int16_t)(start.x + vectors[direction][0]);
        input.steering.actor_position[0U].y =
            (int16_t)(start.y + vectors[direction][1]);
        input.steering.actor_position[5U] = start;
        if (!tecmo_gameplay_movement_state_initialize(
                &movement_assets, &input.movement, &start, 0U) ||
            !tecmo_gameplay_cpu_steering_movement_step(
                steering_assets, &movement_assets, &input, &result) ||
            result.contract_tag !=
                TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_RESULT_TAG ||
            !result.steering.writes_direction ||
            result.steering.direction != direction ||
            result.held_direction_bits != held_inputs[direction] ||
            result.movement.position.x != start.x ||
            result.movement.position.y != start.y ||
            result.movement.action_state != held_inputs[direction] ||
            result.movement.direction != direction) {
            (void)snprintf(
                message, message_size,
                "TGAI-3 to TGMO-1 direction composition failed.");
            tecmo_gameplay_movement_assets_destroy(&movement_assets);
            return false;
        }
    }

    /* Shot flight has no possession holder, but the ordinary actor command
       stream may still provide an exact target. Keep this admission narrower
       than held-ball steering: no-holder without an explicit target must be
       byte-identically rejected. */
    input.steering.actor_position[5U] = start;
    input.steering.ball_holder = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    input.steering.has_explicit_target = true;
    input.steering.explicit_target.x = (int16_t)(start.x + 10);
    input.steering.explicit_target.y = start.y;
    input.primary_selected_actor = false;
    if (!tecmo_gameplay_movement_state_initialize(
            &movement_assets, &input.movement, &start, 0U) ||
        !tecmo_gameplay_cpu_steering_movement_step(
            steering_assets, &movement_assets, &input, &result) ||
        result.steering.ball_holder !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        result.steering.target_position.x != start.x + 10 ||
        result.steering.target_position.y != start.y ||
        result.held_direction_bits != TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT) {
        (void)snprintf(message, message_size,
                       "TGAI-3 no-holder explicit-target composition failed.");
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        return false;
    }
    input.steering.has_explicit_target = false;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &input)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 no-holder missing-target rejection failed.");
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        return false;
    }
    input.steering.ball_holder = 0U;
    input.steering.has_explicit_target = false;
    memset(&input.steering.explicit_target, 0,
           sizeof(input.steering.explicit_target));

    input.steering.actor_position[0U].x = (int16_t)(start.x + 10);
    input.steering.actor_position[0U].y = start.y;
    input.steering.actor_position[5U] = start;
    if (!tecmo_gameplay_movement_state_initialize(
            &movement_assets, &input.movement, &start, 0U) ||
        !tecmo_gameplay_cpu_steering_movement_step(
            steering_assets, &movement_assets, &input, &result)) {
        goto movement_vector_failure;
    }
    input.movement = result.movement;
    input.steering.actor_position[5U] = result.movement.position;
    if (!tecmo_gameplay_cpu_steering_movement_step(
            steering_assets, &movement_assets, &input, &result) ||
        result.movement.position.x != start.x + 1 ||
        result.movement.position.y != start.y ||
        result.movement.fractional_accumulator != 3U ||
        result.movement.animation_phase != 0x40U) {
        goto movement_vector_failure;
    }

    input.steering.actor_position[0U] = start;
    input.steering.actor_position[5U] = start;
    if (!tecmo_gameplay_movement_state_initialize(
            &movement_assets, &input.movement, &start, 0U) ||
        !tecmo_gameplay_cpu_steering_movement_step(
            steering_assets, &movement_assets, &input, &result) ||
        result.steering.writes_direction ||
        result.steering.direction !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
        result.held_direction_bits !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL ||
        result.movement.position.x != start.x ||
        result.movement.position.y != start.y ||
        result.movement.action_state !=
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL) {
        (void)snprintf(
            message, message_size,
            "TGAI-3 zero-vector neutral composition failed.");
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        return false;
    }

    malformed = input;
    malformed.contract_tag ^= 1U;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    malformed.steering.contract_tag ^= 1U;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    ++malformed.steering.actor_position[5U].x;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    malformed.movement.contract_tag ^= 1U;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    malformed.primary_selected_actor = true;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    malformed.condition = 101U;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed)) {
        goto movement_transaction_failure;
    }
    malformed = input;
    malformed.speed_value = TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT;
    if (!movement_step_rejected_unchanged(
            steering_assets, &movement_assets, &malformed) ||
        !movement_step_rejected_unchanged(
            steering_assets, &movement_assets, NULL) ||
        tecmo_gameplay_cpu_steering_movement_step(
            steering_assets, &movement_assets, &input, NULL)) {
        goto movement_transaction_failure;
    }
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    return true;

movement_vector_failure:
    (void)snprintf(message, message_size,
                   "TGAI-3 to TGMO-1 movement vector failed.");
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    return false;

movement_transaction_failure:
    (void)snprintf(
        message, message_size,
        "TGAI-3 to TGMO-1 transactional rejection failed.");
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    return false;
}

static bool find_lifecycle_opcode_offset(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t opcode,
    uint16_t *offset_out)
{
    TecmoGameplayCpuSteeringCommand command;
    if (assets == NULL || offset_out == NULL ||
        opcode >= TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
        return false;
    }
    for (uint16_t offset = 0U;
         offset < TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT *
             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE;
         offset = (uint16_t)(offset +
             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        if (tecmo_gameplay_cpu_steering_decode_command(
                assets, offset, &command) && command.opcode == opcode) {
            *offset_out = offset;
            return true;
        }
    }
    return false;
}

static void opcode15_raw_fixture(
    TecmoGameplayCpuSteeringOpcode15RawInput *input)
{
    memset(input, 0, sizeof(*input));
    input->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_INPUT_TAG;
    input->observed_mask =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK;
    input->command_record_offset =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET;
    input->actor_x = 6U;
    input->raw_0499_slot10 = 0x46U;
    input->raw_04b0_actor_x = 0x10U;
    input->raw_007e = 0U;
    input->raw_0308_primary = 4U;
    input->raw_0309_defender = 9U;
    input->raw_030a_offense_side = 0U;
    input->raw_030b_defense_side = 1U;
    input->raw_000e_000f_selected_actor[0U] = 4U;
    input->raw_000e_000f_selected_actor[1U] = 9U;
    input->raw_06d5 = 6U;
    input->raw_06d6 = 2U;
    input->raw_059e = 5U;
    input->raw_037f_0380_primary_link[0U] = 8U;
    input->raw_037f_0380_primary_link[1U] = 7U;
    input->raw_06da = 3U;
    input->raw_06db = 1U;
    input->raw_058b = 0x22U;
    input->raw_058c = 0x33U;
    input->raw_06df = 4U;
    input->raw_06e1 = 5U;
    input->formation_output.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_FORMATION_TAG;
    input->formation_output.assigned_actor_mask = 0x03AFU;
    input->formation_output.raw_058b_after = 2U;
    input->formation_output.raw_058c_after = 3U;
    for (uint8_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        input->actor[actor].raw_0547_0551_stream_offset =
            (uint16_t)(0x0100U + actor * 5U);
        input->actor[actor].raw_057c_state = 0x04U;
        input->actor[actor].raw_046e_timer = (uint8_t)(0x20U + actor);
        input->actor[actor].raw_0463_direction =
            (uint8_t)(actor % TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT);
        input->actor[actor].raw_0442_pose_low = 0xAAU;
        input->actor[actor].raw_044d_pose_high = 0xBBU;
        input->actor[actor].raw_0479_sprite_flags = 0x40U;
        input->actor[actor].raw_0458_action = 0x50U;
        input->formation_output.actor_stream_offset[actor] =
            (uint16_t)(0x0200U + actor * 5U);
        input->formation_output.actor_state[actor] = 4U;
    }
    input->actor[9U].raw_0547_0551_stream_offset = 0x1234U;
    input->actor[9U].raw_057c_state = 0x08U;
    input->actor[9U].raw_046e_timer = 0xC3U;
    input->actor[9U].raw_0463_direction = 5U;
}

static bool opcode15_raw_resolver_self_test(
    const TecmoGameplayCpuSteeringAssets *assets)
{
    TecmoGameplayCpuSteeringOpcode15RawInput input;
    TecmoGameplayCpuSteeringOpcode15RawInput output;
    TecmoGameplayCpuSteeringOpcode15RawInput before;
    TecmoGameplayCpuSteeringOpcode15RawInput output_before;
    TecmoGameplayCpuSteeringOpcode15RawResult result;
    TecmoGameplayCpuSteeringOpcode15RawResult result_before;

    opcode15_raw_fixture(&input);
    input.observed_mask =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499;
    input.raw_0499_slot10 = 0x45U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_RESULT_TAG ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_GATE_NOOP ||
        result.committed || !result.returned_9179_without_advance ||
        memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }

    opcode15_raw_fixture(&input);
    input.raw_04b0_actor_x = 0U;
    input.raw_007e = 0x04U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_BIT2_RETURN ||
        result.committed || !result.returned_9179_without_advance ||
        memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }
    input.raw_007e = 0U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_PRIMARY_REPLACED ||
        !result.committed || result.returned_9179_without_advance ||
        result.raw_0308_before != 4U || result.raw_0308_after != 6U ||
        result.raw_037f_before != 8U || result.raw_037f_after != 4U ||
        result.raw_06da_before != 3U || result.raw_06da_after != 4U ||
        result.raw_06db_before != 1U || result.raw_06db_after != 9U ||
        !result.c060_invalidated_058b_058c_to_ff ||
        result.raw_058b_after_c060 != 0xFFU ||
        result.raw_058c_after_c060 != 0xFFU ||
        result.raw_058b_before != 0x22U || result.raw_058c_before != 0x33U ||
        result.raw_058b_after != 2U || result.raw_058c_after != 3U ||
        result.raw_06df_after != 0U || result.raw_06e1_after != 0U ||
        result.formation_assigned_actor_mask != 0x03AFU ||
        !result.primary_state7_handoff_observed ||
        result.c711_selector != 4U ||
        !result.c711_selector_observed_unexecuted ||
        output.raw_0308_primary != 6U ||
        output.actor[4U].raw_0442_pose_low != 0x0AU ||
        output.actor[4U].raw_044d_pose_high != 0x04U ||
        output.actor[4U].raw_0479_sprite_flags != 0xC1U ||
        output.actor[4U].raw_0458_action != 0x30U ||
        output.raw_037f_0380_primary_link[0U] != 4U ||
        output.raw_06da != 4U || output.raw_06db != 9U ||
        output.raw_058b != 2U || output.raw_058c != 3U ||
        output.actor[5U].raw_0547_0551_stream_offset != 0x0219U ||
        output.actor[5U].raw_057c_state != 4U ||
        output.actor[6U].raw_046e_timer != 0x1BU ||
        output.actor[6U].raw_057c_state != 0U ||
        output.raw_059e != 6U ||
        output.raw_000e_000f_selected_actor[0U] != 6U) {
        return false;
    }
    opcode15_raw_fixture(&input);
    input.raw_04b0_actor_x = 0U;
    input.formation_output.assigned_actor_mask |= (uint16_t)(1U << 6U);
    memset(&output, 0xA5, sizeof(output));
    output_before = output;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    if (tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) {
        return false;
    }
    opcode15_raw_fixture(&input);
    input.raw_04b0_actor_x = 0U;
    input.observed_mask &= ~((uint32_t)
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT);
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }
    input.raw_04b0_actor_x = 0x10U;
    input.raw_007e = 0x08U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_QUALIFIED_BIT3_RETURN ||
        result.committed || !result.returned_9179_without_advance ||
        memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }

    opcode15_raw_fixture(&input);
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED ||
        !result.committed || result.returned_9179_without_advance ||
        result.raw_0308_before != 4U ||
        result.raw_0308_after != 4U || result.raw_0309_before != 9U ||
        result.raw_0309_after != 6U || result.defender_stream_before != 0x1234U ||
        result.defender_stream_after != 0x005AU ||
        result.defender_state_before != 0x08U ||
        result.defender_state_after != 0x04U ||
        result.new_actor_state_before != 0x04U ||
        result.new_actor_state_after != 0x07U ||
        result.raw_06d5_before != 6U || result.raw_06d5_after != 9U ||
        result.raw_06d6_before != 2U || result.raw_06d6_after != 9U ||
        result.raw_059e_before != 5U || result.raw_059e_after != 6U ||
        result.c711_selector != 4U || result.c711_x_actor != 6U ||
        result.c711_y_actor != 6U ||
        !result.c711_selector_observed_unexecuted ||
        output.raw_0309_defender != 6U ||
        output.actor[9U].raw_0547_0551_stream_offset != 0x005AU ||
        output.actor[9U].raw_057c_state != 0x04U ||
        output.actor[9U].raw_046e_timer != 0U ||
        output.actor[9U].raw_0442_pose_low != 0x0EU ||
        output.actor[9U].raw_044d_pose_high != 0x04U ||
        output.actor[9U].raw_0479_sprite_flags != 0xC1U ||
        output.actor[9U].raw_0458_action != 0x30U ||
        output.actor[6U].raw_0479_sprite_flags != 0x81U ||
        output.actor[6U].raw_057c_state != 0x07U ||
        output.raw_000e_000f_selected_actor[1U] != 6U ||
        output.raw_06d5 != 9U || output.raw_06d6 != 9U ||
        output.raw_059e != 6U) {
        return false;
    }
    opcode15_raw_fixture(&input);
    input.observed_mask &=
        ~((uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_PRIMARY_LINKS |
          (uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_FORMATION_OUTPUT);
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED ||
        !result.committed) {
        return false;
    }
    input.command_record_offset =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED) {
        return false;
    }

    opcode15_raw_fixture(&input);
    input.raw_06d5 = 5U;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED ||
        !result.committed || result.raw_06d5_before != 5U ||
        result.raw_06d5_after != 5U || output.raw_06d5 != 5U ||
        result.raw_06d6_before != 2U || result.raw_06d6_after != 9U ||
        output.raw_06d6 != 9U) {
        return false;
    }

    opcode15_raw_fixture(&input);
    input.actor[9U].raw_0463_direction = 8U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_INVALID_DIRECTION ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }
    opcode15_raw_fixture(&input);
    input.observed_mask &=
        ~((uint32_t)TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E);
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0 ||
        (result.missing_raw_mask &
         TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SELECTION_059E) == 0U) {
        return false;
    }

    opcode15_raw_fixture(&input);
    before = input;
    memset(&output, 0xA5, sizeof(output));
    output_before = output;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    input.contract_tag = 0U;
    if (tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        memcmp(&output, &output_before, sizeof(output)) != 0) {
        return false;
    }
    /* Invalid inputs must leave caller outputs untouched. */
    if (memcmp(&result, &result_before, sizeof(result)) != 0) return false;
    input = before;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    if (tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &input, &result) ||
        memcmp(&input, &before, sizeof(input)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) {
        return false;
    }
    return true;
}

/* These are independent parser-layer mutations: the normal public parser
   also rejects them at its immutable whole-payload fingerprint, while this
   narrow test proves the nested opcode-15 descriptor/raw/anchor checks are
   not merely metadata beside an unchecked payload. */
static bool opcode15_parser_anchor_self_test(
    const TecmoGameplayCpuSteeringAssets *assets)
{
    uint8_t *mutated;
    size_t handler_offset;
    bool valid = assets != NULL && assets->storage != NULL &&
        assets->storage_size == TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE;
    if (!valid) return false;
    mutated = (uint8_t *)malloc(assets->storage_size);
    if (mutated == NULL) return false;
    handler_offset =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET +
        (0x9146U - 0x8BE1U);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[handler_offset + (0x920DU - 0x9146U)] ^= 1U;
    valid = !validate_opcode15_contract(mutated);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_POSE_LOW_OFFSET] ^= 1U;
    valid = valid && !validate_opcode15_contract(mutated) &&
        !validate_header(mutated, assets->storage_size);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET] ^= 1U;
    valid = valid && !validate_opcode15_contract(mutated);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET] ^= 1U;
    valid = valid && !validate_opcode15_contract(mutated);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET +
            8U] ^= 1U;
    valid = valid && !validate_header(mutated, assets->storage_size);

    memcpy(mutated, assets->storage, assets->storage_size);
    mutated[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
            (TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_HANDLERS - 1U) *
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE + 12U] ^= 1U;
    valid = valid && !validate_source_records(mutated, assets->storage_size);
    free(mutated);
    return valid;
}

static bool play_formation_graph_reaches_records(
    const TecmoGameplayCpuSteeringAssets *assets,
    const uint16_t *target_offsets,
    size_t target_count,
    bool opcode15_holds_record)
{
    bool visited[TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT] = {false};
    uint16_t queue[TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT];
    size_t head = 0U;
    size_t tail = 0U;
    uint16_t stream_size = (uint16_t)(
        assets->command_record_count * TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE);
    for (size_t formation = 0U;
         formation < assets->formation_start_count; ++formation) {
        for (size_t actor = 0U;
             actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
            uint16_t offset = assets->formation_stream_offsets[formation][actor];
            size_t index = offset / TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE;
            if (offset < stream_size &&
                offset % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE == 0U &&
                !visited[index]) {
                visited[index] = true;
                queue[tail++] = offset;
            }
        }
    }
    while (head < tail) {
        TecmoGameplayCpuSteeringCommand command;
        uint16_t edges[3U];
        size_t edge_count = 0U;
        uint16_t offset = queue[head++];
        for (size_t target = 0U; target < target_count; ++target) {
            if (offset == target_offsets[target]) return true;
        }
        if (!tecmo_gameplay_cpu_steering_decode_command(
                assets, offset, &command)) {
            return true;
        }
        if (command.opcode == 1U) {
            edges[edge_count++] = (uint16_t)command.arguments[0U] |
                ((uint16_t)command.arguments[1U] << 8U);
        } else if (!(opcode15_holds_record && command.opcode == 15U)) {
            edges[edge_count++] = play_next_offset(offset, 5U);
            if (command.opcode == 7U) {
                edges[edge_count++] = play_next_offset(
                    (uint16_t)command.arguments[2U] |
                        ((uint16_t)command.arguments[3U] << 8U), 5U);
            } else if (command.opcode == 8U) {
                edges[edge_count++] = (uint16_t)command.arguments[0U] |
                    ((uint16_t)command.arguments[1U] << 8U);
            } else if (command.opcode == 12U || command.opcode == 21U) {
                edges[edge_count++] = play_next_offset(offset, 10U);
            }
        }
        for (size_t edge = 0U; edge < edge_count; ++edge) {
            uint16_t next = edges[edge];
            size_t index = next / TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE;
            if (next < stream_size &&
                next % TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE == 0U &&
                !visited[index]) {
                if (tail >= TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT) {
                    return true;
                }
                visited[index] = true;
                queue[tail++] = next;
            }
        }
    }
    return false;
}

static bool play_formation_graph_reaches_opcode12_record(
    const TecmoGameplayCpuSteeringAssets *assets)
{
    static const uint16_t targets[3U] = {0x0069U, 0x006EU, 0x0073U};
    return play_formation_graph_reaches_records(
        assets, targets, 3U, false);
}

static bool play_formation_graph_reaches_opcode13_record_0041(
    const TecmoGameplayCpuSteeringAssets *assets)
{
    static const uint16_t target = 0x0041U;
    /* `$9172` returns without advancing for both successful replacement and
       no-op/gate outcomes. Therefore `$0037->$003C` is not a legal edge. */
    return play_formation_graph_reaches_records(
        assets, &target, 1U, true);
}

bool tecmo_gameplay_cpu_steering_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    char fixed_rng_message[192];
    char defense_interaction_message[192];
    TecmoGameplayCpuSteeringAssets assets;
    TecmoGameplayCpuSteeringCommand command;
    TecmoGameplayCpuSteeringCommand before;
    static const TecmoGameplayCourtCoordinate harness_positions[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {
            {384,148},{420,120},{440,180},{360,100},{400,200},
            {500,160},{520,110},{540,190},{460,90},{480,210}
        };
    TecmoGameplayCpuSteeringHarnessInput harness_input;
    TecmoGameplayCpuSteeringHarnessInput malformed_input;
    TecmoGameplayCpuSteeringHarnessResult harness_result;
    TecmoGameplayCpuSteeringHarnessResult repeat_result;
    TecmoGameplayCpuSteeringFormationResult formation_result;
    TecmoGameplayCpuSteeringFormationResult formation_before;
    TecmoGameplayCpuSteeringRouteInput route_input;
    TecmoGameplayCpuSteeringRouteResult route_result;
    TecmoGameplayCpuSteeringRouteResult route_before;
    TecmoGameplayCpuSteeringRouteLaunchInput launch_input;
    TecmoGameplayCpuSteeringRouteLaunchResult launch_result;
    TecmoGameplayCpuSteeringRouteLaunchResult launch_before;
    TecmoGameplayCpuSteeringRouteMotionState motion_out;
    TecmoGameplayCpuSteeringRouteMotionState motion_before;
    TecmoGameplayCpuSteeringRouteStepResult route_step_result;
    TecmoGameplayCpuSteeringRouteStepResult route_step_before;
    TecmoGameplayCpuSteeringPlayState play_state;
    TecmoGameplayCpuSteeringPlayState play_before;
    TecmoGameplayCpuSteeringPlayState play_out;
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCpuSteeringPlayInput play_input_before;
    TecmoGameplayCpuSteeringPlayResult play_result;
    TecmoGameplayCpuSteeringPlayResult play_result_before;
    char global_latch_message[192];
    char a9da_assignment_message[192];
    char a0f3_launch_message[192];
    char a8e9_velocity_message[192];
    char opcode15_selection_message[192];
    TecmoGameplayCpuSteeringShotInput shot_input;
    TecmoGameplayCpuSteeringShotResult shot_result;
    TecmoGameplayCpuSteeringShotResult shot_before;
    static const int16_t route_octant_delta[8U][2U] = {
        {64,0},{-64,0},{0,64},{64,64},
        {-64,64},{0,-64},{64,-64},{-64,-64}
    };
    uint16_t opcode_offsets[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint32_t baseline_fingerprint;
    uint8_t direction = 0xA5U;
    tecmo_gameplay_cpu_steering_assets_init(&assets);
    if (!tecmo_gameplay_cpu_steering_assets_load(
            &assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s", assets.status);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* Exact planar route arithmetic subset: $88DA-$8AF3 plus the
       revision-locked $9BD8-$9C6E divider. Presentation/action side effects
       are deliberately outside this pure contract. */
    memset(&launch_input, 0, sizeof(launch_input));
    launch_input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_INPUT_TAG;
    launch_input.actor_position.x = 256;
    launch_input.actor_position.y = 128;
    launch_input.condition_7c48 = 100U;
    launch_input.movement_value_06e7 = 20U;
    launch_input.horizontal_delta = 64;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        !launch_result.launched || launch_result.direction != 0U ||
        launch_result.duration != 45U ||
        launch_result.motion.horizontal_velocity_q6 != 91 ||
        launch_result.motion.depth_velocity_q6 != 0 ||
        launch_result.motion.horizontal_accumulator_q6 != 0x4000U ||
        launch_result.motion.depth_accumulator_q6 != 0x2000U ||
        launch_result.motion.remaining_timer != 45U ||
        !launch_result.motion.active) {
        (void)snprintf(message, message_size,
                       "TGAI-3 cardinal route-launch vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    motion_out = launch_result.motion;
    memset(&route_step_result, 0, sizeof(route_step_result));
    for (uint16_t route_tick = 0U; route_tick < 45U; ++route_tick) {
        TecmoGameplayCpuSteeringRouteMotionState stepped_motion;
        if (!tecmo_gameplay_cpu_steering_route_step(
                0U, 1U, &motion_out, &stepped_motion,
                &route_step_result)) {
            (void)snprintf(message, message_size,
                           "TGAI-3 cardinal route integration failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        motion_out = stepped_motion;
    }
    if (!route_step_result.finished ||
        route_step_result.horizontal_position != 319U ||
        route_step_result.depth_position != 128U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 cardinal route final coordinate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_input.horizontal_delta = 64;
    launch_input.depth_delta = 64;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        launch_result.direction != 3U || launch_result.duration != 67U ||
        launch_result.motion.horizontal_velocity_q6 != 61 ||
        launch_result.motion.depth_velocity_q6 != 61) {
        (void)snprintf(message, message_size,
                       "TGAI-3 diagonal route-launch vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (uint8_t route_octant = 0U; route_octant < 8U; ++route_octant) {
        launch_input.horizontal_delta = route_octant_delta[route_octant][0U];
        launch_input.depth_delta = route_octant_delta[route_octant][1U];
        if (!tecmo_gameplay_cpu_steering_route_launch(
                &assets, &launch_input, &launch_result) ||
            launch_result.direction != route_octant) {
            (void)snprintf(message, message_size,
                           "TGAI-3 route-launch octant %u failed.",
                           (unsigned)route_octant);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    launch_input.horizontal_delta = -64;
    launch_input.depth_delta = 0;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        launch_result.direction != 1U || launch_result.duration != 45U ||
        launch_result.motion.horizontal_velocity_q6 != -91) {
        (void)snprintf(message, message_size,
                       "TGAI-3 signed route-launch vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_input.horizontal_delta = 1;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        launch_result.duration != 1U ||
        launch_result.motion.horizontal_velocity_q6 != 64) {
        (void)snprintf(message, message_size,
                       "TGAI-3 unit route-launch vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_input.horizontal_delta = 0;
    memset(&launch_result, 0xA5, sizeof(launch_result));
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        launch_result.launched || launch_result.motion.active ||
        launch_result.direction != TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION) {
        (void)snprintf(message, message_size,
                       "TGAI-3 zero route-launch vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&launch_before, 0xA5, sizeof(launch_before));
    launch_result = launch_before;
    launch_input.contract_tag = 0U;
    if (tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        memcmp(&launch_result, &launch_before, sizeof(launch_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route-launch transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_LAUNCH_INPUT_TAG;
    launch_input.horizontal_delta = 1;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&route_step_before, 0xA5, sizeof(route_step_before));
    route_step_result = route_step_before;
    memset(&motion_before, 0x5A, sizeof(motion_before));
    motion_out = motion_before;
    if (tecmo_gameplay_cpu_steering_route_step(
            0U, 2U, &launch_result.motion, &motion_out,
            &route_step_result) ||
        memcmp(&motion_out, &motion_before, sizeof(motion_out)) != 0 ||
        memcmp(&route_step_result, &route_step_before,
               sizeof(route_step_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route-step transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_route_step(
            0U, 0U, &launch_result.motion, &motion_out,
            &route_step_result) || route_step_result.finished ||
        route_step_result.horizontal_position != 257U ||
        route_step_result.depth_position != 128U ||
        motion_out.remaining_timer != 0U || !motion_out.active ||
        !tecmo_gameplay_cpu_steering_route_step(
            0U, 0U, &motion_out, &launch_result.motion,
            &route_step_result) || !route_step_result.finished ||
        route_step_result.horizontal_position != 258U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 low-half delayed completion failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_input.horizontal_delta = 1;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        !tecmo_gameplay_cpu_steering_route_step(
            0U, 1U, &launch_result.motion, &motion_out,
            &route_step_result) || !route_step_result.finished ||
        !tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        !tecmo_gameplay_cpu_steering_route_step(
            5U, 0U, &launch_result.motion, &motion_out,
            &route_step_result) || !route_step_result.finished) {
        (void)snprintf(message, message_size,
                       "TGAI-3 completion-side vectors failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* A zero timer at state-5 entry still integrates once before the
       completion check at $8B55. */
    launch_result.motion.remaining_timer = 0U;
    if (!tecmo_gameplay_cpu_steering_route_step(
            0U, 0U, &launch_result.motion, &motion_out,
            &route_step_result) || !route_step_result.finished ||
        route_step_result.horizontal_position != 257U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 zero-timer integration vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* The complementary high-half/bit-one case delays completion for one
       zero-timer integration. */
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        !tecmo_gameplay_cpu_steering_route_step(
            5U, 1U, &launch_result.motion, &motion_out,
            &route_step_result) || route_step_result.finished ||
        !tecmo_gameplay_cpu_steering_route_step(
            5U, 1U, &motion_out, &launch_result.motion,
            &route_step_result) || !route_step_result.finished) {
        (void)snprintf(message, message_size,
                       "TGAI-3 high-half delayed completion failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* $8AF4-$8B55 is wrapping Q6 integration, with no TGMO/fixed clamp. */
    memset(&motion_before, 0, sizeof(motion_before));
    motion_before.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    motion_before.horizontal_accumulator_q6 = 0xFFC0U;
    motion_before.horizontal_velocity_q6 = 64;
    motion_before.remaining_timer = 1U;
    motion_before.active = true;
    if (!tecmo_gameplay_cpu_steering_route_step(
            0U, 1U, &motion_before, &motion_out, &route_step_result) ||
        route_step_result.horizontal_position != 0U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 positive Q6 wrap vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    motion_before.horizontal_accumulator_q6 = 0U;
    motion_before.horizontal_velocity_q6 = -64;
    if (!tecmo_gameplay_cpu_steering_route_step(
            0U, 1U, &motion_before, &motion_out, &route_step_result) ||
        route_step_result.horizontal_position != 1023U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 negative Q6 wrap vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!opcode15_raw_resolver_self_test(&assets)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-15 raw resolver contract failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!opcode15_parser_anchor_self_test(&assets)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-15 parser/anchor mutations failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0U, &command) ||
        command.opcode != 4U || command.handler_cpu != 0x8FFAU ||
        command.kind != TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ACTOR_TARGET ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x007DU, &command) ||
        command.opcode != 3U || command.arguments[0] != 8U ||
        command.handler_cpu != 0x905EU ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x00D7U, &command) ||
        command.opcode != 2U || command.arguments[0] != 0xB4U ||
        command.arguments[2] != 0x96U ||
        command.handler_cpu != 0x9280U ||
        command.kind !=
            TECMO_GAMEPLAY_CPU_STEERING_COMMAND_ABSOLUTE_TARGET) {
        (void)snprintf(message, message_size,
                       "TGAI-3 command decode vectors failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&before, 0xA5, sizeof(before));
    command = before;
    if (tecmo_gameplay_cpu_steering_decode_command(
            &assets, 1U, &command) ||
        memcmp(&command, &before, sizeof(command)) != 0 ||
        tecmo_gameplay_cpu_steering_decode_command(
            &assets,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE,
            &command) ||
        memcmp(&command, &before, sizeof(command)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 transactional decode rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (assets.formation_start_count !=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT ||
        assets.formation_source_pinned_count !=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT ||
        memcmp(assets.fixed_link,
               (const uint8_t[]){5U,6U,7U,8U,9U,0U,1U,2U,3U,4U},
               TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT) != 0 ||
        !tecmo_gameplay_cpu_steering_formation_select(
            &assets, 45U, &formation_result) ||
        formation_result.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_RESULT_TAG ||
        !formation_result.source_pinned ||
        formation_result.actor_count !=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        (void)snprintf(message, message_size,
                       "TGAI-3 formation lifecycle contract failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&formation_before, 0xA5, sizeof(formation_before));
    formation_result = formation_before;
    if (tecmo_gameplay_cpu_steering_formation_select(
            &assets, 46U, &formation_result) ||
        memcmp(&formation_result, &formation_before,
               sizeof(formation_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 formation boundary rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&route_input, 0, sizeof(route_input));
    memset(&route_before, 0xA5, sizeof(route_before));
    route_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_ROUTE_INPUT_TAG;
    route_input.route_slot = 0U;
    route_input.actor = 0U;
    route_input.control_flags[0U] = 1U;
    route_input.table_index_035A = 1U;
    route_input.age_0094 = 0x27U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        route_result.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_ROUTE_RESULT_TAG ||
        !route_result.wrote_route || route_result.used_long_route ||
        route_result.stream_offset != 0x007DU ||
        route_result.actor_state != 0x04U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 short route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0x80U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route || route_result.stream_offset != 0x00D7U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 long route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0U;
    route_input.flag_0095 = 1U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route flag branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.flag_0095 = 0U;
    route_input.age_0094 = 0x28U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route age branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.control_flags[0U] = 0U;
    route_result = route_before;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        route_result.wrote_route || route_result.stream_offset != 0U ||
        route_result.actor_state != 0U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route no-write branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_result = route_before;
    route_input.contract_tag = 0U;
    if (tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        memcmp(&route_result, &route_before, sizeof(route_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route bad-tag transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_ROUTE_INPUT_TAG;
    route_input.route_slot = TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT;
    route_result = route_before;
    if (tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        memcmp(&route_result, &route_before, sizeof(route_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 route bad-index transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.route_slot = 0U;
    {
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_0 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 0U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_4 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 4U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_7 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 7U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_10 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 10U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_11 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 11U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_12 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 12U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_13 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 13U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_17 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 17U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_18 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 18U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_19 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 19U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_20 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 20U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_21 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 21U);
        if (metadata_0 == NULL || metadata_4 == NULL || metadata_7 == NULL ||
            metadata_10 == NULL || metadata_11 == NULL ||
            metadata_12 == NULL || metadata_13 == NULL ||
            metadata_17 == NULL || metadata_18 == NULL || metadata_19 == NULL ||
            metadata_20 == NULL || metadata_21 == NULL ||
            !metadata_0->exact_bounded || metadata_0->deferred_inputs ||
            !metadata_4->exact_bounded || metadata_4->deferred_inputs ||
            metadata_7->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_BRANCH_PLUS_FIVE ||
            !metadata_10->exact_bounded || metadata_10->deferred_inputs ||
            !metadata_11->exact_bounded || metadata_11->deferred_inputs ||
            metadata_11->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_FIXED_LINK_RELATIVE_POSE ||
            !metadata_12->exact_bounded || metadata_12->deferred_inputs ||
            metadata_12->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_OPCODE12_ZERO_FIVE_OR_TEN ||
            !metadata_13->exact_bounded || metadata_13->deferred_inputs ||
            metadata_13->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET ||
            metadata_17->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER ||
            metadata_18->kind != metadata_17->kind ||
            metadata_19->kind != metadata_17->kind ||
            !metadata_17->exact_bounded || !metadata_18->exact_bounded ||
            !metadata_19->exact_bounded || !metadata_20->exact_bounded ||
            metadata_20->deferred_inputs || metadata_20->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_TARGET ||
            metadata_20->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE ||
            metadata_21->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_FIVE_OR_TEN ||
            metadata_10->native_approximation) {
            (void)snprintf(message, message_size,
                           "TGAI-3 lifecycle effect metadata failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    if (strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE),
               "missing-actor-046e-probe") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF),
               "missing-special-actor-07df") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA),
               "missing-ba-lifecycle") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS),
               "unimplemented-handler") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET),
               "missing-global-target") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE6_CONTEXT),
               "missing-opcode6-context") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE6_CONTROLLED_BRANCH),
               "opcode6-controlled-branch") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE23_CONTEXT),
               "missing-opcode23-context") != 0 ||
        strcmp(tecmo_gameplay_cpu_steering_deferred_reason_name(
                   TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE23_CONTROLLED_BRANCH),
               "opcode23-controlled-branch") != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 deferred-reason names changed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (uint8_t opcode = 0U;
         opcode < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++opcode) {
        if (assets.command_count_by_opcode[opcode] != 0U &&
            !find_lifecycle_opcode_offset(&assets, opcode,
                                          &opcode_offsets[opcode])) {
            (void)snprintf(message, message_size,
                           "TGAI-3 lifecycle opcode coverage failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state) ||
        play_state.matchup_seed[0U] != 2U || play_state.matchup_seed[1U] != 7U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 lifecycle startup state failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (play_state.fixed_link_target[actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            (void)snprintf(message, message_size,
                           "TGAI-3 fixed startup seeds leaked into fixed-link target state.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    memset(&play_input, 0, sizeof(play_input));
    play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    play_input.actor = 0U;
    play_input.step_budget = 4U;
    /* Pure TGAI tests supply explicit source captures.  Production LIVE does
       not set these availability bits without a typed owner. */
    play_input.common_tail_ba_available = true;
    play_input.actor_046e_probe_available = true;
    play_input.opcode21_gate_inputs_available = true;
    play_input.special_actor_07df_available = true;
    play_input.linked_actor_branch_context_available = true;
    play_input.special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));
    play_input.ball_position.x = 255;
    play_input.ball_position.y = 0;

    /* Pin both opcode-11 records and their local following gotos. Exhaustive
       formation scanning below deliberately rejects the earlier, incorrect
       claim that a pinned formation starts at one of these offsets. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0050U, &command) || command.opcode != 11U ||
        command.cpu_address != 0x9F7EU || command.handler_cpu != 0x8C40U ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x005FU, &command) || command.opcode != 11U ||
        command.cpu_address != 0x9F8DU || command.handler_cpu != 0x8C40U ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0055U, &command) || command.opcode != 1U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0064U, &command) || command.opcode != 1U ||
        command.arguments[0U] != 0x5AU || command.arguments[1U] != 0U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-11 canonical records failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    {
        size_t claimed_start_count = 0U;
        static const uint16_t actor_x[7U] = {
            200U,100U,100U,100U,200U,1U,100U};
        static const uint8_t actor_depth[7U] = {
            100U,100U,200U,100U,200U,100U,0U};
        static const uint16_t linked_x[7U] = {
            100U,200U,100U,100U,100U,767U,100U};
        static const uint8_t linked_depth[7U] = {
            100U,100U,100U,200U,100U,100U,223U};
        static const uint8_t expected_pose[7U] = {
            0x0AU,0x0CU,0x0EU,0x10U,0x0AU,0x0CU,0x10U};
        for (size_t formation = 0U;
             formation < assets.formation_start_count; ++formation) {
            for (size_t formation_actor = 0U;
                 formation_actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
                 ++formation_actor) {
                uint16_t start = assets.formation_stream_offsets[formation]
                    [formation_actor];
                if (start == 0x0050U || start == 0x0055U ||
                    start == 0x005FU) {
                    ++claimed_start_count;
                }
            }
        }
        if (claimed_start_count != 0U ||
            assets.formation_source_pinned_count != 46U) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-11 formation nonreachability changed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        for (size_t vector = 0U; vector < 7U; ++vector) {
            TecmoGameplayCourtCoordinate positions_before[
                TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
            if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                    &assets, 0U, &play_state)) {
                (void)snprintf(message, message_size,
                               "TGAI-3 opcode-11 record fixture init failed.");
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_input.actor = 1U;
            play_input.step_budget = 1U;
            memcpy(play_input.actor_position, harness_positions,
                   sizeof(harness_positions));
            play_input.actor_position[1U].x = (int16_t)actor_x[vector];
            play_input.actor_position[1U].y = (int16_t)actor_depth[vector];
            play_input.actor_position[6U].x = (int16_t)linked_x[vector];
            play_input.actor_position[6U].y = (int16_t)linked_depth[vector];
            memcpy(positions_before, play_input.actor_position,
                   sizeof(positions_before));
            /* Intentional canonical-record executor fixture. No formation or
               production cursor owner is inferred from this assignment. */
            play_state.stream_offset[1U] = 0x0050U;
            play_state.direction[1U] = 6U;
            play_state.target_object[1U] = 7U;
            play_state.target_x[1U] = 222;
            play_state.target_depth[1U] = 111;
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input, &play_out,
                    &play_result) || play_result.command.opcode != 11U ||
                play_result.deferred || !play_result.advanced ||
                play_result.next_offset != 0x0055U ||
                play_out.pose[1U] != expected_pose[vector] ||
                play_out.action[1U] != 0x30U ||
                play_out.actor_state[1U] != 0x04U ||
                play_out.direction[1U] != 6U ||
                play_out.target_object[1U] != 7U ||
                play_out.target_x[1U] != 222 ||
                play_out.target_depth[1U] != 111 ||
                !play_result.raw_pose_valid ||
                play_result.raw_pose_low_0442 != expected_pose[vector] ||
                play_result.raw_pose_high_044d != 0x04U ||
                play_result.raw_packed_action_0458 != 0x30U ||
                memcmp(positions_before, play_input.actor_position,
                       sizeof(positions_before)) != 0) {
                (void)snprintf(message, message_size,
                               "TGAI-3 opcode-11 quadrant vector %u failed (pose=%02X raw=%02X high=%02X action=%02X next=%04X defer=%u x=%d/%d y=%d/%d link=%u actor=%u).",
                               (unsigned)vector,
                               (unsigned)play_out.pose[1U],
                               (unsigned)play_result.raw_pose_low_0442,
                               (unsigned)play_result.raw_pose_high_044d,
                               (unsigned)play_out.action[1U],
                               (unsigned)play_result.next_offset,
                               play_result.deferred ? 1U : 0U,
                               (int)play_input.actor_position[1U].x,
                               (int)play_input.actor_position[6U].x,
                               (int)play_input.actor_position[1U].y,
                               (int)play_input.actor_position[6U].y,
                               (unsigned)play_state.fixed_link[1U],
                               (unsigned)play_result.actor);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            if (vector == 0U) {
                play_state = play_out;
                if (!tecmo_gameplay_cpu_steering_play_step(
                        &assets, &play_state, &play_input, &play_out,
                        &play_result) || !play_result.jumped ||
                    play_result.next_offset != 0x0050U) {
                    (void)snprintf(message, message_size,
                                   "TGAI-3 opcode-11 first loop failed.");
                    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                    return false;
                }
            }
        }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-11 second record fixture init failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.actor = 6U;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));
    play_input.actor_position[6U].x = 200;
    play_input.actor_position[1U].x = 100;
    play_input.actor_position[7U].x = 300;
    play_state.stream_offset[6U] = 0x005FU;
    play_state.fixed_link_target[6U] = 7U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 11U || play_result.deferred ||
        play_result.next_offset != 0x0064U || play_out.pose[6U] != 0x0AU) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-11 fixed-link source failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state = play_out;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.jumped || play_result.next_offset != 0x005AU) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-11 second goto failed (next=$%04X).",
                       (unsigned)play_result.next_offset);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    }
    play_input.actor = 0U;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));

    /* The sole opcode-12 record is exact. The imported 46-formation table
       has no `$0069/$006E/$0073` start, contrary to an earlier exploratory
       label, so this is deliberately a direct canonical-record fixture and
       makes no upstream cursor-owner claim. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x006EU, &command) || command.opcode != 12U ||
        command.cpu_address != 0x9F9CU || command.handler_cpu != 0x8E4FU ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        play_formation_graph_reaches_opcode12_record(&assets) ||
        !tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 26U, &play_state)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 canonical record failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.actor = 7U;
    play_input.step_budget = 1U;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));
    play_state.stream_offset[7U] = 0x006EU;
    play_state.actor_state[7U] = 0x04U;
    play_input.opcode12_context_available = true;
    play_input.opcode12_automatic_offense = true;
    play_input.opcode12_actor_eligible = true;
    play_input.opcode12_linked_actor_06cb = 2U;
    play_input.linked_actor_resolved_valid = true;
    play_input.linked_actor = 2U;
    play_input.linked_relative_valid = true;
    play_input.linked_relative_x = 0;
    play_input.linked_relative_depth = 0;
    play_input.actor_position[2U].x = 200;
    play_input.actor_position[2U].y = 100;
    play_input.actor_position[7U].x = 192; /* exact -8 close boundary */
    play_input.actor_position[7U].y = 107; /* exact +7 close boundary */
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.proximity_met ||
        play_result.opcode12_stalled || play_result.next_offset != 0x0073U ||
        !play_result.raw_pose_valid || play_out.target_object[7U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        play_out.target_x[7U] != 0 || play_out.target_depth[7U] != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 close boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Non-close `$92A8` publishes the target and contributes +5 before the
       nonstalled final pose contributes the second +5. */
    play_input.actor_position[7U].x = 191; /* -9 is outside */
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || play_result.proximity_met ||
        play_result.next_offset != 0x0078U ||
        play_out.target_object[7U] != 2U ||
        play_out.target_x[7U] != 200 || play_out.target_depth[7U] != 100 ||
        !play_result.raw_pose_valid) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 nonclose target failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Linked-primary state 5 stalls after the close path (zero advance) or
       after the non-close common tail (+5). */
    play_input.opcode12_linked_actor_06cb = play_state.primary_actor;
    play_state.actor_state[play_state.primary_actor] = 0x05U;
    play_input.actor_position[play_state.primary_actor].x = 200;
    play_input.actor_position[play_state.primary_actor].y = 100;
    play_input.linked_actor = play_state.primary_actor;
    play_input.actor_position[7U].x = 200;
    play_input.actor_position[7U].y = 100;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.opcode12_stalled || !play_result.proximity_met ||
        play_result.next_offset != 0x006EU || !play_result.raw_pose_valid) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 close stall failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.actor_position[7U].x = 208; /* +8 is outside */
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.opcode12_stalled || play_result.proximity_met ||
        play_result.next_offset != 0x0073U || play_result.raw_pose_valid ||
        play_out.target_object[7U] != play_state.primary_actor) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 nonclose stall failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    play_before = play_state;
    play_result_before = play_result;
    play_input.opcode12_context_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE12_CONTEXT ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 missing-context rollback failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode12_context_available = true;
    play_input.opcode12_automatic_offense = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 controlled exclusion failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode12_automatic_offense = true;
    play_input.opcode12_actor_eligible = false;
    play_state.defender_actor = 7U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT ||
        memcmp(&play_out, &play_state, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 defender exclusion failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state = play_before;
    play_input.opcode12_actor_eligible = true;
    for (uint8_t ba = 1U; ba <= 3U; ++ba) {
        play_input.flags_ba = ba;
        if (!tecmo_gameplay_cpu_steering_play_step(
                &assets, &play_state, &play_input, &play_out, &play_result) ||
            !play_result.fetched || !play_result.deferred ||
            play_result.deferred_reason !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE12_UNSAFE_CONTEXT ||
            play_result.advanced || play_result.next_offset != 0x006EU ||
            memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-12 BA=%u rollback failed.",
                           (unsigned)ba);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.flags_ba = 0U;
    play_input.opcode12_linked_actor_06cb =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    play_out = play_before;
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0 ||
        memcmp(&play_result, &play_result_before, sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-12 malformed transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode12_context_available = false;
    play_input.opcode12_automatic_offense = false;
    play_input.opcode12_actor_eligible = false;
    play_input.opcode12_linked_actor_06cb = 0U;
    play_input.linked_actor_resolved_valid = false;
    play_input.linked_relative_valid = false;

    /* The first canonical record is opcode 4 C8=$0A. Canonical Bank06
       $8FFA-$9031 subtracts target object X as a 16-bit value and target
       depth as an 8-bit value sign-extended through $A7. Choose adjacent
       low-byte boundaries so both source borrows are observable. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0000U;
    play_input.actor = 0U;
    play_input.step_budget = 1U;
    play_input.actor_position[0U].x = 256;
    play_input.actor_position[0U].y = 1;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.stream_offset != 0x0000U ||
        play_result.command.cpu_address != 0x9F2EU ||
        play_result.command.opcode != 4U ||
        play_result.command.arguments[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
        play_result.deferred || !play_result.advanced ||
        play_result.target_object !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
        play_out.target_object[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
        play_out.target_x[0U] != 255 || play_out.target_depth[0U] != 0 ||
        play_result.target_horizontal_delta != -1 ||
        play_result.target_depth_delta != -1 ||
        play_result.target_vector_zero ||
        play_out.direction[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 canonical ball target borrow failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* Opcode 4 captures the object coordinate once. Feed its returned
       vector into the pure launch kernel, then mutate the caller's ball
       snapshot. The launched Q6 route and captured play target must remain
       frozen rather than dynamically chase the new coordinate. */
    launch_input.actor_position = play_input.actor_position[0U];
    launch_input.horizontal_delta = play_result.target_horizontal_delta;
    launch_input.depth_delta = play_result.target_depth_delta;
    launch_input.condition_7c48 = 100U;
    launch_input.movement_value_06e7 = 20U;
    if (!tecmo_gameplay_cpu_steering_route_launch(
            &assets, &launch_input, &launch_result) ||
        !launch_result.launched || launch_result.duration != 1U ||
        launch_result.motion.horizontal_velocity_q6 != -64 ||
        launch_result.motion.depth_velocity_q6 != -64) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 captured route composition failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    launch_before = launch_result;
    play_input.ball_position.x = 700;
    play_input.ball_position.y = 200;
    if (play_out.target_x[0U] != 255 || play_out.target_depth[0U] != 0 ||
        memcmp(&launch_result, &launch_before, sizeof(launch_result)) != 0 ||
        !tecmo_gameplay_cpu_steering_route_step(
            0U, 1U, &launch_result.motion, &motion_out,
            &route_step_result) ||
        route_step_result.horizontal_position != 255U ||
        route_step_result.depth_position != 0U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 frozen-target route failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.ball_position.x = 255;
    play_input.ball_position.y = 0;
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0000U;
    play_state.actor_state[0U] = 0x0BU;
    /* A non-sentinel prior direction proves the $9025 zero-vector branch
       preserves the existing $0463 value instead of merely retaining the
       initialized sentinel. */
    play_state.direction[0U] = 3U;
    play_input.actor_position[0U] = play_input.ball_position;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 4U || play_result.deferred ||
        !play_result.target_vector_zero ||
        play_result.target_horizontal_delta != 0 ||
        play_result.target_depth_delta != 0 ||
        play_out.actor_state[0U] != 0x04U ||
        play_out.direction[0U] != 3U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 zero-vector guard failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));

    /* The only canonical opcode-7 records pin the selected-primary subset:
       C8=$0A indexes object slot 10 `$0478`, and C9=0 selects current+5. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x013BU, &command) || command.opcode != 7U ||
        command.cpu_address != 0xA069U ||
        memcmp(command.arguments,
               (const uint8_t[4U]){0x0AU,0x00U,0x36U,0x01U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0172U, &command) || command.opcode != 7U ||
        command.cpu_address != 0xA0A0U ||
        memcmp(command.arguments,
               (const uint8_t[4U]){0x0AU,0x00U,0x68U,0x01U}, 4U) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 canonical records failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Bank06 $8F12 cannot treat a zeroed native array as $046E,C8. An
       unavailable probe reports its owner and leaves the fetched lifecycle
       byte-for-byte unchanged. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x013BU;
    play_before = play_state;
    play_input.actor_046e_probe_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 7U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE ||
        play_result.advanced || play_result.next_offset != 0x013BU ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 unavailable probe transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.actor_046e_probe_available = true;

    /* Opcode 7 uses the original eleven-entry $046E probe. Equal C9 takes
       current+5; mismatch stores CA/CB and then takes alternate+5. */
    play_state.stream_offset[0U] = 0x013BU;
    play_input.actor_046e_probe[0x0AU] = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 7U || play_result.jumped ||
        play_result.next_offset != 0x0140U ||
        play_out.stream_offset[0U] != 0x0140U ||
        play_out.actor_state[0U] != 0x04U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 equal-probe golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0172U;
    play_input.actor_046e_probe[0x0AU] = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 7U || play_result.jumped ||
        play_result.next_offset != 0x0177U ||
        play_out.stream_offset[0U] != 0x0177U ||
        play_out.actor_state[0U] != 0x04U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 second equal-probe golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x013BU;
    play_input.actor_046e_probe[0x0AU] = 1U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 7U || !play_result.jumped ||
        play_result.jump_offset != 0x0136U ||
        play_result.next_offset != 0x013BU || play_result.advanced ||
        play_out.stream_offset[0U] != 0x013BU) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 mismatch-probe golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0172U;
    play_input.actor_046e_probe[0x0AU] = 1U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 7U || !play_result.jumped ||
        play_result.jump_offset != 0x0168U ||
        play_result.next_offset != 0x016DU || !play_result.advanced ||
        play_out.stream_offset[0U] != 0x016DU ||
        play_out.actor_state[0U] != 0x04U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-7 second mismatch rewind failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Opcode 5 begins the canonical pass stream. Pin its sole record and the
       full orientation mirror, then prove canonical direction 2 is invariant
       while unowned pose input remains untouched. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x017CU, &command) || command.opcode != 5U ||
        command.cpu_address != 0xA0AAU ||
        memcmp(command.arguments,
               (const uint8_t[4U]){2U,0U,0U,0U}, 4U) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-5 canonical record failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (uint8_t source_direction = 0U; source_direction < 8U;
         ++source_direction) {
        static const uint8_t mirror[8U] = {1U,0U,2U,4U,3U,5U,7U,6U};
        uint8_t mirrored_direction;
        if (!play_opcode5_direction(
                source_direction, 0U, &mirrored_direction) ||
            mirrored_direction != source_direction ||
            !play_opcode5_direction(
                source_direction, 1U, &mirrored_direction) ||
            mirrored_direction != mirror[source_direction]) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-5 direction mirror failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    for (uint8_t orientation = 0U; orientation < 2U; ++orientation) {
        if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                &assets, 0U, &play_state)) {
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        play_state.stream_offset[0U] = 0x017CU;
        play_state.actor_state[0U] = 0x04U;
        play_state.pose[0U] = 0x5AU;
        play_state.action[0U] = 0xA5U;
        play_input.orientation_035a = orientation;
        if (!tecmo_gameplay_cpu_steering_play_step(
                &assets, &play_state, &play_input, &play_out, &play_result) ||
            play_result.deferred || !play_result.advanced ||
            play_result.next_offset != 0x0181U ||
            play_out.actor_state[0U] != 0x04U ||
            play_out.action_state_046e[0U] != 0x18U ||
            play_out.direction[0U] != 2U ||
            play_out.pose[0U] != 0x5AU || play_out.action[0U] != 0xA5U) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-5 canonical execution failed "
                           "(o=%u defer=%u adv=%u next=%u state=%u a046e=%u "
                           "dir=%u pose=%u action=%u).",
                           (unsigned int)orientation,
                           play_result.deferred ? 1U : 0U,
                           play_result.advanced ? 1U : 0U,
                           (unsigned int)play_result.next_offset,
                           (unsigned int)play_out.actor_state[0U],
                           (unsigned int)play_out.action_state_046e[0U],
                           (unsigned int)play_out.direction[0U],
                           (unsigned int)play_out.pose[0U],
                           (unsigned int)play_out.action[0U]);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.orientation_035a = 0U;

    /* Opcode 8 has eight exact post-catch boundary records. The selected
       holder snapshot is immutable for this play step; no raw `$0588`
       availability is required for the transport/state result. */
    {
        static const uint16_t offsets[8U] = {
            0x0B68U,0x0B77U,0x0B86U,0x0B95U,
            0x0BA4U,0x0BB3U,0x0BC2U,0x0BD1U
        };
        static const uint16_t targets[8U] = {
            0x025DU,0x029EU,0x02DAU,0x030CU,
            0x025DU,0x029EU,0x02DAU,0x030CU
        };
        static const uint8_t orientations[4U] = {0U,0U,1U,1U};
        static const int16_t ball_x[4U] = {
            0x013F,0x0140,0x01BF,0x01C0
        };
        static const bool redirects[4U] = {true,false,false,true};
        for (size_t record = 0U; record < 8U; ++record) {
            if (!tecmo_gameplay_cpu_steering_decode_command(
                    &assets, offsets[record], &command) ||
                command.opcode != 8U ||
                command.cpu_address !=
                    (uint16_t)(0x9F2EU + offsets[record]) ||
                command.arguments[0U] != (uint8_t)targets[record] ||
                command.arguments[1U] !=
                    (uint8_t)(targets[record] >> 8U) ||
                command.arguments[2U] != 0U ||
                command.arguments[3U] != 0U) {
                (void)snprintf(message, message_size,
                               "TGAI-3 opcode-8 exact record %u failed.",
                               (unsigned)record);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                    &assets, 0U, &play_state)) {
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_state.stream_offset[0U] = offsets[record];
            play_state.actor_state[0U] = 0x09U;
            play_input.orientation_035a = 0U;
            play_input.ball_position.x = 0x013F;
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input,
                    &play_out, &play_result) || play_result.deferred ||
                !play_result.jumped ||
                play_result.jump_offset != targets[record] ||
                play_out.stream_offset[0U] != targets[record] ||
                play_out.actor_state[0U] != 0x04U) {
                (void)snprintf(message, message_size,
                               "TGAI-3 opcode-8 redirect target %u failed.",
                               (unsigned)record);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
        }
        if (play_stream_offset_valid(0x025EU) ||
            play_stream_offset_valid(CPU_STEERING_COMMAND_STREAM_SIZE)) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-8 redirect validation failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        for (size_t edge = 0U; edge < 4U; ++edge) {
            if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                    &assets, 0U, &play_state)) {
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_state.stream_offset[0U] = 0x0B68U;
            play_state.actor_state[0U] = 0x09U;
            play_input.orientation_035a = orientations[edge];
            play_input.ball_position.x = ball_x[edge];
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input,
                    &play_out, &play_result) || play_result.deferred ||
                play_out.actor_state[0U] != 0x04U ||
                play_result.jumped != redirects[edge] ||
                play_out.stream_offset[0U] !=
                    (redirects[edge] ? 0x025DU : 0x0B6DU) ||
                play_result.next_offset !=
                    play_out.stream_offset[0U]) {
                (void)snprintf(message, message_size,
                               "TGAI-3 opcode-8 boundary edge %u failed.",
                               (unsigned)edge);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
        }
        play_input.orientation_035a = 0U;
    }

    /* Opcode 23 is the sole source predecessor. The selected/uncontrolled
       branch advances without sampling `$6A`, defender depth, or direction. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x018BU, &command) || command.opcode != 23U ||
        command.cpu_address != 0xA0B9U ||
        memcmp(command.arguments,
               (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-23 canonical record failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x018BU;
    play_state.actor_state[0U] = 0x04U;
    play_state.direction[0U] = 3U;
    play_before = play_state;
    play_input.opcode23_context_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE23_CONTEXT ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-23 missing context transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode23_context_available = true;
    play_input.opcode23_uncontrolled = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE23_CONTROLLED_BRANCH ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-23 controlled branch transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode23_uncontrolled = true;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.advanced || play_result.jumped ||
        play_result.next_offset != 0x0190U ||
        play_out.stream_offset[0U] != 0x0190U ||
        play_out.actor_state[0U] != 0x04U ||
        play_out.direction[0U] != 3U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-23 uncontrolled subset failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode23_context_available = false;
    play_input.opcode23_uncontrolled = false;

    /* Opcode 6 has one exact record. Its controlled `$8F4D->$89DB` branch is
       outside this executor; the automatic `$8F2D-$8F4C` subset retains the
       cursor/state and writes only typed action/object-state results. */
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0190U, &command) || command.opcode != 6U ||
        command.cpu_address != 0xA0BEU ||
        memcmp(command.arguments,
               (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-6 canonical record failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0190U;
    play_state.actor_state[0U] = 0x04U;
    play_before = play_state;
    play_input.opcode6_context_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE6_CONTEXT ||
        play_result.advanced ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-6 missing context transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode6_context_available = true;
    play_input.opcode6_automatic = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_OPCODE6_CONTROLLED_BRANCH ||
        play_result.advanced ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-6 controlled branch transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode6_automatic = true;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || play_result.advanced || play_result.jumped ||
        play_result.next_offset != 0x0190U ||
        play_out.stream_offset[0U] != 0x0190U ||
        play_out.actor_state[0U] != 0x04U ||
        play_out.action_state_046e[0U] != 0x10U ||
        !play_result.opcode6_action10_written ||
        !play_result.opcode6_object10_state_written ||
        play_result.opcode6_object10_state != 0x13U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-6 automatic subset failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode6_context_available = false;
    play_input.opcode6_automatic = false;

    /* A non-goto handler consumes one native tick even when the caller gives
       it a larger budget. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[0U];
    play_input.actor_046e_probe[0x0AU] = 0U;
    play_input.step_budget = 4U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.steps_executed != 1U || play_result.command.opcode != 0U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 one-handler-per-tick golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Opcode 1 is the only handler that chains into another record in this
       tick. The canonical $0005 record jumps to $0000, whose opcode 4 then
       consumes one record and returns to $0005. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0005U;
    play_input.step_budget = 2U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.steps_executed != 2U ||
        play_result.command.opcode != 4U || !play_result.jumped ||
        play_result.jump_offset != 0x0000U ||
        play_result.next_offset != 0x0005U ||
        play_result.budget_exhausted || play_result.deferred ||
        play_result.target_object !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
        play_out.stream_offset[0U] != 0x0005U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 goto-chain two-step golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0005U;
    play_input.step_budget = 1U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.steps_executed != 1U ||
        play_result.command.opcode != 1U || !play_result.jumped ||
        play_result.jump_offset != 0x0000U ||
        play_result.next_offset != 0x0000U ||
        !play_result.budget_exhausted || play_out.stream_offset[0U] != 0x0000U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 goto-chain budget golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Bank06 $8CD0 first compares the exceptional $07DF actor. Without that
       typed selector, even a supplied relative vector has no faithful route. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[10U];
    play_input.step_budget = 4U;
    play_input.flags_ba = 0U;
    play_before = play_state;
    play_input.special_actor_07df_available = false;
    play_input.linked_relative_valid = true;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 10U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[10U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-10 unavailable $07DF transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.special_actor_07df_available = true;
    play_input.linked_actor_branch_context_available = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 10U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_ACTOR_BRANCH_CONTEXT ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[10U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-10 unavailable link branch failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.linked_actor_branch_context_available = true;
    play_input.linked_relative_valid = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 10U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[10U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-10 unavailable helper transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    play_input.linked_relative_valid = true;
    play_input.common_tail_ba_available = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 10U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[10U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-10 unavailable BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* Opcode 2 reaches the same $92CA common tail and must preserve its
       source record when the external $BA lifecycle is unavailable. */
    play_state.stream_offset[0U] = opcode_offsets[2U];
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 2U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[2U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-2 unavailable BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.common_tail_ba_available = true;
    play_input.flags_ba = 0U;
    /* A typed zero is distinct from the unavailable-input case above:
       Bank06 $92CA-$92D0 reaches its local $8FD9 five-byte increment only
       when ($BA & 3) is zero. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[2U];
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 2U || play_result.deferred ||
        play_result.deferred_reason != TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
        !play_result.advanced ||
        play_result.next_offset != (uint16_t)(opcode_offsets[2U] + 5U) ||
        play_out.stream_offset[0U] !=
            (uint16_t)(opcode_offsets[2U] + 5U)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-2 typed-zero BA advance failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* With the bounded helper workspace present, opcode 10 synthesizes the
       linked target with 16-bit wrap and admits exactly [-8,+7] on each axis. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[10U];
    play_state.fixed_link_target[0U] = 5U;
    play_input.actor = 0U;
    play_input.step_budget = 1U;
    play_input.linked_actor_resolved_valid = true;
    play_input.linked_actor = 7U;
    play_input.linked_relative_valid = true;
    play_input.linked_relative_x = 3;
    play_input.linked_relative_depth = -2;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || play_result.target_object != 7U ||
        play_result.target_x !=
            play_input.actor_position[7U].x + 3 ||
        play_result.target_depth !=
            play_input.actor_position[7U].y - 2) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-10 resolved fixed-link failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.linked_actor_resolved_valid = false;
    play_input.linked_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    for (int delta = -8; delta <= 8; ++delta) {
        if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                &assets, 0U, &play_state)) {
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        play_state.stream_offset[0U] = opcode_offsets[10U];
        play_state.fixed_link_target[0U] = 5U;
        play_input.actor = 0U;
        play_input.step_budget = 1U;
        play_input.linked_relative_valid = true;
        play_input.linked_relative_x = (int16_t)(
            play_input.actor_position[0U].x -
            play_input.actor_position[5U].x - delta);
        play_input.linked_relative_depth = (int16_t)(
            play_input.actor_position[0U].y -
            play_input.actor_position[5U].y);
        if (!tecmo_gameplay_cpu_steering_play_step(
                &assets, &play_state, &play_input, &play_out, &play_result) ||
            play_result.deferred ||
            play_result.proximity_met != (delta >= -8 && delta <= 7) ||
            play_result.advanced != (delta >= -8 && delta <= 7)) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-10 proximity boundary failed at %d.",
                           delta);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.linked_relative_valid = false;

    /* $9085 cannot resolve the $0309 target route without the paired
       $036E/$0370 workspace, and its $92CA tail separately needs $BA. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[16U];
    play_before = play_state;
    play_input.pointer_workspace_valid = false;
    play_input.common_tail_ba_available = true;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 16U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_POINTER_WORKSPACE ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[16U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-16 unavailable pointer transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.pointer_workspace_valid = true;
    play_input.common_tail_ba_available = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 16U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[16U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-16 unavailable BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.common_tail_ba_available = true;

    /* Both opcode-16 corpus records point at $0309. Exercise the exact
       depth +/-10 and orientation-selected horizontal +/-16 branches. */
    for (uint8_t branch = 0U; branch < 4U; ++branch) {
        if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                &assets, 0U, &play_state)) {
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        play_state.stream_offset[0U] = opcode_offsets[16U];
        play_state.defender_actor = 9U;
        play_input.pointer_workspace_valid = true;
        play_input.orientation_035a = (uint8_t)(branch & 1U);
        play_input.workspace_036e = branch < 2U ? 2U : 0U;
        play_input.workspace_0370 = 1U;
        play_input.actor_position[9U].y = branch == 1U ? 0x94 : 0x93;
        if (!tecmo_gameplay_cpu_steering_play_step(
                &assets, &play_state, &play_input, &play_out, &play_result) ||
            play_result.deferred || play_result.command.opcode != 16U ||
            play_out.target_object[0U] != 9U ||
            (branch < 2U && play_out.target_depth[0U] !=
                (branch == 1U ? 0x8A : 0x9D)) ||
            (branch >= 2U && play_out.target_x[0U] !=
                (int16_t)(play_input.actor_position[9U].x +
                    ((branch & 1U) != 0U ? -16 : 16)))) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-16 adjustment branch %u failed.",
                           (unsigned)branch);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.pointer_workspace_valid = false;
    play_input.orientation_035a = 0U;

    /* State index 6 dispatches through $82B6/$82C4 to canonical
       $9053-$905D.  DEC $04E7,X wraps 0->$FF, and only a decrement result of
       zero writes state 4.  Every state-6 tick returns without fetching or
       advancing the already-retained command cursor. */
    {
        static const uint8_t wait_before[] = {0U, 2U, 1U};
        static const uint8_t wait_after[] = {0xFFU, 1U, 0U};
        static const uint8_t state_after[] = {0x06U, 0x06U, 0x04U};
        for (size_t vector = 0U;
             vector < sizeof(wait_before) / sizeof(wait_before[0]); ++vector) {
            if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                    &assets, 0U, &play_state)) {
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_state.stream_offset[0U] = 0x009BU;
            play_state.wait_counter[0U] = wait_before[vector];
            play_state.actor_state[0U] = 0x06U;
            play_before = play_state;
            play_input.actor = 0U;
            play_input.step_budget = 4U;
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input, &play_out,
                    &play_result) ||
                play_result.fetched || play_result.advanced ||
                play_result.steps_executed != 1U ||
                play_result.waiting != (wait_after[vector] != 0U) ||
                play_result.previous_offset != 0x009BU ||
                play_result.next_offset != 0x009BU ||
                play_out.wait_counter[0U] != wait_after[vector] ||
                play_out.actor_state[0U] != state_after[vector] ||
                play_out.stream_offset[0U] !=
                    play_before.stream_offset[0U]) {
                (void)snprintf(
                    message, message_size,
                    "TGAI-3 state-6 DEC vector %u failed.",
                    (unsigned)vector);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
        }
    }

    /* A stale nonzero $04E7 byte outside state 6 is ignored.  Exercise that
       rule on two exact opcode-3 records: the handler seeds its new wait byte,
       writes state 6, and advances the cursor once before later $9053 ticks
       retain the following record. */
    {
        static const uint16_t wait_record[] = {0x0096U, 0x062CU};
        static const uint16_t next_record[] = {0x009BU, 0x0631U};
        static const uint8_t seeded_wait[] = {10U, 30U};
        for (size_t vector = 0U;
             vector < sizeof(wait_record) / sizeof(wait_record[0]); ++vector) {
            if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                    &assets, 0U, &play_state)) {
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_state.stream_offset[0U] = wait_record[vector];
            play_state.actor_state[0U] = 0x04U;
            play_state.wait_counter[0U] = 0xA5U;
            play_input.actor = 0U;
            play_input.step_budget = 1U;
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input, &play_out,
                    &play_result) ||
                !play_result.fetched || !play_result.advanced ||
                play_result.command.opcode != 3U ||
                play_result.command.stream_offset != wait_record[vector] ||
                play_result.next_offset != next_record[vector] ||
                play_out.stream_offset[0U] != next_record[vector] ||
                play_out.actor_state[0U] != 0x06U ||
                play_out.wait_counter[0U] != seeded_wait[vector]) {
                (void)snprintf(
                    message, message_size,
                    "TGAI-3 opcode-3 wait seed vector %u failed.",
                    (unsigned)vector);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
            play_state = play_out;
            if (!tecmo_gameplay_cpu_steering_play_step(
                    &assets, &play_state, &play_input, &play_out,
                    &play_result) ||
                play_result.fetched || play_result.advanced ||
                play_result.next_offset != next_record[vector] ||
                play_out.stream_offset[0U] != next_record[vector] ||
                play_out.actor_state[0U] != 0x06U ||
                play_out.wait_counter[0U] !=
                    (uint8_t)(seeded_wait[vector] - 1U)) {
                (void)snprintf(
                    message, message_size,
                    "TGAI-3 opcode-3 retained wait vector %u failed.",
                    (unsigned)vector);
                tecmo_gameplay_cpu_steering_assets_destroy(&assets);
                return false;
            }
        }
    }

    /* Opcode 14 uses exact $04B0 bit $10 and seeds other actors at $0023. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[14U];
    memset(play_input.actor_04b0, 0, sizeof(play_input.actor_04b0));
    play_input.actor_04b0[1U] = 0x10U;
    play_input.actor_04b0[2U] = 0x01U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 14U ||
        play_out.stream_offset[1U] != 0x0023U ||
        play_out.actor_state[1U] != 0x04U ||
        play_out.stream_offset[2U] == 0x0023U ||
        play_out.actor_state[0U] != 0x04U ||
        play_out.stream_offset[0U] != (uint16_t)(opcode_offsets[14U] + 5U)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-14 $04B0 golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Opcodes 17/18/19 are the same exact aggregation/barrier handler. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[17U];
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 17U ||
        play_out.actor_state[0U] != 0x0BU ||
        play_out.aggregation_06e0 != play_result.command.arguments[0U] ||
        play_out.aggregation_06df != 1U ||
        play_out.aggregation_06e1 != 0x01U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 aggregation barrier golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* The $8BF6 gate is four caller-owned bytes, not a shot-clock-derived
       substitute. Its unavailable state must leave this record untouched. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[21U];
    play_before = play_state;
    play_input.opcode21_gate_inputs_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 21U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE21_GATE_INPUTS ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[21U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-21 unavailable gate transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.opcode21_gate_inputs_available = true;

    /* Opcode 21's exact gate selects one or two record advances. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[21U];
    play_input.state_058a = 0U;
    play_input.state_0357 = 0U;
    play_input.state_0358 = 0U;
    play_input.flags_007e = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.next_offset != (uint16_t)(opcode_offsets[21U] + 5U)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-21 one-record golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[21U];
    play_input.state_058a = 4U;
    play_input.state_0357 = 0U;
    play_input.state_0358 = 4U;
    play_input.flags_007e = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.next_offset != (uint16_t)(opcode_offsets[21U] + 10U)) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-21 two-record golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Opcode 22 replaces $0791/$0792 and retains/ORs the existing $0790. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[22U];
    play_state.global_0790 = 0x80U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 22U ||
        play_out.global_0791 != play_result.command.arguments[0U] ||
        play_out.global_0792 != play_result.command.arguments[1U] ||
        play_out.global_0790 !=
            (uint8_t)(0x80U | play_result.command.arguments[2U])) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-22 mask retention golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* $92CA cannot consume a literal zero in place of the caller's $BA
       lifecycle. The bounded opcode-0 write therefore defers transactionally
       until a harness supplies an explicit valid value. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[0U];
    play_before = play_state;
    play_input.common_tail_ba_available = false;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 0U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[0U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-0 unavailable BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.common_tail_ba_available = true;

    /* The corpus contains exactly the two source-proven opcode-13 records at
       Bank04 $9F5B/$9F6F. Both carry zero arguments because $9125 consumes
       the external $038D-$0390 latch, then reaches the common BA tail. */
    if (opcode_offsets[13U] != 0x002DU ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x002DU, &command) || command.opcode != 13U ||
        command.cpu_address != 0x9F5BU ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0041U, &command) || command.opcode != 13U ||
        command.cpu_address != 0x9F6FU ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0032U, &command) || command.opcode != 14U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0037U, &command) || command.opcode != 15U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0046U, &command) || command.opcode != 14U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x004BU, &command) || command.opcode != 15U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 canonical records failed "
                       "(first=%u last_cpu=$%04X last_opcode=%u).",
                       (unsigned int)opcode_offsets[13U],
                       (unsigned int)command.cpu_address,
                       (unsigned int)command.opcode);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* `$0041` is decodable but not production-reachable from the imported
       formation scheduler. Exhaustive source-pinned starts contain no direct
       seed, and the over-approximating command graph still cannot reach it
       once opcode 15's exact no-advance return removes `$0037->$003C`. */
    {
        size_t count_0041 = 0U;
        for (size_t formation = 0U;
             formation < assets.formation_source_pinned_count; ++formation) {
            for (size_t formation_actor = 0U;
                 formation_actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
                 ++formation_actor) {
                if (assets.formation_stream_offsets[formation]
                        [formation_actor] == 0x0041U) {
                    ++count_0041;
                }
            }
        }
        if (count_0041 != 0U ||
            play_formation_graph_reaches_opcode13_record_0041(&assets)) {
            (void)snprintf(message, message_size,
                           "TGAI-3 opcode-13 $0041 bounded "
                           "nonreachability changed (%u).",
                           (unsigned int)count_0041);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }

    /* Missing latch input fails before the later BA gate and cannot mutate
       target, stream, or lifecycle state. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[13U];
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 13U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[13U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 missing target transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* A valid latch still cannot substitute for the common-tail BA owner. */
    play_input.global_target_available = true;
    play_input.global_target.x = 320;
    play_input.global_target.depth = 120;
    play_input.common_tail_ba_available = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.deferred || play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        play_result.advanced ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 missing BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Zero vector reaches the exact common tail, advances under BA&3==0,
       writes the absolute no-object target, and preserves prior direction. */
    play_input.common_tail_ba_available = true;
    play_input.flags_ba = 0U;
    play_input.global_target.x =
        (uint16_t)play_input.actor_position[0U].x;
    play_input.global_target.depth =
        (uint16_t)(uint8_t)play_input.actor_position[0U].y;
    play_state.direction[0U] = 3U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.advanced ||
        play_result.next_offset != 0x0032U ||
        play_result.target_object != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        !play_result.raw_target_valid ||
        play_result.raw_target_x != play_input.global_target.x ||
        play_result.raw_target_depth != play_input.global_target.depth ||
        (uint16_t)play_result.target_x != play_input.global_target.x ||
        (uint16_t)play_result.target_depth != play_input.global_target.depth ||
        !play_result.target_vector_zero ||
        play_result.target_horizontal_delta != 0 ||
        play_result.target_depth_delta != 0 ||
        play_out.direction[0U] != 3U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 zero-vector target failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* The second record proves both latch high bytes remain live. `$0390=1`
       makes depth `$0100-$00C8=+$0038`; raw X wraps `$FF00-$0100=$FE00`.
       Neither raw target word is required to be a court coordinate. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0041U;
    play_input.actor_position[0U].x = 0x0100;
    play_input.actor_position[0U].y = 200;
    play_input.global_target.x = 0xFF00U;
    play_input.global_target.depth = 0x0100U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.advanced ||
        play_result.next_offset != 0x0046U ||
        play_result.target_object != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        !play_result.raw_target_valid ||
        play_result.raw_target_x != 0xFF00U ||
        play_result.raw_target_depth != 0x0100U ||
        (uint16_t)play_result.target_x != 0xFF00U ||
        (uint16_t)play_result.target_depth != 0x0100U ||
        (uint16_t)play_out.target_x[0U] != 0xFF00U ||
        (uint16_t)play_out.target_depth[0U] != 0x0100U ||
        play_result.target_horizontal_delta != -512 ||
        play_result.target_depth_delta != 56 ||
        play_result.target_vector_zero) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 raw16 subtraction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* BA&3 nonzero retains the source record after the exact target write. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x002DU;
    play_input.flags_ba = 1U;
    play_input.global_target.x = 111;
    play_input.global_target.depth = 99;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || play_result.advanced ||
        play_result.next_offset != 0x002DU ||
        play_out.target_object[0U] != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        play_out.target_x[0U] != 111 || play_out.target_depth[0U] != 99) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 BA retention failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* `$0390=$FF` is retained verbatim and participates in the complete
       16-bit subtraction: `$FF10-$0020=$FEF0` (signed -272). */
    play_state.stream_offset[0U] = 0x002DU;
    play_input.flags_ba = 0U;
    play_input.actor_position[0U].x = 16;
    play_input.actor_position[0U].y = 0x20;
    play_input.global_target.x = 16U;
    play_input.global_target.depth = 0xFF10U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred ||
        play_result.raw_target_depth != 0xFF10U ||
        (uint16_t)play_out.target_depth[0U] != 0xFF10U ||
        play_result.target_horizontal_delta != 0 ||
        play_result.target_depth_delta != -272 ||
        play_result.target_vector_zero) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-13 high-depth subtraction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.flags_ba = 0U;
    play_input.global_target_available = false;

    /* Orientation $035A and BA&3 gate target transport for opcode 0. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[0U];
    play_input.orientation_035a = 0U;
    play_input.flags_ba = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 0U || !play_result.advanced) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-0 forward golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_before = play_out;
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[0U];
    play_input.orientation_035a = 1U;
    play_input.flags_ba = 1U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 0U || play_result.advanced ||
        play_out.stream_offset[0U] != opcode_offsets[0U] ||
        play_out.target_x[0U] != play_clamp_int16(
            (int32_t)play_input.actor_position[0U].x -
            play_signed_u16(play_result.command.arguments[0U],
                            play_result.command.arguments[1U])) ||
        play_before.target_x[0U] == play_out.target_x[0U]) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-0 orientation/BA golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* `$9032-$9052` has exactly two zero-argument opcode-20 records, each
       followed by a goto `$0000`. */
    if (opcode_offsets[20U] != 0x000FU ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x000FU, &command) || command.opcode != 20U ||
        command.cpu_address != 0x9F3DU ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0014U, &command) || command.opcode != 1U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x0019U, &command) || command.opcode != 20U ||
        command.cpu_address != 0x9F47U ||
        memcmp(command.arguments, (const uint8_t[4U]){0U,0U,0U,0U}, 4U) != 0 ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &assets, 0x001EU, &command) || command.opcode != 1U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 canonical records failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x000FU;
    play_state.target_object[0U] = 7U;
    play_state.target_x[0U] = 77;
    play_state.target_depth[0U] = 88;
    play_state.direction[0U] = 3U;
    play_input.actor = 0U;
    play_input.step_budget = 1U;
    play_input.global_target_available = false;
    play_before = play_state;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 20U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_GLOBAL_TARGET ||
        play_result.advanced ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 missing-latch transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* Zero vector writes only source state 4, preserves direction and every
       target plane, advances +5, then the next tick's goto returns to zero. */
    play_input.global_target_available = true;
    play_input.flags_ba = 3U; /* Proves opcode 20 does not read BA. */
    play_input.global_target.x =
        (uint16_t)play_input.actor_position[0U].x;
    play_input.global_target.depth =
        (uint16_t)(uint8_t)play_input.actor_position[0U].y;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.advanced ||
        play_result.next_offset != 0x0014U ||
        !play_result.raw_target_valid || !play_result.target_vector_zero ||
        play_result.target_horizontal_delta != 0 ||
        play_result.target_depth_delta != 0 ||
        play_out.actor_state[0U] != 0x04U || play_out.direction[0U] != 3U ||
        play_out.target_object[0U] != 7U || play_out.target_x[0U] != 77 ||
        play_out.target_depth[0U] != 88) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 zero-vector bounded effect failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state = play_out;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        !play_result.jumped || play_result.next_offset != 0x0000U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 following goto failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x000FU;
    play_state.target_x[0U] = 91;
    play_input.actor_position[0U].x = 0x0100;
    play_input.actor_position[0U].y = 0x00C8;
    play_input.global_target.x = 0x0100U;
    play_input.global_target.depth = 0x0100U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || play_result.target_horizontal_delta != 0 ||
        play_result.target_depth_delta != 56 ||
        play_result.raw_target_depth != 0x0100U ||
        play_out.direction[0U] != 2U || play_out.target_x[0U] != 91) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 high-depth direction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    /* The second record proves both raw words and wrapping subtraction remain
       live without mutating the pre-existing actor target. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = 0x0019U;
    play_state.target_object[0U] = 6U;
    play_state.target_x[0U] = -123;
    play_state.target_depth[0U] = 45;
    play_input.actor_position[0U].x = 0x0100;
    play_input.actor_position[0U].y = 0x20;
    play_input.global_target.x = 0xFF00U;
    play_input.global_target.depth = 0xFF10U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.deferred || !play_result.advanced ||
        play_result.next_offset != 0x001EU ||
        !play_result.raw_target_valid ||
        play_result.raw_target_x != 0xFF00U ||
        play_result.raw_target_depth != 0xFF10U ||
        play_result.target_horizontal_delta != -512 ||
        play_result.target_depth_delta != -272 ||
        play_result.target_vector_zero || play_out.direction[0U] != 7U ||
        play_out.target_object[0U] != 6U || play_out.target_x[0U] != -123 ||
        play_out.target_depth[0U] != 45) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-20 raw16/direction effect failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.global_target_available = false;
    play_input.flags_ba = 0U;

    /* LIVE's existing opcode-15 boundary remains explicitly deferred. The
       raw resolver above is a source-contract harness only; no caller may
       accidentally use it to mutate this typed play state without the raw
       `$0499/$007E/$06D5/$06D6/$0479/$0442/$044D/$059E` owners. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET;
    play_state.primary_actor = 4U;
    play_state.defender_actor = 9U;
    play_state.actor_state[0U] = 0x0BU;
    play_state.action_state_046e[0U] = 0xC3U;
    play_before = play_state;
    play_input.actor = 0U;
    play_input.step_budget = 1U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 15U || !play_result.deferred ||
        play_result.deferred_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE ||
        play_result.advanced ||
        play_result.next_offset !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET ||
        play_out.stream_offset[0U] != play_before.stream_offset[0U] ||
        play_out.primary_actor != play_before.primary_actor ||
        play_out.defender_actor != play_before.defender_actor ||
        play_out.actor_state[0U] != play_before.actor_state[0U] ||
        play_out.action_state_046e[0U] !=
            play_before.action_state_046e[0U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-15 LIVE deferred boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Every invalid/aliased play contract leaves both outputs untouched. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[0U];
    play_input.orientation_035a = 0U;
    play_input.flags_ba = 0U;
    play_before = play_state;
    memset(&play_result_before, 0xA5, sizeof(play_result_before));
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_state, &play_result) ||
        memcmp(&play_state, &play_before, sizeof(play_state)) != 0 ||
        memcmp(&play_result, &play_result_before,
               sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 play state alias rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_out = play_before;
    play_result = play_result_before;
    play_input_before = play_input;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_before, &play_input, &play_out,
            (TecmoGameplayCpuSteeringPlayResult *)(void *)&play_input) ||
        memcmp(&play_input, &play_input_before, sizeof(play_input)) != 0 ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 play input/result alias rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.orientation_035a = 2U;
    play_out = play_before;
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_before, &play_input, &play_out, &play_result) ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0 ||
        memcmp(&play_result, &play_result_before,
               sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 play input validation rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.orientation_035a = 0U;
    /* Availability and value are separate: an unavailable $07DF must use
       NO_ACTOR rather than silently carrying a plausible actor slot. */
    play_input.special_actor_07df_available = false;
    play_input.special_actor_07df = 0U;
    play_out = play_before;
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_before, &play_input, &play_out, &play_result) ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0 ||
        memcmp(&play_result, &play_result_before,
               sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 unavailable $07DF sentinel validation failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.special_actor_07df_available = true;
    play_input.special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    play_input.ball_position.x =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1);
    play_out = play_before;
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_before, &play_input, &play_out, &play_result) ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0 ||
        memcmp(&play_result, &play_result_before,
               sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 ball input transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.ball_position.x = 255;
    play_input.ball_position.y = 0;
    play_before.target_object[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_OBJECT_COUNT;
    play_out = play_before;
    play_result = play_result_before;
    if (tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_before, &play_input, &play_out, &play_result) ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0 ||
        memcmp(&play_result, &play_result_before,
               sizeof(play_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode-4 object-state transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&shot_input, 0, sizeof(shot_input));
    shot_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_SHOT_INPUT_TAG;
    shot_input.timer_0798 = 0x2AU;
    shot_input.timer_0760 = 0x10U;
    shot_input.rating_0533 = 0xE0U;
    shot_input.random_byte = 0U;
    for (uint8_t difficulty = 0U;
         difficulty < TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT;
         ++difficulty) {
        shot_input.difficulty = difficulty;
        if (!tecmo_gameplay_cpu_steering_shot_request(
                &assets, &shot_input, &shot_result) ||
            !shot_result.request || !shot_result.wrote_state ||
            shot_result.difficulty_threshold !=
                (uint8_t[]){0x12U,0x1CU,0x28U}[difficulty] ||
            shot_result.actor_state != 0x0DU ||
            shot_result.action != 0x32U || shot_result.timer != 0x12U ||
            shot_result.handoff_code != 0x03U) {
            (void)snprintf(message, message_size,
                           "TGAI-3 shot-request difficulty golden failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    shot_input.difficulty = 0U;
    shot_input.target_delta_low = 0x10U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_low = 0U;
    shot_input.target_delta_high = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request high-distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_high = 0U;
    shot_input.timer_0798 = 0x2BU;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request timer boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.timer_0798 = 0x2AU;
    shot_input.rating_0533 = 0U;
    shot_input.random_byte = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request rating boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.random_byte = 0U;
    shot_input.state_0588 = 1U;
    shot_before = shot_result;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request state gate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.state_0588 = 0U;
    shot_input.flags_ba = 0x40U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request BA gate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&shot_before, 0xA5, sizeof(shot_before));
    shot_result = shot_before;
    shot_input.contract_tag ^= 1U;
    if (tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) ||
        memcmp(&shot_result, &shot_before, sizeof(shot_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-3 shot-request transaction rejection failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!direction_vector(&assets, 10, 0, 0U) ||
        !direction_vector(&assets, -10, 0, 1U) ||
        !direction_vector(&assets, 0, 10, 2U) ||
        !direction_vector(&assets, 10, 10, 3U) ||
        !direction_vector(&assets, -10, 10, 4U) ||
        !direction_vector(&assets, 0, -10, 5U) ||
        !direction_vector(&assets, 10, -10, 6U) ||
        !direction_vector(&assets, -10, -10, 7U) ||
        !direction_vector(&assets, 20, 10, 0U) ||
        !direction_vector(&assets, 19, 10, 3U) ||
        !direction_vector(&assets, 10, 20, 2U) ||
        !direction_vector(&assets, 10, 19, 3U) ||
        !direction_vector(&assets, -10, 20, 2U) ||
        !direction_vector(&assets, -10, 19, 4U) ||
        !direction_vector(&assets, 10, -20, 5U) ||
        !direction_vector(&assets, 10, -19, 6U) ||
        !direction_vector(&assets, -32768, 0, 1U) ||
        !direction_vector(&assets, 0, -32768, 5U) ||
        !direction_vector(&assets, -32768, -32768, 1U) ||
        !direction_vector(&assets, 32767, -32768, 0U) ||
        !direction_vector(&assets, -32767, -32768, 1U) ||
        tecmo_gameplay_cpu_steering_direction_for_delta(
            &assets, 0, 0, &direction) || direction != 0xA5U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 exact octant/transaction vectors failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memset(&harness_input, 0, sizeof(harness_input));
    harness_input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    memcpy(harness_input.actor_position, harness_positions,
           sizeof(harness_positions));
    harness_input.actor = 5U;
    harness_input.possession = 0U;
    harness_input.orientation = 0U;
    harness_input.ball_holder = 0U;
    harness_input.matchup_actor = 0U;
    harness_input.difficulty = 2U;
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &harness_result) ||
        !tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &repeat_result) ||
        memcmp(&harness_result, &repeat_result,
               sizeof(harness_result)) != 0 ||
        harness_result.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_RESULT_TAG ||
        harness_result.input_fingerprint != 0x15AEBE1BU ||
        harness_result.actor_team != 1U ||
        harness_result.target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_LINKED_ACTOR ||
        harness_result.target_actor != 0U ||
        harness_result.target_position.x != 384 ||
        harness_result.target_position.y != 148 ||
        harness_result.horizontal_delta != -116 ||
        harness_result.depth_delta != -12 ||
        !harness_result.writes_direction ||
        harness_result.direction != 1U) {
        (void)snprintf(message, message_size,
                       "TGAI-3 harness linked-actor vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    baseline_fingerprint = harness_result.input_fingerprint;
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        malformed_input = harness_input;
        ++malformed_input.actor_position[actor].x;
        if (!tecmo_gameplay_cpu_steering_harness_evaluate(
                &assets, &malformed_input, &repeat_result) ||
            repeat_result.input_fingerprint == baseline_fingerprint) {
            (void)snprintf(
                message, message_size,
                "TGAI-3 harness ten-coordinate fingerprint failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }

    harness_input.actor = 0U;
    harness_input.matchup_actor = 5U;
    harness_input.difficulty = 0U;
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &harness_result) ||
        harness_result.target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH ||
        harness_result.target_actor !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        harness_result.target_position.x != 208 ||
        harness_result.target_position.y != 148 ||
        harness_result.horizontal_delta != -176 ||
        harness_result.depth_delta != 0 ||
        harness_result.direction != 1U ||
        !harness_result.writes_direction) {
        (void)snprintf(message, message_size,
                       "TGAI-3 harness left-hoop vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    harness_input.orientation = 1U;
    harness_input.difficulty = 2U;
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &harness_result) ||
        harness_result.target_position.x != 568 ||
        harness_result.horizontal_delta != 184 ||
        harness_result.direction != 0U ||
        !harness_result.writes_direction) {
        (void)snprintf(message, message_size,
                       "TGAI-3 harness right-hoop vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    harness_input.actor = 5U;
    harness_input.possession = 0U;
    harness_input.orientation = 0U;
    harness_input.ball_holder = 0U;
    harness_input.matchup_actor = 0U;
    harness_input.difficulty = 1U;
    harness_input.has_explicit_target = true;
    harness_input.explicit_target.x = 600;
    harness_input.explicit_target.y = 148;
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &harness_result) ||
        harness_result.input_fingerprint == baseline_fingerprint ||
        harness_result.target_kind !=
            TECMO_GAMEPLAY_CPU_STEERING_HARNESS_EXPLICIT_TARGET ||
        harness_result.target_actor != 0U ||
        harness_result.target_position.x != 600 ||
        harness_result.target_position.y != 148 ||
        harness_result.horizontal_delta != 100 ||
        harness_result.depth_delta != -12 ||
        harness_result.direction != 0U ||
        !harness_result.writes_direction) {
        (void)snprintf(message, message_size,
                       "TGAI-3 explicit target vector failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    memset(&harness_input, 0, sizeof(harness_input));
    harness_input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    memcpy(harness_input.actor_position, harness_positions,
           sizeof(harness_positions));
    harness_input.actor = 5U;
    harness_input.possession = 0U;
    harness_input.ball_holder = 0U;
    harness_input.matchup_actor = 0U;
    harness_input.difficulty = 1U;
    harness_input.actor_position[5U] =
        harness_input.actor_position[0U];
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, &harness_result) ||
        harness_result.horizontal_delta != 0 ||
        harness_result.depth_delta != 0 ||
        harness_result.writes_direction ||
        harness_result.direction !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION) {
        (void)snprintf(message, message_size,
                       "TGAI-3 harness zero-vector gate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    malformed_input = harness_input;
    malformed_input.contract_tag ^= 1U;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.actor_position[9U].x =
        (int16_t)(TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1);
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.actor_position[9U].y = -1;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.actor = TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.possession = TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.orientation =
        TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.ball_holder = 5U;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.ball_holder =
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.matchup_actor = 6U;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.matchup_actor =
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.difficulty =
        TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT;
    if (!harness_rejected_unchanged(&assets, &malformed_input))
        goto malformed_harness_failure;
    malformed_input = harness_input;
    malformed_input.has_explicit_target = true;
    malformed_input.explicit_target.x =
        TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1;
    if (!harness_rejected_unchanged(&assets, &malformed_input) ||
        !harness_rejected_unchanged(&assets, NULL) ||
        tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &harness_input, NULL)) {
        goto malformed_harness_failure;
    }
    if (!movement_composition_self_test(
            asset_pack_path, &assets, message, message_size)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_global_latch_self_test(
            global_latch_message, sizeof(global_latch_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 global latch failed: %.180s",
                       global_latch_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_self_test(
            a9da_assignment_message, sizeof(a9da_assignment_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 A9DA assignment failed: %.160s",
                       a9da_assignment_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_a8e9_velocity_self_test(
            a8e9_velocity_message, sizeof(a8e9_velocity_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 A8E9 velocity failed: %.160s",
                       a8e9_velocity_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_fixed_rng_self_test(
            fixed_rng_message, sizeof(fixed_rng_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 fixed RNG failed: %.160s",
                       fixed_rng_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_defense_interaction_self_test(
            defense_interaction_message,
            sizeof(defense_interaction_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 defense interaction failed: %.150s",
                       defense_interaction_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_a0f3_launch_self_test(
            asset_pack_path, a0f3_launch_message,
            sizeof(a0f3_launch_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 A0F3 launch failed: %.160s",
                       a0f3_launch_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!tecmo_gameplay_cpu_opcode15_selection_self_test(
            opcode15_selection_message, sizeof(opcode15_selection_message))) {
        (void)snprintf(message, message_size,
                       "TGAI-3 opcode15 selection failed: %.160s",
                       opcode15_selection_message);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    (void)snprintf(
        message, message_size,
        "TGAI-3 CPU steering isolated: commands=680 handlers=24 directions=8 tgmo_adapter=1 scene_adapter=1 route_kernel=1 route_live=1 rom_policy=0");
    return true;

malformed_harness_failure:
    (void)snprintf(message, message_size,
                   "TGAI-3 transactional harness rejection failed.");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return false;
}
