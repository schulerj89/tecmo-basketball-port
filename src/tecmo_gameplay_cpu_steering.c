#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_cpu_steering.h"

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
    TECMO_GAMEPLAY_CPU_STEERING_EFFECT_GLOBAL_SCRATCH_TARGET,
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
    /* 12 */ TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_NONE_OR_FIVE,
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
    1U,1U,1U,1U,1U,0U,0U,1U,0U,1U,1U,0U,
    0U,0U,1U,0U,1U,1U,1U,1U,0U,1U,1U,0U
};

static const uint8_t cpu_steering_effect_deferred[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0U,0U,0U,0U,0U,1U,1U,0U,1U,0U,0U,1U,
    1U,1U,0U,1U,0U,0U,0U,0U,1U,0U,0U,1U
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
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK

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
                   message != NULL ? message : "TGAI-2 rejected");
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
        read_u32(payload + 264U) != 0x00113415U ||
        payload[268U] != 5U ||
        !bytes_are_zero(payload + 269U, 3U) ||
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
                        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE -
                            (TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET +
                             TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE))) {
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
        return "TGAI-2 opcode-15 handler range rejected";
    }
    if (fnv1a32(handlers + handler_offset,
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_FNV1A32) {
        return "TGAI-2 opcode-15 full handler anchor rejected";
    }
    if (memcmp(handlers + handler_offset,
               "\xA0\x09\x84\xA4\xE4\xA4\xF0\x16"
               "\xB9\xB0\x04\x29\x10\xF0\x0F"
               "\xA9\x23\x99\x47\x05\xA9\x00"
               "\x99\x51\x05\xA9\x04\x99\x7C\x05",
               30U) != 0) {
        return "TGAI-2 opcode-15 mark-other anchor rejected";
    }
    if (memcmp(handlers + (0x9172U - 0x8BE1U),
               "\xAD\x99\x04\xC9\x46\xB0\x01\x60"
               "\xBD\xB0\x04\x29\x10\xD0\x41",
               15U) != 0) {
        return "TGAI-2 opcode-15 gate anchor rejected";
    }
    if (memcmp(handlers + (0x91C8U - 0x8BE1U),
               "\xAC\x09\x03\x8E\x09\x03\xA9\x04"
               "\x99\x7C\x05\xA9\x5A\x99\x47\x05"
               "\xA9\x00\x99\x51\x05\xA9\x00\x99\x6E\x04",
               26U) != 0) {
        return "TGAI-2 opcode-15 defender-write anchor rejected";
    }
    if (memcmp(handlers + (0x9208U - 0x8BE1U),
               "\xA9\x07\x9D\x7C\x05\x8E\x9E\x05"
               "\x8A\xA8\xA9\x04\x4C\x11\xC7", 15U) != 0 ||
        fnv1a32(handlers + (0x9208U - 0x8BE1U),
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_FINAL_TAIL_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_FINAL_TAIL_FNV1A32) {
        return "TGAI-2 opcode-15 canonical Rev1 tail anchor rejected";
    }
    if (memcmp(helper,
               "\xBC\x63\x04\xB9\xCA\x88\x9D\x42\x04"
               "\xB9\xD2\x88\x9D\x4D\x04\xA9\xC1\x9D"
               "\x79\x04\xA9\x30\x9D\x58\x04\x60"
               "\x0C\x0A\x10\x0C\x0A\x0E\x0C\x0A"
               "\x04\x04\x04\x04\x04\x04\x04\x04",
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE) != 0) {
        return "TGAI-2 opcode-15 $88B0 helper raw anchor rejected";
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
        return "TGAI-2 canonical opcode-15 record anchor rejected";
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
            assets, "TGAI-2 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32) {
        return reject(
            assets, "TGAI-2 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size)) {
        return reject(assets, "TGAI-2 source descriptor/raw contract rejected");
    }
    if (!validate_padding(payload)) {
        return reject(assets, "TGAI-2 payload padding contract rejected");
    }
    if (!validate_handlers_and_commands(payload) ||
        !validate_lifecycle_command_corpus(payload)) {
        return reject(assets, "TGAI-2 command corpus contract rejected");
    }
    if (!validate_opcode15_contract(payload)) {
        return reject(assets, opcode15_contract_error(payload));
    }
    if (!validate_dependency(movement, movement_size)) {
        return reject(
            assets, "TGAI-2 same-pack TGMO-1 dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGAI-2 allocation failed");
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
        return reject(assets, "TGAI-2 formation semantic decode rejected");
    }
    assets->movement_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGAI-2 CPU steering evidence assetpack (opcode-15 raw contract; LIVE integration deferred)");
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
            "TGAI-2 gameplay/cpu-steering entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            &movement, &movement_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets, "TGAI-2 same-pack TGMO-1 dependency missing");
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
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_RETRY:
        return "deferred-primary-retry";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_SWAP:
        return "deferred-primary-swap";
    case TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MARK_OTHER:
        return "deferred-mark-other";
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
    case 0U:
    case 2U:
        /* $92CA consumes $BA after these handlers. */
        return input->common_tail_ba_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    case 13U:
        /* $9125 first reads the unretained $038D-$0390 global target before
           it could reach $92CA. Do not report a later BA byte as its owner. */
        return TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS;
    case 7U:
        /* $8F11 probes $046E,C8 before selecting either stream branch. */
        return input->actor_046e_probe_available
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_ACTOR_046E_PROBE;
    case 10U:
        /* $8CD0 compares X with the exceptional $07DF actor before it can
           enter the linked-relative helper at $8D59. */
        if (!input->special_actor_07df_available) {
            return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_SPECIAL_ACTOR_07DF;
        }
        return input->linked_relative_valid
            ? TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE
            : TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_LINKED_RELATIVE_WORKSPACE;
    case 15U:
        /* $9172-$9216 owns a wider raw lifecycle than this LIVE contract. */
        return TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_OPCODE15_RAW_LIFECYCLE;
    case 16U:
        /* $9081 resolves $0309 through $036E/$0370; $92CA then needs $BA. */
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
        state.native_matchup_actor[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
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
             TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT)) {
        return false;
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
            /* $9181-$9185 branches back to the raw $0499 gate. The bounded
               harness records the retry rather than iterating or mutating
               primary ownership. */
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_RETRY;
        } else {
            result.branch =
                TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_SWAP;
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
        /* $91C2-$91C7 selects the $9146 mark-other path. Its full actor-loop
           ownership and $8FD9/$9070 tails are intentionally not inferred. */
        result.branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MARK_OTHER;
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
            (state_in->native_matchup_actor[actor] !=
                 TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
             !play_valid_actor(state_in->native_matchup_actor[actor]))) {
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
        if (next_state.wait_counter[actor] != 0U) {
            --next_state.wait_counter[actor];
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
                /* $901D-$9027 ORs the full X and sign-extended depth
                   vectors; $9025 takes the no-direction-write branch only
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
        case 5U:
        case 6U:
        case 8U:
        case 11U:
        case 12U:
        case 13U:
        case 20U:
        case 23U:
            /* Their effect inputs are deferred because the contract does not
               carry the source RAM/workspace. The source-pinned transport is
               selected by the opcode-specific policy below; it is not implied
               that every deferred handler leaves this record in place. */
            result.deferred = true;
            result.deferred_reason =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS;
            break;
        case 10U: {
            uint8_t linked = actor == input->special_actor_07df
                ? next_state.primary_actor
                : next_state.native_matchup_actor[actor];
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
            next_state.timer[actor] = command.arguments[1U];
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

        if (command.opcode == 6U || command.opcode == 8U ||
            (command.opcode == 10U && !result.proximity_met) ||
            command.opcode == 12U || command.opcode == 15U ||
            (command.opcode == 16U && (input->flags_ba & 0x03U) != 0U)) {
            following_offset = current_offset;
        } else if (command.opcode == 11U) {
            /* $8C40 itself calls $8FD9 after its source-dependent pose work. */
            following_offset = play_next_offset(current_offset, 5U);
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
            command.opcode != 6U && command.opcode != 8U &&
            command.opcode != 10U && command.opcode != 12U &&
            command.opcode != 20U && command.opcode != 23U &&
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
        input->ball_holder >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        input->matchup_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        input->difficulty >=
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT ||
        harness_actor_team(input->ball_holder) != input->possession) {
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
                "TGAI-2 to TGMO-1 direction composition failed.");
            tecmo_gameplay_movement_assets_destroy(&movement_assets);
            return false;
        }
    }

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
            "TGAI-2 zero-vector neutral composition failed.");
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
                   "TGAI-2 to TGMO-1 movement vector failed.");
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    return false;

movement_transaction_failure:
    (void)snprintf(
        message, message_size,
        "TGAI-2 to TGMO-1 transactional rejection failed.");
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
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }

    opcode15_raw_fixture(&input);
    input.raw_04b0_actor_x = 0U;
    input.raw_007e = 0x04U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_RETRY ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }
    input.raw_007e = 0U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_PRIMARY_SWAP ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }
    input.raw_04b0_actor_x = 0x10U;
    input.raw_007e = 0x08U;
    before = input;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MARK_OTHER ||
        result.committed || memcmp(&output, &before, sizeof(output)) != 0) {
        return false;
    }

    opcode15_raw_fixture(&input);
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            assets, &input, &output, &result) ||
        result.branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFENDER_REPLACED ||
        !result.committed || result.raw_0308_before != 4U ||
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

