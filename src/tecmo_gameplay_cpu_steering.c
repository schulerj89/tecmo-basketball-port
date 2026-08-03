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
    1U,1U,1U,1U,0U,0U,0U,1U,0U,1U,0U,0U,
    0U,0U,1U,0U,0U,1U,1U,1U,0U,1U,1U,0U
};

static const uint8_t cpu_steering_effect_deferred[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0U,0U,0U,0U,1U,1U,1U,0U,1U,0U,1U,1U,
    1U,1U,0U,1U,1U,0U,0U,0U,1U,0U,0U,1U
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
                   message != NULL ? message : "TGAI-1 rejected");
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
        !bytes_are_zero(payload + 269U,
                        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE -
                            269U)) {
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
            assets, "TGAI-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32) {
        return reject(
            assets, "TGAI-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload) ||
        !validate_handlers_and_commands(payload) ||
        !validate_lifecycle_command_corpus(payload)) {
        return reject(assets, "TGAI-1 source/command contract rejected");
    }
    if (!validate_dependency(movement, movement_size)) {
        return reject(
            assets, "TGAI-1 same-pack TGMO-1 dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGAI-1 allocation failed");
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
    memcpy(assets->fixed_link, cpu_steering_fixed_link,
           sizeof(assets->fixed_link));
    assets->formation_start_count =
        TECMO_GAMEPLAY_CPU_STEERING_FORMATION_START_COUNT;
    if (!decode_formation_native_fields(assets)) {
        return reject(assets, "TGAI-1 formation semantic decode rejected");
    }
    assets->movement_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGAI-1 CPU steering evidence assetpack (isolated lifecycle semantics; scene integration deferred)");
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
            "TGAI-1 gameplay/cpu-steering entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            &movement, &movement_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(
            assets, "TGAI-1 same-pack TGMO-1 dependency missing");
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

static void play_set_target_from_actor(
    TecmoGameplayCpuSteeringPlayState *state,
    const TecmoGameplayCpuSteeringPlayInput *input,
    uint8_t actor,
    uint8_t target_actor)
{
    state->target_actor[actor] = target_actor;
    state->target_x[actor] = input->actor_position[target_actor].x;
    state->target_depth[actor] = input->actor_position[target_actor].y;
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
        state.target_actor[actor] = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
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
        !play_valid_positions(input->actor_position) ||
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
            (state_in->target_actor[actor] !=
                 TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
             !play_valid_actor(state_in->target_actor[actor])) ||
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
        ++next_state.step_serial;

        switch (command.opcode) {
        case 0U: {
            int16_t horizontal = play_signed_u16(
                command.arguments[0U], command.arguments[1U]);
            if (input->orientation_035a != 0U) {
                horizontal = (int16_t)(uint16_t)(0U -
                    (uint16_t)horizontal);
            }
            next_state.target_actor[actor] =
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
            next_state.target_actor[actor] =
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
            if (play_valid_actor(command.arguments[0U])) {
                play_set_target_from_actor(
                    &next_state, input, actor, command.arguments[0U]);
            } else {
                /* The canonical records use C8=$0A, an object index outside
                   the ten player-coordinate inputs. Preserve the handler's
                   proven transport while deferring that unresolved lookup. */
                result.deferred = true;
            }
            break;
        case 5U:
        case 6U:
        case 8U:
        case 10U:
        case 11U:
        case 12U:
        case 13U:
        case 16U:
        case 20U:
        case 23U:
            /* Their effect inputs are deferred because the contract does not
               carry the source RAM/workspace. The source-pinned transport is
               selected by the opcode-specific policy below; it is not implied
               that every deferred handler leaves this record in place. */
            result.deferred = true;
            break;
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
            command.opcode == 10U ||
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
    result.target_actor = next_state.target_actor[actor];
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
                "TGAI-1 to TGMO-1 direction composition failed.");
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
            "TGAI-1 zero-vector neutral composition failed.");
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
                   "TGAI-1 to TGMO-1 movement vector failed.");
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    return false;

movement_transaction_failure:
    (void)snprintf(
        message, message_size,
        "TGAI-1 to TGMO-1 transactional rejection failed.");
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
                       "TGAI-1 command decode vectors failed.");
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
                       "TGAI-1 transactional decode rejection failed.");
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
                       "TGAI-1 formation lifecycle contract failed.");
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
                       "TGAI-1 formation boundary rejection failed.");
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
                       "TGAI-1 short route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0x80U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route || route_result.stream_offset != 0x00D7U) {
        (void)snprintf(message, message_size,
                       "TGAI-1 long route selector golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.global_0373 = 0U;
    route_input.flag_0095 = 1U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-1 route flag branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.flag_0095 = 0U;
    route_input.age_0094 = 0x28U;
    if (!tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        !route_result.used_long_route) {
        (void)snprintf(message, message_size,
                       "TGAI-1 route age branch golden failed.");
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
                       "TGAI-1 route no-write branch golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_result = route_before;
    route_input.contract_tag = 0U;
    if (tecmo_gameplay_cpu_steering_route_select(
            &assets, &route_input, &route_result) ||
        memcmp(&route_result, &route_before, sizeof(route_result)) != 0) {
        (void)snprintf(message, message_size,
                       "TGAI-1 route bad-tag transaction failed.");
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
                       "TGAI-1 route bad-index transaction failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    route_input.route_slot = 0U;
    {
        const TecmoGameplayCpuSteeringEffectMetadata *metadata_0 =
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 0U);
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
        if (metadata_0 == NULL || metadata_7 == NULL || metadata_10 == NULL ||
            metadata_17 == NULL || metadata_18 == NULL || metadata_19 == NULL ||
            metadata_21 == NULL ||
            !metadata_0->exact_bounded || metadata_0->deferred_inputs ||
            metadata_7->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_PLUS_FIVE_OR_BRANCH_PLUS_FIVE ||
            metadata_10->exact_bounded || !metadata_10->deferred_inputs ||
            metadata_17->kind !=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_AGGREGATION_BARRIER ||
            metadata_18->kind != metadata_17->kind ||
            metadata_19->kind != metadata_17->kind ||
            !metadata_17->exact_bounded || !metadata_18->exact_bounded ||
            !metadata_19->exact_bounded || metadata_21->advance_policy !=
                TECMO_GAMEPLAY_CPU_STEERING_ADVANCE_CONDITIONAL_FIVE_OR_TEN ||
            tecmo_gameplay_cpu_steering_effect_metadata(&assets, 4U)->exact_bounded ||
            !tecmo_gameplay_cpu_steering_effect_metadata(&assets, 4U)->deferred_inputs) {
            (void)snprintf(message, message_size,
                           "TGAI-1 lifecycle effect metadata failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    for (uint8_t opcode = 0U;
         opcode < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++opcode) {
        if (assets.command_count_by_opcode[opcode] != 0U &&
            !find_lifecycle_opcode_offset(&assets, opcode,
                                          &opcode_offsets[opcode])) {
            (void)snprintf(message, message_size,
                           "TGAI-1 lifecycle opcode coverage failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state) ||
        play_state.matchup_seed[0U] != 2U || play_state.matchup_seed[1U] != 7U) {
        (void)snprintf(message, message_size,
                       "TGAI-1 lifecycle startup state failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (play_state.native_matchup_actor[actor] !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            (void)snprintf(message, message_size,
                           "TGAI-1 fixed startup seeds leaked into matchup state.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    memset(&play_input, 0, sizeof(play_input));
    play_input.contract_tag = TECMO_GAMEPLAY_CPU_STEERING_PLAY_INPUT_TAG;
    play_input.actor = 0U;
    play_input.step_budget = 4U;
    memcpy(play_input.actor_position, harness_positions,
           sizeof(harness_positions));

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
                       "TGAI-1 opcode-7 equal-probe golden failed.");
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
                       "TGAI-1 opcode-7 mismatch-probe golden failed.");
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
                       "TGAI-1 one-handler-per-tick golden failed.");
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
        play_result.budget_exhausted || !play_result.deferred ||
        play_out.stream_offset[0U] != 0x0005U) {
        (void)snprintf(message, message_size,
                       "TGAI-1 goto-chain two-step golden failed.");
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
                       "TGAI-1 goto-chain budget golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

    /* Opcode 10's global/link workspace is intentionally absent from this
       contract: defer only its target/proximity effect and keep the record
       available for the caller's source-observed retry/advance decision. */
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            &assets, 0U, &play_state)) {
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    play_state.stream_offset[0U] = opcode_offsets[10U];
    play_input.step_budget = 4U;
    play_input.flags_ba = 0U;
    if (!tecmo_gameplay_cpu_steering_play_step(
            &assets, &play_state, &play_input, &play_out, &play_result) ||
        play_result.command.opcode != 10U || !play_result.deferred ||
        play_result.advanced ||
        play_result.next_offset != opcode_offsets[10U] ||
        play_out.stream_offset[0U] != opcode_offsets[10U]) {
        (void)snprintf(message, message_size,
                       "TGAI-1 opcode-10 deferred retry golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

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
                       "TGAI-1 wait-expiry golden failed.");
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
                       "TGAI-1 opcode-14 $04B0 golden failed.");
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
                       "TGAI-1 aggregation barrier golden failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }

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
                       "TGAI-1 opcode-21 one-record golden failed.");
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
                       "TGAI-1 opcode-21 two-record golden failed.");
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
                       "TGAI-1 opcode-22 mask retention golden failed.");
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
                       "TGAI-1 opcode-0 forward golden failed.");
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
                       "TGAI-1 opcode-0 orientation/BA golden failed.");
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
            play_result.next_offset !=
                (uint16_t)(opcode_offsets[deferred_opcode] + 5U)) {
            (void)snprintf(message, message_size,
                           "TGAI-1 deferred transport golden failed for opcode %u.",
                           (unsigned)deferred_opcode);
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
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
                       "TGAI-1 play state alias rejection failed.");
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
                       "TGAI-1 play input/result alias rejection failed.");
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
                       "TGAI-1 play input validation rejection failed.");
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
                           "TGAI-1 shot-request difficulty golden failed.");
            tecmo_gameplay_cpu_steering_assets_destroy(&assets);
            return false;
        }
    }
    shot_input.difficulty = 0U;
    shot_input.target_delta_low = 0x10U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_low = 0U;
    shot_input.target_delta_high = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request high-distance boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.target_delta_high = 0U;
    shot_input.timer_0798 = 0x2BU;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request timer boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.timer_0798 = 0x2AU;
    shot_input.rating_0533 = 0U;
    shot_input.random_byte = 1U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request rating boundary failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.random_byte = 0U;
    shot_input.state_0588 = 1U;
    shot_before = shot_result;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request state gate failed.");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return false;
    }
    shot_input.state_0588 = 0U;
    shot_input.flags_ba = 0x40U;
    if (!tecmo_gameplay_cpu_steering_shot_request(
            &assets, &shot_input, &shot_result) || shot_result.request) {
        (void)snprintf(message, message_size,
                       "TGAI-1 shot-request BA gate failed.");
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
                       "TGAI-1 shot-request transaction rejection failed.");
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
                       "TGAI-1 exact octant/transaction vectors failed.");
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
                       "TGAI-1 harness linked-actor vector failed.");
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
                "TGAI-1 harness ten-coordinate fingerprint failed.");
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
                       "TGAI-1 harness left-hoop vector failed.");
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
                       "TGAI-1 harness right-hoop vector failed.");
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
                       "TGAI-1 explicit target vector failed.");
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
                       "TGAI-1 harness zero-vector gate failed.");
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
        "TGAI-1 CPU steering isolated: commands=680 handlers=24 directions=8 tgmo_adapter=1 scene_adapter=1 rom_policy=0");
    return true;

malformed_harness_failure:
    (void)snprintf(message, message_size,
                   "TGAI-1 transactional harness rejection failed.");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return false;
}
