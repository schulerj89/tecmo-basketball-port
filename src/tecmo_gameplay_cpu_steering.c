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
    memset(assets->direction_map, 0, sizeof(assets->direction_map));
    assets->command_base_cpu = 0U;
    assets->command_record_count = 0U;
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
        !validate_handlers_and_commands(payload)) {
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
    }
    memcpy(assets->direction_map,
           storage +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECTION_OFFSET,
           sizeof(assets->direction_map));
    assets->movement_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGAI-1 CPU steering evidence assetpack (native live adapter ready)");
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
    *command_out = command;
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