bool tecmo_gameplay_cpu_steering_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
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
    TecmoGameplayCpuSteeringPlayState play_state;
    TecmoGameplayCpuSteeringPlayState play_before;
    TecmoGameplayCpuSteeringPlayState play_out;
    TecmoGameplayCpuSteeringPlayInput play_input;
    TecmoGameplayCpuSteeringPlayInput play_input_before;
    TecmoGameplayCpuSteeringPlayResult play_result;
    TecmoGameplayCpuSteeringPlayResult play_result_before;
    TecmoGameplayCpuSteeringShotInput shot_input;
    TecmoGameplayCpuSteeringShotResult shot_result;
    TecmoGameplayCpuSteeringShotResult shot_before;
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
    if (!opcode15_raw_resolver_self_test(&assets)) {
        (void)snprintf(message, message_size,
                       "TGAI-2 opcode-15 raw resolver contract failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    if (!opcode15_parser_anchor_self_test(&assets)) {
        (void)snprintf(message, message_size,
                       "TGAI-2 opcode-15 parser/anchor mutations failed.");
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
                       "TGAI-2 command decode vectors failed.");
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
                       "TGAI-2 transactional decode rejection failed.");
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
                       "TGAI-2 formation lifecycle contract failed.");
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
                       "TGAI-2 formation boundary rejection failed.");
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
                       "TGAI-2 short route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0x80U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route || route_result.stream_offset != 0x00D7U) {
        (void)snprintf(message, message_size,
                       "TGAI-2 long route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0U;
    route_input.flag_0095 = 1U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-2 route flag branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.flag_0095 = 0U;
    route_input.age_0094 = 0x28U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-2 route age branch golden failed.");
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
                       "TGAI-2 route no-write branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_result = route_before;
    route_input.contract_tag = 0U;
    if (tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        memcmp(&route_result, &route_before, sizeof(route_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-2 route bad-tag transaction failed.");
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
                       "TGAI-2 route bad-index transaction failed.");
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
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_17 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 17U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_18 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 18U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_19 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 19U);
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_21 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 21U);
        if (metadata_0 == NULL || metadata_4 == NULL || metadata_7 == NULL || metadata_10 == NULL ||
            metadata_17 == NULL || metadata_18 == NULL || metadata_19 == NULL ||
            metadata_21 == NULL ||
            !metadata_0->exact_bounded || metadata_0->deferred_inputs ||
            !metadata_4->exact_bounded || metadata_4->deferred_inputs ||
            metadata_7->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_BRANCH_PLUS_FIVE ||
            !metadata_10->exact_bounded || metadata_10->deferred_inputs ||
            metadata_17->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER ||
            metadata_18->kind != metadata_17->kind ||
            metadata_19->kind != metadata_17->kind ||
            !metadata_17->exact_bounded || !metadata_18->exact_bounded ||
            !metadata_19->exact_bounded || metadata_21->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_FIVE_OR_TEN ||
            metadata_10->native_approximation) {
            (void)snprintf(message, message_size,
                           "TGAI-2 lifecycle effect metadata failed.");
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
               "unimplemented-handler") != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-2 deferred-reason names changed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (uint8_t opcode = 0U;
         opcode < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++opcode) {
        if (assets.command_count_by_opcode[opcode] != 0U &&
            !find_lifecycle_opcode_offset(&assets, opcode,
                                          &opcode_offsets[opcode])) {
            (void)snprintf(message, message_size,
                           "TGAI-2 lifecycle opcode coverage failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state) ||
        play_state.matchup_seed[0U] != 2U || play_state.matchup_seed[1U] != 7U) {
        (void)snprintf(message, message_size,
                       "TGAI-2 lifecycle startup state failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (play_state.native_matchup_actor[actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            (void)snprintf(message, message_size,
                           "TGAI-2 fixed startup seeds leaked into matchup state.");
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
    play_input.special_actor_07df = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));
    play_input.ball_position.x = 255;
    play_input.ball_position.y = 0;

    /* The first canonical record is opcode 4 C8=$0A. Bank06
       $8FFD-$9018 subtracts target object X as a 16-bit value and target
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
                       "TGAI-2 opcode-4 canonical ball target borrow failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
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
                       "TGAI-2 opcode-4 zero-vector guard failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));

    /* Bank06 $8F11 cannot treat a zeroed native array as $046E,C8. An
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
                       "TGAI-2 opcode-7 unavailable probe transaction failed.");
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
                       "TGAI-2 opcode-7 equal-probe golden failed.");
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
                       "TGAI-2 opcode-7 mismatch-probe golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

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
                       "TGAI-2 one-handler-per-tick golden failed.");
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
                       "TGAI-2 goto-chain two-step golden failed.");
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
                       "TGAI-2 goto-chain budget golden failed.");
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
                       "TGAI-2 opcode-10 unavailable $07DF transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.special_actor_07df_available = true;
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
                       "TGAI-2 opcode-10 unavailable helper transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* With the bounded helper workspace present, opcode 10 synthesizes the
       linked target with 16-bit wrap and admits exactly [-8,+7] on each axis. */
    for (int delta = -8; delta <= 8; ++delta) {
        if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                &assets, 0U, &play_state)) {
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        play_state.stream_offset[0U] = opcode_offsets[10U];
        play_state.native_matchup_actor[0U] = 5U;
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
                           "TGAI-2 opcode-10 proximity boundary failed at %d.",
                           delta);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.linked_relative_valid = false;

    /* $9081 cannot resolve the $0309 target route without the paired
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
                       "TGAI-2 opcode-16 unavailable pointer transaction failed.");
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
                       "TGAI-2 opcode-16 unavailable BA transaction failed.");
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
                           "TGAI-2 opcode-16 adjustment branch %u failed.",
                           (unsigned)branch);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    play_input.pointer_workspace_valid = false;
    play_input.orientation_035a = 0U;

    /* Existing wait state expires to state 4 without fetching a record. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.wait_counter[0U] = 1U;
    play_state.actor_state[0U] = 0x06U;
    play_before = play_state;
    play_input.step_budget = 4U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.fetched || play_result.waiting ||
        play_out.wait_counter[0U] != 0U ||
        play_out.actor_state[0U] != 0x04U ||
        play_out.stream_offset[0U] != play_before.stream_offset[0U]) {
        (void)snprintf(message, message_size,
                       "TGAI-2 wait-expiry golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
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
                       "TGAI-2 opcode-14 $04B0 golden failed.");
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
                       "TGAI-2 aggregation barrier golden failed.");
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
                       "TGAI-2 opcode-21 unavailable gate transaction failed.");
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
                       "TGAI-2 opcode-21 one-record golden failed.");
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
                       "TGAI-2 opcode-21 two-record golden failed.");
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
                       "TGAI-2 opcode-22 mask retention golden failed.");
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
                       "TGAI-2 opcode-0 unavailable BA transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_input.common_tail_ba_available = true;

    /* Opcode 13 starts at $9125 with the unretained $038D-$0390 target
       workspace. Its later common-tail branch cannot make that handler live. */
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
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[13U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-2 opcode-13 missing target transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

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
                       "TGAI-2 opcode-0 forward golden failed.");
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
                       "TGAI-2 opcode-0 orientation/BA golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Deferred pose/target effects preserve their exact transport. */
    for (size_t index = 0U; index < 3U; ++index) {
        static const uint8_t deferred_opcodes[3] = {5U,20U,23U};
        uint8_t deferred_opcode = deferred_opcodes[index];
        if (!tecmo_gameplay_cpu_steering_play_state_initialize(
                &assets, 0U, &play_state)) {
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
        play_state.stream_offset[0U] = opcode_offsets[deferred_opcode];
        play_input.orientation_035a = 0U;
        play_input.flags_ba = 0U;
        if (!tecmo_gameplay_cpu_steering_play_step(
                &assets, &play_state, &play_input, &play_out, &play_result) ||
            !play_result.deferred ||
            play_result.deferred_reason !=
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_UNSUPPORTED_HANDLER_INPUTS ||
            play_result.next_offset !=
                (uint16_t)(opcode_offsets[deferred_opcode] + 5U)) {
            (void)snprintf(message, message_size,
                           "TGAI-2 deferred transport golden failed for opcode %u.",
                           (unsigned)deferred_opcode);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }

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
    play_state.timer[0U] = 0xC3U;
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
        play_out.timer[0U] != play_before.timer[0U] ||
        memcmp(&play_out, &play_before, sizeof(play_out)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-2 opcode-15 LIVE deferred boundary failed.");
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
                       "TGAI-2 play state alias rejection failed.");
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
                       "TGAI-2 play input/result alias rejection failed.");
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
                       "TGAI-2 play input validation rejection failed.");
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
                       "TGAI-2 unavailable $07DF sentinel validation failed.");
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
                       "TGAI-2 opcode-4 ball input transaction failed.");
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
                       "TGAI-2 opcode-4 object-state transaction failed.");
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
                           "TGAI-2 shot-request difficulty golden failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    shot_input.difficulty = 0U;
    shot_input.target_delta_low = 0x10U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_low = 0U;
    shot_input.target_delta_high = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request high-distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_high = 0U;
    shot_input.timer_0798 = 0x2BU;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request timer boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.timer_0798 = 0x2AU;
    shot_input.rating_0533 = 0U;
    shot_input.random_byte = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request rating boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.random_byte = 0U;
    shot_input.state_0588 = 1U;
    shot_before = shot_result;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request state gate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.state_0588 = 0U;
    shot_input.flags_ba = 0x40U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-2 shot-request BA gate failed.");
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
                       "TGAI-2 shot-request transaction rejection failed.");
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
                       "TGAI-2 exact octant/transaction vectors failed.");
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
                       "TGAI-2 harness linked-actor vector failed.");
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
                "TGAI-2 harness ten-coordinate fingerprint failed.");
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
                       "TGAI-2 harness left-hoop vector failed.");
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
                       "TGAI-2 harness right-hoop vector failed.");
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
                       "TGAI-2 explicit target vector failed.");
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
                       "TGAI-2 harness zero-vector gate failed.");
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
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    (void)snprintf(
        message, message_size,
        "TGAI-2 CPU steering isolated: commands=680 handlers=24 directions=8 tgmo_adapter=1 scene_adapter=1 rom_policy=0");
    return true;

malformed_harness_failure:
    (void)snprintf(message, message_size,
                   "TGAI-2 transactional harness rejection failed.");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return false;
}
