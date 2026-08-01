#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_ball_dribble.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_ball_dribble.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG 0x31444254U

static const uint8_t ball_dribble_rev1_sha256[32] = {
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

static bool reject(TecmoGameplayBallDribbleAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->direction_half = NULL;
    assets->bounce_height = NULL;
    assets->y_offset = NULL;
    assets->x_offset_low = NULL;
    assets->x_offset_high = NULL;
    assets->sound_phase = 0U;
    assets->sound_high_nibble = 0U;
    assets->gameplay_core_fingerprint = 0U;
    assets->movement_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGBD-1 rejected");
    return false;
}

void tecmo_gameplay_ball_dribble_assets_init(
    TecmoGameplayBallDribbleAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG;
}

void tecmo_gameplay_ball_dribble_assets_destroy(
    TecmoGameplayBallDribbleAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_ball_dribble_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE &&
           memcmp(payload, "TGBD", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_FNV1A32 &&
           read_u32(payload + 32U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE &&
           read_u32(payload + 40U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_FNV1A32 &&
           read_u32(payload + 44U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u32(payload + 48U) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           read_u32(payload + 52U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE &&
           read_u32(payload + 56U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32 &&
           read_u32(payload + 60U) == 393232U &&
           read_u32(payload + 64U) == 0x0650F5B0U &&
           memcmp(payload + 68U, ball_dribble_rev1_sha256,
                  sizeof(ball_dribble_rev1_sha256)) == 0 &&
           payload[100U] == TECMO_GAMEPLAY_BALL_DRIBBLE_DIRECTION_COUNT &&
           payload[101U] == TECMO_GAMEPLAY_BALL_DRIBBLE_PHASE_COUNT &&
           payload[102U] == TECMO_GAMEPLAY_BALL_DRIBBLE_HALF_COUNT &&
           payload[103U] == 3U && payload[104U] == 0U &&
           payload[105U] == 10U && payload[106U] == 6U &&
           payload[107U] == 0U &&
           read_u32(payload + 108U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_FNV1A32 &&
           bytes_are_zero(
               payload + 112U,
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_HEADER_SIZE - 112U);
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT; ++index) {
        const TecmoGameplayBallDribbleExpectedSource *expected =
            &tecmo_gameplay_ball_dribble_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE;
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
    return bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE));
}

static bool validate_dependencies(const uint8_t *gameplay_core,
                                  size_t gameplay_core_size,
                                  const uint8_t *movement,
                                  size_t movement_size)
{
    return gameplay_core != NULL &&
           gameplay_core_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(gameplay_core, "TGPL", 4U) == 0 &&
           read_u16(gameplay_core + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           fnv1a32(gameplay_core, gameplay_core_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           movement != NULL &&
           movement_size == TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE &&
           memcmp(movement, "TGMO", 4U) == 0 &&
           read_u16(movement + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_VERSION &&
           fnv1a32(movement, movement_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
}

static bool validate_table_layout(const uint8_t *tables)
{
    static const uint8_t direction_half[8] = {
        1U,0U,0U,1U,0U,0U,0U,1U
    };
    if (memcmp(tables, direction_half, sizeof(direction_half)) != 0) {
        return false;
    }
    for (size_t index = 0U; index < 128U; ++index) {
        if (tables[8U + index] > 0x16U) return false;
    }
    return true;
}

bool tecmo_gameplay_ball_dribble_assets_parse(
    TecmoGameplayBallDribbleAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *movement,
    size_t movement_size)
{
    uint8_t *storage;
    const uint8_t *tables;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_ball_dribble_assets_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(
            assets, "TGBD-1 header/size/reserved contract rejected");
    }
    tables = payload + TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET;
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_FNV1A32 ||
        !validate_sources(payload, payload_size) ||
        !validate_table_layout(tables)) {
        return reject(
            assets, "TGBD-1 canonical source/table contract rejected");
    }
    if (!validate_dependencies(gameplay_core, gameplay_core_size,
                               movement, movement_size)) {
        return reject(
            assets,
            "TGBD-1 same-pack gameplay/core and gameplay/movement dependencies rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGBD-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT; ++index) {
        const TecmoGameplayBallDribbleExpectedSource *expected =
            &tecmo_gameplay_ball_dribble_expected_sources[index];
        TecmoGameplayBallDribbleSourceSpan *source =
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
    tables = storage + TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET;
    assets->direction_half = tables;
    assets->bounce_height = tables + 8U;
    assets->y_offset = tables + 136U;
    assets->x_offset_low = tables + 152U;
    assets->x_offset_high = tables + 168U;
    assets->sound_phase = storage[103U];
    assets->sound_high_nibble = storage[104U];
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->movement_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGBD-1 held-ball bounce assetpack");
    return true;
}

bool tecmo_gameplay_ball_dribble_assets_load(
    TecmoGameplayBallDribbleAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *movement = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t movement_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag !=
            TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_ball_dribble_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ID,
            TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGBD-1 gameplay/ball-dribble entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            &movement, &movement_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        tecmo_asset_pack_free(movement);
        return reject(
            assets,
            "TGBD-1 same-pack dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_ball_dribble_assets_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        movement, (size_t)movement_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(movement);
    return loaded;
}

static bool assets_valid(const TecmoGameplayBallDribbleAssets *assets)
{
    return assets != NULL &&
           assets->lifecycle_tag ==
               TECMO_GAMEPLAY_BALL_DRIBBLE_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_FNV1A32 &&
           assets->direction_half != NULL &&
           assets->bounce_height != NULL &&
           assets->y_offset != NULL &&
           assets->x_offset_low != NULL &&
           assets->x_offset_high != NULL &&
           assets->sound_phase == 3U &&
           assets->sound_high_nibble == 0U &&
           assets->gameplay_core_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           assets->movement_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32;
}

const TecmoGameplayBallDribbleSourceSpan *
tecmo_gameplay_ball_dribble_find_source(
    const TecmoGameplayBallDribbleAssets *assets,
    TecmoGameplayBallDribbleSourceKind kind)
{
    if (!assets_valid(assets)) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_ball_dribble_resolve(
    const TecmoGameplayBallDribbleAssets *assets,
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayMovementState *holder,
    const TecmoGameplayCourtCoordinate *linked_position,
    TecmoGameplayBallDribbleFrame *frame_out)
{
    TecmoGameplayBallDribbleFrame frame;
    bool alternate;
    size_t attachment_index;
    size_t height_index;
    int16_t x_offset;
    int8_t y_offset;
    int32_t base_x;
    int32_t base_y;
    int32_t visible_y;
    uint8_t phase;
    if (!assets_valid(assets) || frame_out == NULL ||
        !tecmo_gameplay_movement_state_valid(movement_assets, holder) ||
        linked_position == NULL ||
        !tecmo_gameplay_court_coordinate_valid(linked_position) ||
        !tecmo_gameplay_movement_pose_half(
            movement_assets, holder, linked_position, &alternate)) {
        return false;
    }
    phase = (uint8_t)(holder->animation_phase & 0x0FU);
    if (holder->direction >= TECMO_GAMEPLAY_BALL_DRIBBLE_DIRECTION_COUNT ||
        phase >= TECMO_GAMEPLAY_BALL_DRIBBLE_PHASE_COUNT) {
        return false;
    }
    attachment_index = (alternate ? 8U : 0U) + holder->direction;
    height_index = (alternate ? 64U : 0U) +
                   (size_t)holder->direction * 8U + phase;
    x_offset = (int16_t)(uint16_t)(
        (uint16_t)assets->x_offset_low[attachment_index] |
        ((uint16_t)assets->x_offset_high[attachment_index] << 8U));
    y_offset = (int8_t)assets->y_offset[attachment_index];
    base_x = (int32_t)holder->position.x + x_offset;
    base_y = (int32_t)holder->position.y + y_offset;
    visible_y = base_y - assets->bounce_height[height_index];
    if (base_x < INT16_MIN || base_x > INT16_MAX ||
        base_y < INT16_MIN || base_y > INT16_MAX ||
        visible_y < INT16_MIN || visible_y > INT16_MAX) {
        return false;
    }
    memset(&frame, 0, sizeof(frame));
    frame.contract_tag = TECMO_GAMEPLAY_BALL_DRIBBLE_FRAME_TAG;
    frame.base_position.x = (int16_t)base_x;
    frame.base_position.y = (int16_t)base_y;
    frame.visible_position.x = (int16_t)base_x;
    frame.visible_position.y = (int16_t)visible_y;
    frame.x_offset = x_offset;
    frame.y_offset = y_offset;
    frame.bounce_height = assets->bounce_height[height_index];
    frame.table_half = alternate ? 1U : 0U;
    frame.direction = holder->direction;
    frame.animation_phase = holder->animation_phase;
    frame.sound_trigger =
        phase == assets->sound_phase &&
        (holder->animation_phase >> 4U) ==
            assets->sound_high_nibble;
    if (!tecmo_gameplay_court_coordinate_valid(&frame.base_position) ||
        !tecmo_gameplay_court_coordinate_valid(&frame.visible_position)) {
        return false;
    }
    *frame_out = frame;
    return true;
}

bool tecmo_gameplay_ball_dribble_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayBallDribbleAssets assets;
    TecmoGameplayMovementAssets movement_assets;
    TecmoGameplayMovementState holder;
    TecmoGameplayCourtCoordinate position = {352,198};
    TecmoGameplayCourtCoordinate linked = {395,190};
    TecmoGameplayBallDribbleFrame frame;
    TecmoGameplayBallDribbleFrame before;
    bool ok = false;
    tecmo_gameplay_ball_dribble_assets_init(&assets);
    tecmo_gameplay_movement_assets_init(&movement_assets);
    if (!tecmo_gameplay_ball_dribble_assets_load(
            &assets, asset_pack_path) ||
        !tecmo_gameplay_movement_assets_load(
            &movement_assets, asset_pack_path) ||
        !tecmo_gameplay_movement_state_initialize(
            &movement_assets, &holder, &position, 0U)) {
        (void)snprintf(message, message_size, "%s",
                       assets.available ? movement_assets.status
                                        : assets.status);
        goto cleanup;
    }
    holder.animation_phase = 0x00U;
    if (!tecmo_gameplay_ball_dribble_resolve(
            &assets, &movement_assets, &holder, &linked, &frame) ||
        frame.contract_tag != TECMO_GAMEPLAY_BALL_DRIBBLE_FRAME_TAG ||
        frame.table_half != 0U || frame.direction != 0U ||
        frame.x_offset != 6 || frame.y_offset != 1 ||
        frame.bounce_height != 18U ||
        frame.base_position.x != 358 || frame.base_position.y != 199 ||
        frame.visible_position.x != 358 ||
        frame.visible_position.y != 181 || frame.sound_trigger) {
        (void)snprintf(message, message_size,
                       "TGBD-1 phase-0 bounce vector failed");
        goto cleanup;
    }
    holder.animation_phase = 0x03U;
    if (!tecmo_gameplay_ball_dribble_resolve(
            &assets, &movement_assets, &holder, &linked, &frame) ||
        frame.bounce_height != 0U ||
        frame.visible_position.y != 199 || !frame.sound_trigger) {
        (void)snprintf(message, message_size,
                       "TGBD-1 native sound-phase vector failed");
        goto cleanup;
    }
    holder.direction = 1U;
    holder.animation_phase = 0x00U;
    linked.y = holder.position.y;
    if (!tecmo_gameplay_ball_dribble_resolve(
            &assets, &movement_assets, &holder, &linked, &frame) ||
        frame.table_half != 1U || frame.x_offset != -4 ||
        frame.y_offset != 1 || frame.bounce_height != 18U ||
        frame.visible_position.x != 348 ||
        frame.visible_position.y != 181) {
        (void)snprintf(message, message_size,
                       "TGBD-1 alternate-half attachment vector failed");
        goto cleanup;
    }
    before = frame;
    holder.direction = 8U;
    if (tecmo_gameplay_ball_dribble_resolve(
            &assets, &movement_assets, &holder, &linked, &frame) ||
        memcmp(&frame, &before, sizeof(frame)) != 0) {
        (void)snprintf(message, message_size,
                       "TGBD-1 transactional state rejection failed");
        goto cleanup;
    }
    ok = true;
    (void)snprintf(message, message_size,
                   "TGBD-1 held-ball bounce self-test passed");
cleanup:
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    tecmo_gameplay_ball_dribble_assets_destroy(&assets);
    return ok;
}
