#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_assets.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_ASSETS_LIFECYCLE_TAG 0x5447504CU

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
    return (uint64_t)read_u32(bytes) |
           ((uint64_t)read_u32(bytes + 4U) << 32U);
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

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
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

static bool reject(TecmoGameplayAssets *assets, const char *message)
{
    free(assets->storage);
    free(assets->chr_storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->chr_storage = NULL;
    assets->chr_storage_size = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGPL-1 rejected");
    return false;
}

void tecmo_gameplay_assets_init(TecmoGameplayAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_ASSETS_LIFECYCLE_TAG;
}

void tecmo_gameplay_assets_destroy(TecmoGameplayAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_ASSETS_LIFECYCLE_TAG) return;
    free(assets->storage);
    free(assets->chr_storage);
    tecmo_gameplay_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(payload, "TGPL", 4U) == 0 &&
           read_u16(payload + 4U) == TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           read_u16(payload + 6U) == TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE &&
           read_u32(payload + 8U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u16(payload + 12U) == TECMO_GAMEPLAY_ASSET_SCREEN_COUNT &&
           read_u16(payload + 14U) == TECMO_GAMEPLAY_ASSET_SOURCE_COUNT &&
           read_u16(payload + 16U) == TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE &&
           read_u16(payload + 18U) == TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE &&
           read_u32(payload + 20U) == TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET &&
           read_u32(payload + 24U) == TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET &&
           read_u32(payload + 28U) == TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET &&
           read_u32(payload + 32U) == TECMO_ASSET_PACK_GAMEPLAY_ENCODED_SIZE &&
           read_u32(payload + 36U) == TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET &&
           read_u32(payload + 40U) == TECMO_ASSET_PACK_GAMEPLAY_DECODED_SIZE &&
           read_u32(payload + 44U) == TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET &&
           read_u32(payload + 48U) == TECMO_ASSET_PACK_GAMEPLAY_PALETTES_SIZE &&
           read_u32(payload + 52U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET &&
           read_u32(payload + 56U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE &&
           read_u32(payload + 60U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET &&
           read_u32(payload + 64U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_SIZE &&
           read_u32(payload + 68U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_OFFSET &&
           read_u32(payload + 72U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_SIZE &&
           read_u32(payload + 76U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_OFFSET &&
           read_u32(payload + 80U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_SIZE &&
           read_u32(payload + 84U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_POINTERS_OFFSET &&
           read_u32(payload + 88U) == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_POINTERS_SIZE &&
           read_u32(payload + 92U) == TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET &&
           read_u32(payload + 96U) == TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE &&
           read_u32(payload + 100U) == TECMO_ASSET_PACK_GAMEPLAY_RENDERER_OFFSET &&
           read_u32(payload + 104U) == TECMO_ASSET_PACK_GAMEPLAY_RENDERER_SIZE &&
           read_u32(payload + 108U) == TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_OFFSET &&
           read_u32(payload + 112U) == TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_SIZE &&
           read_u32(payload + 116U) == TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET &&
           read_u32(payload + 120U) == TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_SIZE &&
           read_u32(payload + 124U) == TECMO_ASSET_PACK_GAMEPLAY_RULES_OFFSET &&
           read_u32(payload + 128U) == TECMO_ASSET_PACK_GAMEPLAY_RULES_SIZE &&
           read_u32(payload + 132U) == TECMO_ASSET_PACK_GAMEPLAY_PERIOD_OFFSET &&
           read_u32(payload + 136U) == TECMO_ASSET_PACK_GAMEPLAY_PERIOD_SIZE &&
           read_u32(payload + 140U) == TECMO_ASSET_PACK_GAMEPLAY_EVENTS_OFFSET &&
           read_u32(payload + 144U) == TECMO_ASSET_PACK_GAMEPLAY_EVENTS_SIZE &&
           read_u32(payload + 148U) == TECMO_ASSET_PACK_GAMEPLAY_LIVE_OFFSET &&
           read_u32(payload + 152U) == TECMO_ASSET_PACK_GAMEPLAY_LIVE_SIZE &&
           read_u32(payload + 156U) == TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE &&
           read_u32(payload + 160U) == TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 &&
           read_u64(payload + 164U) == TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64 &&
           read_u16(payload + 172U) == TECMO_GAMEPLAY_ASSET_POINTER_COUNT &&
           read_u16(payload + 174U) == TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT &&
           read_u16(payload + 176U) == TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT &&
           read_u16(payload + 178U) == TECMO_GAMEPLAY_ASSET_RULE_COUNT &&
           read_u16(payload + 180U) == TECMO_GAMEPLAY_LIVE_BAND_COUNT &&
           read_u16(payload + 182U) == 0U &&
           read_u32(payload + 184U) == 0x41472384U &&
           read_u32(payload + 188U) == 0xABB3133DU &&
           read_u32(payload + 192U) == 0x133461D5U &&
           read_u32(payload + 196U) == 0x55B90F03U &&
           read_u32(payload + 200U) == 0xFCB596AFU &&
           read_u32(payload + 204U) == 0x740B4855U &&
           read_u32(payload + 208U) == 0xD487D107U &&
           read_u32(payload + 212U) == 0xA93E123BU &&
           read_u32(payload + 216U) == 0x6E11429DU &&
           read_u32(payload + 220U) == 0x5041054FU &&
           read_u32(payload + 224U) == 0xE77E8F44U &&
           read_u32(payload + 228U) == 0xF4C8965CU &&
           read_u32(payload + 232U) == 0x76401D85U &&
           read_u32(payload + 236U) == 0x0000001FU &&
           read_u32(payload + 240U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_OFFSET &&
           read_u32(payload + 244U) ==
               TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_SIZE &&
           read_u32(payload + 248U) == 0xBBA0B94BU &&
           read_u32(payload + 252U) == 0x2397C99BU;
}

static bool validate_screen_records(const uint8_t *payload,
                                    size_t payload_size)
{
    size_t encoded_offset = TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET;
    size_t decoded_offset = TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET;
    size_t palette_offset = TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET;
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++index) {
        const TecmoGameplayExpectedScreen *expected =
            &tecmo_gameplay_expected_screens[index];
        const uint8_t *record = payload + TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET +
                                index * TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE;
        if (record[0U] != expected->screen_id ||
            record[1U] != expected->source_bank ||
            memcmp(record + 2U, expected->descriptor, 7U) != 0 ||
            record[9U] != 0U || read_u16(record + 10U) != expected->descriptor_cpu ||
            read_u16(record + 12U) != expected->stream_cpu ||
            read_u16(record + 14U) != expected->palette_cpu ||
            read_u32(record + 16U) != encoded_offset ||
            read_u32(record + 20U) != expected->encoded_size ||
            read_u32(record + 24U) != decoded_offset ||
            read_u32(record + 28U) != 2048U ||
            read_u32(record + 32U) != palette_offset ||
            read_u32(record + 36U) != 16U ||
            read_u32(record + 40U) != 1U ||
            read_u32(record + 44U) != 0U ||
            read_u32(record + 48U) != expected->descriptor_fingerprint ||
            read_u32(record + 52U) != expected->encoded_fingerprint ||
            read_u32(record + 56U) != expected->decoded_fingerprint ||
            read_u32(record + 60U) != expected->palette_fingerprint ||
            !range_ok(encoded_offset, expected->encoded_size, payload_size) ||
            !range_ok(decoded_offset, 2048U, payload_size) ||
            !range_ok(palette_offset, 16U, payload_size) ||
            fnv1a32(payload + encoded_offset, expected->encoded_size) !=
                expected->encoded_fingerprint ||
            fnv1a32(payload + decoded_offset, 2048U) !=
                expected->decoded_fingerprint ||
            fnv1a32(payload + palette_offset, 16U) !=
                expected->palette_fingerprint) {
            return false;
        }
        for (size_t color = 0U; color < 16U; ++color) {
            if (payload[palette_offset + color] > 0x3FU) return false;
        }
        encoded_offset += expected->encoded_size;
        decoded_offset += 2048U;
        palette_offset += 16U;
    }
    return encoded_offset == TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET &&
           decoded_offset == TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET &&
           palette_offset == TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET;
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        const TecmoGameplayExpectedSource *expected =
            &tecmo_gameplay_expected_sources[index];
        const uint8_t *record = payload + TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET +
                                index * TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank || record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != 0U ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->payload_offset ||
            read_u32(record + 16U) != expected->fingerprint ||
            read_u16(record + 20U) != (uint16_t)cpu_end ||
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

static bool validate_actor_records(const uint8_t *payload)
{
    const uint8_t *records =
        payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET;
    const uint8_t *pointers =
        payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET;
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_POINTER_COUNT; ++index) {
        uint16_t target = read_u16(pointers + index * 2U);
        size_t offset;
        uint8_t dimensions;
        size_t cells;
        if (target < 0x8000U || target >= 0xA5B9U) return false;
        offset = (size_t)(target - 0x8000U);
        dimensions = records[offset];
        cells = (size_t)(dimensions & 0x0FU) * (dimensions >> 4U);
        if ((dimensions >> 4U) == 0U || (dimensions & 0x0FU) == 0U ||
            cells > TECMO_GAMEPLAY_RESOLVED_PIECE_MAX ||
            offset + 4U > TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE ||
            cells > TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE - offset - 4U) {
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_assets_parse(TecmoGameplayAssets *assets,
                                 const uint8_t *payload,
                                 size_t payload_size,
                                 const uint8_t *chr_bytes,
                                 size_t chr_size)
{
    uint8_t *storage;
    uint8_t *chr_storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_ASSETS_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_assets_destroy(assets);
    if (payload == NULL || chr_bytes == NULL ||
        !validate_header(payload, payload_size)) {
        return reject(assets, "TGPL-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) {
        return reject(assets, "TGPL-1 canonical payload fingerprint rejected");
    }
    if (!validate_screen_records(payload, payload_size) ||
        !validate_source_records(payload, payload_size) ||
        !validate_actor_records(payload) ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_RULES_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_RULES_SIZE) != 0x5041054FU ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_PERIOD_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_PERIOD_SIZE) != 0xE77E8F44U ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_EVENTS_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_EVENTS_SIZE) != 0xF4C8965CU ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_LIVE_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_LIVE_SIZE) != 0x76401D85U ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_OFFSET,
                TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_SIZE +
                    TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_SIZE) !=
            0x2397C99BU) {
        return reject(assets, "TGPL-1 screen/source/actor contract rejected");
    }
    if (chr_size != TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE ||
        fnv1a32(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 ||
        fnv1a64(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64) {
        return reject(assets, "TGPL-1 same-pack chr/all dependency rejected");
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT; ++index) {
        if (payload[TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET + index] !=
            (uint8_t)(0x40U + index)) {
            return reject(assets, "TGPL-1 sprite R2 selector metadata rejected");
        }
    }
    for (size_t index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE; ++index) {
        if (payload[TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET + index] >
            0x3FU) {
            return reject(assets, "TGPL-1 actor palette group rejected");
        }
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGPL-1 allocation failed");
    chr_storage = (uint8_t *)malloc(chr_size);
    if (chr_storage == NULL) {
        free(storage);
        return reject(assets, "TGPL-1 CHR allocation failed");
    }
    memcpy(storage, payload, payload_size);
    memcpy(chr_storage, chr_bytes, chr_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    assets->chr_storage = chr_storage;
    assets->chr_storage_size = chr_size;
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++index) {
        const TecmoGameplayExpectedScreen *expected =
            &tecmo_gameplay_expected_screens[index];
        const uint8_t *record = storage + TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET +
                                index * TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE;
        TecmoGameplayScreenAsset *screen = &assets->screens[index];
        screen->screen_id = record[0U];
        screen->source_bank = record[1U];
        memcpy(screen->descriptor, record + 2U, 7U);
        screen->descriptor_cpu = read_u16(record + 10U);
        screen->stream_cpu = read_u16(record + 12U);
        screen->palette_cpu = read_u16(record + 14U);
        screen->encoded_size = expected->encoded_size;
        screen->decoded_size = 2048U;
        screen->descriptor_fingerprint = read_u32(record + 48U);
        screen->encoded_fingerprint = read_u32(record + 52U);
        screen->decoded_fingerprint = read_u32(record + 56U);
        screen->palette_fingerprint = read_u32(record + 60U);
        screen->encoded_stream = storage + read_u32(record + 16U);
        screen->nametables = storage + read_u32(record + 24U);
        screen->palette = storage + read_u32(record + 32U);
    }
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        const TecmoGameplayExpectedSource *expected =
            &tecmo_gameplay_expected_sources[index];
        TecmoGameplaySourceSpan *source = &assets->sources[index];
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
    assets->actor_records =
        storage + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET;
    assets->actor_records_size = TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE;
    assets->actor_pointers =
        storage + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET;
    assets->actor_pointers_size =
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_SIZE;
    assets->actor_palette_groups =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET;
    assets->actor_palette_groups_size =
        TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE;
    assets->r2_selectors =
        storage + TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET;
    assets->r2_selector_count = TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT;
    {
        static const uint8_t starts[TECMO_GAMEPLAY_LIVE_BAND_COUNT] = {
            0U, 32U, 48U, 80U, 128U, 176U
        };
        memcpy(assets->live_band_starts, starts, sizeof(starts));
        memcpy(assets->live_band_defaults,
               storage + tecmo_gameplay_expected_sources[
                   TECMO_GAMEPLAY_ASSET_SOURCE_COUNT - 1U].payload_offset + 27U,
               12U);
    }
    assets->chr_fingerprint32 = TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32;
    assets->chr_fingerprint64 = TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGPL-1 gameplay assetpack");
    return true;
}

bool tecmo_gameplay_assets_load(TecmoGameplayAssets *assets,
                                const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint64_t payload_size = 0U;
    uint64_t chr_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_ASSETS_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE, &payload, &payload_size) != 0) {
        return reject(assets, "TGPL-1 gameplay/core entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all", TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE,
            &chr, &chr_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(assets, "TGPL-1 chr/all dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_assets_parse(
        assets, payload, (size_t)payload_size, chr, (size_t)chr_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    return loaded;
}

const TecmoGameplaySourceSpan *tecmo_gameplay_assets_find_source(
    const TecmoGameplayAssets *assets,
    TecmoGameplaySourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) return &assets->sources[index];
    }
    return NULL;
}

bool tecmo_gameplay_assets_resolve_pose(
    const TecmoGameplayAssets *assets,
    uint16_t pointer_index,
    const TecmoGameplayPoseContext *context,
    TecmoGameplayResolvedPose *pose)
{
    uint16_t target;
    size_t record_offset;
    const uint8_t *record;
    unsigned columns;
    unsigned rows;
    size_t cell_count;
    size_t output = 0U;
    if (assets == NULL || context == NULL || pose == NULL ||
        !assets->available ||
        pointer_index >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT ||
        context->palette_group >= TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT ||
        (context->actor_slot_base & 0x3FU) != 0x01U ||
        (context->actor_attributes & 0xFCU) != 0U) {
        return false;
    }
    target = read_u16(assets->actor_pointers + pointer_index * 2U);
    if (target < 0x8000U || target >= 0xA5B9U) return false;
    record_offset = (size_t)(target - 0x8000U);
    record = assets->actor_records + record_offset;
    columns = record[0U] & 0x0FU;
    rows = record[0U] >> 4U;
    cell_count = (size_t)columns * rows;
    if (columns == 0U || rows == 0U ||
        cell_count > TECMO_GAMEPLAY_RESOLVED_PIECE_MAX ||
        record_offset + 4U > assets->actor_records_size ||
        cell_count > assets->actor_records_size - record_offset - 4U) {
        return false;
    }
    memset(pose, 0, sizeof(*pose));
    pose->pointer_index = pointer_index;
    pose->pointer_cpu = (uint16_t)(0xA5B9U + pointer_index * 2U);
    pose->record_cpu = target;
    pose->columns = (uint8_t)columns;
    pose->rows = (uint8_t)rows;
    pose->record_tag = record[3U];
    pose->actor_slot_base = context->actor_slot_base;
    pose->actor_attributes = context->actor_attributes;
    pose->palette_group = context->palette_group;
    memcpy(pose->mmc3_r2_r5, context->mmc3_r2_r5,
           sizeof(pose->mmc3_r2_r5));
    for (size_t cell = 0U; cell < cell_count; ++cell) {
        uint8_t cell_byte = record[4U + cell];
        uint8_t tile;
        uint8_t attributes;
        uint8_t selector;
        uint32_t top;
        TecmoGameplayResolvedPiece *piece;
        /* Fixed $D498 uses BPL: every cell with bit 7 set is blank. */
        if ((cell_byte & 0x80U) != 0U) continue;
        tile = (uint8_t)((cell_byte & 0x3EU) +
                         context->actor_slot_base);
        selector = context->mmc3_r2_r5[(tile >> 6U) & 3U];
        top = (uint32_t)selector * 1024U +
              (uint32_t)(tile & 0x3EU) * 16U;
        if (top + 32U > assets->chr_storage_size ||
            output >= TECMO_GAMEPLAY_RESOLVED_PIECE_MAX) {
            return false;
        }
        attributes = (uint8_t)((cell_byte & 0x41U) |
                               context->actor_attributes);
        piece = &pose->pieces[output++];
        piece->dx = (int16_t)(int8_t)record[1U] +
                    (int16_t)((cell % columns) * 8U);
        piece->dy = (int16_t)(int8_t)record[2U] +
                    (int16_t)((cell / columns) * 16U);
        piece->cell_byte = cell_byte;
        piece->tile_id = tile;
        piece->oam_attributes = attributes;
        piece->palette_index = attributes & 3U;
        piece->flip_horizontal = (attributes & 0x40U) != 0U;
        piece->top_chr_offset = top;
        piece->bottom_chr_offset = top + 16U;
        piece->top_chr = assets->chr_storage + top;
        piece->bottom_chr = assets->chr_storage + top + 16U;
        piece->palette = assets->actor_palette_groups +
                         (size_t)context->palette_group * 16U +
                         (size_t)piece->palette_index * 4U;
    }
    pose->piece_count = (uint8_t)output;
    return true;
}

bool tecmo_gameplay_assets_build_live_background_context(
    const TecmoGameplayAssets *assets,
    uint8_t final_r1_selector,
    TecmoGameplayLiveBackgroundContext *context)
{
    if (assets == NULL || context == NULL || !assets->available ||
        final_r1_selector < 0x40U || final_r1_selector > 0x5AU) {
        return false;
    }
    for (size_t band = 0U; band < TECMO_GAMEPLAY_LIVE_BAND_COUNT; ++band) {
        context->pre_asl_r0[band] = assets->live_band_defaults[band * 2U];
        context->pre_asl_r1[band] = assets->live_band_defaults[band * 2U + 1U];
    }
    context->pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT - 1U] =
        final_r1_selector;
    return true;
}

uint8_t tecmo_gameplay_assets_live_band_for_scanline(uint8_t scanline)
{
    static const uint8_t starts[TECMO_GAMEPLAY_LIVE_BAND_COUNT] = {
        0U, 32U, 48U, 80U, 128U, 176U
    };
    uint8_t band = 0U;
    for (uint8_t index = 1U;
         index < TECMO_GAMEPLAY_LIVE_BAND_COUNT; ++index) {
        if (scanline < starts[index]) break;
        band = index;
    }
    return band;
}

bool tecmo_gameplay_assets_resolve_live_orientation_tile(
    const TecmoGameplayAssets *assets,
    uint8_t screen_index,
    uint8_t nametable_page,
    uint8_t row,
    uint8_t column,
    uint8_t scanline,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayResolvedOrientationTile *tile)
{
    const TecmoGameplayScreenAsset *screen;
    size_t nametable;
    size_t attribute;
    unsigned shift;
    uint8_t tile_id;
    uint8_t palette_index;
    uint8_t band;
    uint8_t pre_asl;
    uint8_t mmc3_bank;
    uint32_t chr_offset;
    if (assets == NULL || context == NULL || tile == NULL ||
        !assets->available || screen_index >= TECMO_GAMEPLAY_ASSET_SCREEN_COUNT ||
        nametable_page >= 2U || row >= 30U || column >= 32U ||
        scanline >= 240U) {
        return false;
    }
    screen = &assets->screens[screen_index];
    nametable = (size_t)nametable_page * 1024U;
    tile_id = screen->nametables[nametable + (size_t)row * 32U + column];
    attribute = nametable + 0x3C0U + (size_t)(row / 4U) * 8U + column / 4U;
    shift = ((row & 2U) != 0U ? 4U : 0U) +
            ((column & 2U) != 0U ? 2U : 0U);
    palette_index = (uint8_t)((screen->nametables[attribute] >> shift) & 3U);
    band = tecmo_gameplay_assets_live_band_for_scanline(scanline);
    pre_asl = tile_id < 0x80U ? context->pre_asl_r0[band]
                              : context->pre_asl_r1[band];
    mmc3_bank = (uint8_t)(pre_asl << 1U);
    chr_offset = (uint32_t)mmc3_bank * 1024U +
                 (uint32_t)(tile_id & 0x7FU) * 16U;
    if (chr_offset + 16U > assets->chr_storage_size) return false;
    memset(tile, 0, sizeof(*tile));
    tile->screen_id = screen->screen_id;
    tile->nametable_page = nametable_page;
    tile->row = row;
    tile->column = column;
    tile->band = band;
    tile->tile_id = tile_id;
    tile->palette_index = palette_index;
    tile->mmc3_bank = mmc3_bank;
    tile->chr_offset = chr_offset;
    tile->chr = assets->chr_storage + chr_offset;
    tile->palette = screen->palette + (size_t)palette_index * 4U;
    return true;
}
