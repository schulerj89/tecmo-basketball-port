#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_dunk_cutaway.h"

#include "asset_pack/tecmo_asset_pack_gameplay_dunk_cutaway.h"
#include "tecmo_asset_pack.h"
#include "tecmo_nes_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_DUNK_LIFECYCLE_TAG 0x4B444754U
#define DUNK_GEOMETRY_CPU 0xB3EAU
#define DUNK_GEOMETRY_SIZE 2130U
#define DUNK_RENDER_FNV1A32 0xBA611C75U

static const uint8_t dunk_visible_frame[TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    28U, 32U, 37U, 42U, 47U, 52U, 57U
};

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
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayDunkCutawayAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->cells, 0, sizeof(assets->cells));
    memset(assets->stages, 0, sizeof(assets->stages));
    memset(assets->records, 0, sizeof(assets->records));
    assets->reference_palette = NULL;
    assets->chr_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGDK-1 rejected");
    return false;
}

void tecmo_gameplay_dunk_cutaway_init(
    TecmoGameplayDunkCutawayAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_DUNK_LIFECYCLE_TAG;
}

void tecmo_gameplay_dunk_cutaway_destroy(
    TecmoGameplayDunkCutawayAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_DUNK_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_dunk_cutaway_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE &&
           memcmp(payload, "TGDK", 4U) == 0 &&
           read_u16(payload + 4U) == TECMO_ASSET_PACK_GAMEPLAY_DUNK_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_HEADER_SIZE &&
           read_u32(payload + 8U) == TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE &&
           read_u16(payload + 12U) == TECMO_GAMEPLAY_DUNK_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET &&
           read_u32(payload + 32U) == TECMO_GAMEPLAY_DUNK_CELL_COUNT &&
           read_u16(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE &&
           read_u16(payload + 38U) == TECMO_GAMEPLAY_DUNK_SIDE_COUNT &&
           read_u32(payload + 40U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_SIZE &&
           read_u32(payload + 48U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET &&
           read_u16(payload + 52U) == TECMO_GAMEPLAY_DUNK_STAGE_COUNT &&
           read_u16(payload + 54U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE &&
           read_u32(payload + 56U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE &&
           read_u32(payload + 60U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32 &&
           read_u64(payload + 64U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A64 &&
           read_u32(payload + 72U) == 2048U &&
           read_u32(payload + 76U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_DECODED_FNV1A32 &&
           read_u32(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE0_FNV1A32 &&
           read_u32(payload + 84U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE1_FNV1A32 &&
           read_u32(payload + 96U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_BASE_PALETTE_FNV1A32 &&
           read_u32(payload + 100U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_BG_FNV1A32 &&
           read_u32(payload + 104U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_SPRITE_FNV1A32 &&
           read_u32(payload + 108U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_PALETTE_FNV1A32 &&
           read_u32(payload + 112U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTERS_FNV1A32 &&
           read_u32(payload + 116U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_ONLY_FNV1A32 &&
           read_u32(payload + 120U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_GEOMETRY_FNV1A32 &&
           read_u32(payload + 124U) ==
               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_GEOMETRY_FNV1A32 &&
           read_u16(payload + 128U) == TECMO_GAMEPLAY_DUNK_DISPATCH_FRAME &&
           read_u16(payload + 130U) ==
               TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME &&
           read_u16(payload + 132U) ==
               TECMO_GAMEPLAY_DUNK_FIRST_ASSIGN_FRAME &&
           read_u16(payload + 134U) ==
               TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME &&
           read_u16(payload + 136U) == TECMO_GAMEPLAY_DUNK_STAGE_CADENCE &&
           read_u16(payload + 138U) ==
               TECMO_GAMEPLAY_DUNK_LAST_ASSIGN_FRAME &&
           read_u16(payload + 140U) ==
               TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME &&
           read_u16(payload + 142U) ==
               TECMO_GAMEPLAY_DUNK_PALETTE_BLACK_FRAME &&
           read_u16(payload + 144U) == TECMO_GAMEPLAY_DUNK_CLEAR_FRAME &&
           read_u16(payload + 146U) ==
               TECMO_GAMEPLAY_DUNK_COURT_REBUILD_FRAME &&
           read_u16(payload + 148U) ==
               TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME &&
           read_u16(payload + 150U) ==
               TECMO_GAMEPLAY_DUNK_ROUTE_RESUME_FRAME &&
           read_u16(payload + 152U) == TECMO_GAMEPLAY_DUNK_DMC_FRAME &&
           read_u16(payload + 154U) == TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME &&
           payload[156U] == TECMO_GAMEPLAY_DUNK_MAX_PIECES &&
           payload[157U] == 0x30U && payload[158U] == 1U &&
           payload[159U] == 0xD0U && payload[160U] == 0x04U &&
           payload[161U] == 0xD4U &&
           bytes_are_zero(payload + 162U, 2U) &&
           bytes_are_zero(payload + 168U,
                          TECMO_ASSET_PACK_GAMEPLAY_DUNK_HEADER_SIZE - 168U);
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    size_t index;
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_SOURCE_COUNT; ++index) {
        const TecmoGameplayDunkExpectedSource *expected =
            &tecmo_gameplay_dunk_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCE_STRIDE;
        uint32_t end = (uint32_t)expected->cpu_start +
                       expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)end ||
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

static bool validate_cells_and_stages(const uint8_t *payload,
                                      size_t payload_size)
{
    size_t index;
    if (read_u32(payload + 88U) !=
            fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET,
                    TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_SIZE) ||
        read_u32(payload + 92U) !=
            fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET,
                    TECMO_GAMEPLAY_DUNK_CELL_COUNT *
                        TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE) ||
        read_u32(payload + 164U) !=
            fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET,
                    TECMO_GAMEPLAY_DUNK_STAGE_COUNT *
                        TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE) ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET,
                16U) != TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_BG_FNV1A32 ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET + 16U,
                16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_SPRITE_FNV1A32 ||
        fnv1a32(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET,
                32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_PALETTE_FNV1A32 ||
        !range_ok(TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET,
                  TECMO_GAMEPLAY_DUNK_STAGE_COUNT *
                      TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE,
                  payload_size)) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_CELL_COUNT; ++index) {
        const uint8_t *cell = payload +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE;
        size_t side = index / TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT;
        uint32_t offset = read_u32(cell);
        uint32_t page = offset / 1024U;
        uint32_t first_page = side == 0U ? 0xD0U : 0x04U;
        if (cell[4U] > 3U || cell[5U] != side ||
            !bytes_are_zero(cell + 6U, 2U) ||
            page < first_page || page > first_page + 3U ||
            offset + 16U > TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE) {
            return false;
        }
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++index) {
        const uint8_t *stage = payload +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE;
        uint8_t expected_page = index < 4U ? 0xD4U
                                : index < 6U ? 0xD8U : 0xDCU;
        if (read_u16(stage) !=
                TECMO_GAMEPLAY_DUNK_FIRST_ASSIGN_FRAME +
                    index * TECMO_GAMEPLAY_DUNK_STAGE_CADENCE ||
            read_u16(stage + 2U) != dunk_visible_frame[index] ||
            stage[7U] != expected_page || stage[8U] != index ||
            !bytes_are_zero(stage + 9U, 7U)) {
            return false;
        }
    }
    return true;
}

static bool load_records(TecmoGameplayDunkCutawayAssets *assets)
{
    const uint8_t *pointers = assets->storage +
        tecmo_gameplay_dunk_expected_sources[14].payload_offset;
    const uint8_t *geometry = assets->storage +
        tecmo_gameplay_dunk_expected_sources[15].payload_offset;
    size_t side;
    if (pointers[0U] != 0U || pointers[1U] != 0x12U ||
        fnv1a32(pointers + 2U, 36U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_ONLY_FNV1A32 ||
        fnv1a32(pointers, 38U + DUNK_GEOMETRY_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_GEOMETRY_FNV1A32) {
        return false;
    }
    for (side = 0U; side < TECMO_GAMEPLAY_DUNK_SIDE_COUNT; ++side) {
        size_t slot;
        size_t pointer_base = 2U + side * 18U;
        for (slot = 0U; slot < 9U; ++slot) {
            uint16_t pointer = read_u16(pointers + pointer_base + slot * 2U);
            if (slot >= TECMO_GAMEPLAY_DUNK_STAGE_COUNT) {
                if (pointer != 0U) return false;
                continue;
            }
            if (pointer < DUNK_GEOMETRY_CPU ||
                pointer > DUNK_GEOMETRY_CPU + DUNK_GEOMETRY_SIZE - 1U) {
                return false;
            }
            {
                size_t offset = (size_t)(pointer - DUNK_GEOMETRY_CPU);
                uint8_t count = geometry[offset];
                size_t size = 1U + (size_t)count * 4U;
                size_t piece;
                if (count == 0U || count > TECMO_GAMEPLAY_DUNK_MAX_PIECES ||
                    offset > DUNK_GEOMETRY_SIZE ||
                    size > DUNK_GEOMETRY_SIZE - offset) {
                    return false;
                }
                for (piece = 0U; piece < count; ++piece) {
                    const uint8_t *raw = geometry + offset + 1U + piece * 4U;
                    uint16_t tile = (uint16_t)raw[1U] + 1U;
                    uint32_t page = assets->stages[slot].sprite_chr_page +
                                    ((tile & 0xFEU) >> 6U);
                    uint32_t chr_offset = page * 1024U +
                                          (tile & 0x3EU) * 16U;
                    if ((tile & 1U) == 0U ||
                        page < assets->stages[slot].sprite_chr_page ||
                        page > assets->stages[slot].sprite_chr_page + 3U ||
                        chr_offset + 32U >
                            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE) {
                        return false;
                    }
                }
                assets->records[side][slot].piece_count = count;
                assets->records[side][slot].pieces = geometry + offset + 1U;
            }
        }
    }
    return true;
}

bool tecmo_gameplay_dunk_cutaway_parse(
    TecmoGameplayDunkCutawayAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *chr,
    size_t chr_size)
{
    uint8_t *storage;
    size_t index;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_DUNK_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_dunk_cutaway_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets, "TGDK-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_FNV1A32) {
        return reject(assets, "TGDK-1 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload, payload_size) ||
        !bytes_are_zero(
            payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_PADDING_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_PADDING_SIZE) ||
        !validate_cells_and_stages(payload, payload_size)) {
        return reject(assets, "TGDK-1 source/cell/stage contract rejected");
    }
    if (chr == NULL || chr_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE ||
        fnv1a32(chr, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32 ||
        fnv1a64(chr, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A64 ||
        fnv1a32(chr + 0xD0U * 1024U, 16U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIDE0_CHR_FNV1A32 ||
        fnv1a32(chr + 0x04U * 1024U, 4U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIDE1_BG_CHR_FNV1A32) {
        return reject(assets, "TGDK-1 same-pack chr/all dependency rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGDK-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_SOURCE_COUNT; ++index) {
        const TecmoGameplayDunkExpectedSource *expected =
            &tecmo_gameplay_dunk_expected_sources[index];
        TecmoGameplayDunkSourceSpan *source = &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_CELL_COUNT; ++index) {
        const uint8_t *cell = storage +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE;
        assets->cells[index].chr_offset = read_u32(cell);
        assets->cells[index].palette_index = cell[4U];
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++index) {
        const uint8_t *stage = storage +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE;
        assets->stages[index].assignment_frame = read_u16(stage);
        assets->stages[index].visible_frame = read_u16(stage + 2U);
        assets->stages[index].anchor_y = stage[4U];
        assets->stages[index].anchor_x[0U] = stage[5U];
        assets->stages[index].anchor_x[1U] = stage[6U];
        assets->stages[index].sprite_chr_page = stage[7U];
        assets->stages[index].record_slot = stage[8U];
    }
    if (!load_records(assets)) {
        return reject(assets, "TGDK-1 deep pointer/count/CHR contract rejected");
    }
    assets->reference_palette = storage +
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET;
    assets->chr_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TGDK-1 native dunk cutaway assetpack");
    return true;
}

bool tecmo_gameplay_dunk_cutaway_load(
    TecmoGameplayDunkCutawayAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint64_t payload_size = 0U;
    uint64_t chr_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_DUNK_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_dunk_cutaway_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_DUNK_ID,
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE,
            &payload, &payload_size) != 0) {
        return reject(assets,
                      "TGDK-1 gameplay/dunk-cutaway missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE,
            &chr, &chr_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(assets, "TGDK-1 chr/all missing or wrong-sized");
    }
    loaded = tecmo_gameplay_dunk_cutaway_parse(
        assets, payload, (size_t)payload_size, chr, (size_t)chr_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    return loaded;
}

bool tecmo_gameplay_dunk_cutaway_palette(
    const TecmoGameplayDunkCutawayAssets *assets,
    uint8_t profile,
    uint8_t uniform_color,
    uint8_t palette[32])
{
    const uint8_t *base;
    const uint8_t *recipe;
    const uint8_t *group;
    size_t group_offset;
    if (assets == NULL || !assets->available || palette == NULL ||
        profile > 1U || uniform_color > 0x3FU) {
        return false;
    }
    base = assets->sources[5U].bytes;
    recipe = assets->sources[16U].bytes;
    group_offset = profile == 0U ? (0xB138U - 0xB0EDU)
                                 : (0xB148U - 0xB0EDU);
    if (group_offset + 16U > assets->sources[16U].byte_count) return false;
    group = recipe + group_offset;
    memcpy(palette, base, 16U);
    memcpy(palette + 16U, group, 16U);
    palette[4U] = group[0U];
    palette[8U] = group[0U];
    palette[12U] = group[0U];
    palette[16U] = palette[0U];
    palette[22U] = uniform_color;
    palette[23U] = uniform_color;
    palette[25U] = uniform_color;
    palette[29U] = (uint8_t)((uniform_color + 0x10U) & 0x3FU);
    return true;
}

bool tecmo_gameplay_dunk_cutaway_stage_for_frame(
    const TecmoGameplayDunkCutawayAssets *assets,
    uint16_t frame,
    uint8_t *stage_index)
{
    size_t index;
    uint8_t selected = 0U;
    if (assets == NULL || !assets->available || stage_index == NULL ||
        frame < TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME ||
        frame > TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++index) {
        if (frame < assets->stages[index].visible_frame) break;
        selected = (uint8_t)index;
    }
    *stage_index = selected;
    return true;
}

static bool framebuffer_valid(const TecmoFramebuffer *framebuffer,
                              int origin_x, int origin_y, int scale)
{
    return framebuffer != NULL && framebuffer->pixels != NULL && scale > 0 &&
           origin_x >= 0 && origin_y >= 0 &&
           framebuffer->width >= origin_x + 256 * scale &&
           framebuffer->height >= origin_y + 240 * scale &&
           framebuffer->pitch_pixels >= framebuffer->width;
}

static void fill_rect(TecmoFramebuffer *framebuffer, int x, int y,
                      int width, int height, uint32_t color)
{
    int row;
    for (row = 0; row < height; ++row) {
        uint32_t *pixels = framebuffer->pixels +
            (size_t)(y + row) * (size_t)framebuffer->pitch_pixels +
            (size_t)x;
        int column;
        for (column = 0; column < width; ++column) pixels[column] = color;
    }
}

bool tecmo_gameplay_dunk_cutaway_draw(
    const TecmoGameplayDunkCutawayAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint8_t side,
    uint8_t profile,
    uint8_t uniform_color,
    uint8_t stage_index)
{
    uint8_t palette_bytes[32];
    const TecmoGameplayDunkStage *stage;
    const TecmoGameplayDunkRecord *record;
    size_t index;
    if (assets == NULL || !assets->available ||
        side >= TECMO_GAMEPLAY_DUNK_SIDE_COUNT ||
        stage_index >= TECMO_GAMEPLAY_DUNK_STAGE_COUNT ||
        chr == NULL || chr_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE ||
        fnv1a32(chr, chr_size) != assets->chr_fingerprint ||
        !framebuffer_valid(framebuffer, origin_x, origin_y, scale) ||
        !tecmo_gameplay_dunk_cutaway_palette(
            assets, profile, uniform_color, palette_bytes)) {
        return false;
    }
    stage = &assets->stages[stage_index];
    record = &assets->records[side][stage->record_slot];
    if (record->piece_count == 0U ||
        record->piece_count > TECMO_GAMEPLAY_DUNK_MAX_PIECES) {
        return false;
    }
    for (index = 0U; index < record->piece_count; ++index) {
        const uint8_t *piece = record->pieces + index * 4U;
        uint16_t tile = (uint16_t)piece[1U] + 1U;
        uint32_t page = stage->sprite_chr_page + ((tile & 0xFEU) >> 6U);
        uint32_t offset = page * 1024U + (tile & 0x3EU) * 16U;
        if ((tile & 1U) == 0U || offset + 32U > chr_size ||
            (piece[2U] & 0x3CU) != 0U) {
            return false;
        }
    }

    fill_rect(framebuffer, origin_x, origin_y, 256 * scale, 240 * scale,
              tecmo_nes_2c02_rgba(palette_bytes[0U]));
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT; ++index) {
        const TecmoGameplayDunkCell *cell = &assets->cells[
            (size_t)side * TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT + index];
        uint32_t palette[4] = {0U, 0U, 0U, 0U};
        size_t color;
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(
                palette_bytes[(size_t)cell->palette_index * 4U + color]);
        }
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, chr, chr_size, cell->chr_offset,
            origin_x + (int)(index % 32U) * 8 * scale,
            origin_y + (int)(index / 32U) * 8 * scale,
            scale, palette, false, false);
    }
    for (index = record->piece_count; index-- > 0U;) {
        const uint8_t *piece = record->pieces + index * 4U;
        uint16_t tile = (uint16_t)piece[1U] + 1U;
        uint32_t page = stage->sprite_chr_page + ((tile & 0xFEU) >> 6U);
        uint32_t top_offset = page * 1024U + (tile & 0x3EU) * 16U;
        uint32_t bottom_offset = top_offset + 16U;
        uint8_t attributes = piece[2U];
        uint32_t palette[4] = {0U, 0U, 0U, 0U};
        bool flip_horizontal = (attributes & 0x40U) != 0U;
        bool flip_vertical = (attributes & 0x80U) != 0U;
        int x = origin_x +
            (int)(uint8_t)(stage->anchor_x[side] + piece[3U]) * scale;
        int y = origin_y +
            (int)(uint8_t)(stage->anchor_y + piece[0U] + 1U) * scale;
        size_t color;
        for (color = 1U; color < 4U; ++color) {
            palette[color] = tecmo_nes_2c02_rgba(
                palette_bytes[16U + (size_t)(attributes & 3U) * 4U + color]);
        }
        if (flip_vertical) {
            uint32_t swap = top_offset;
            top_offset = bottom_offset;
            bottom_offset = swap;
        }
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, chr, chr_size, top_offset, x, y, scale,
            palette, flip_horizontal, flip_vertical);
        tecmo_draw_chr_tile_at_offset_ex(
            framebuffer, chr, chr_size, bottom_offset,
            x, y + 8 * scale, scale, palette,
            flip_horizontal, flip_vertical);
    }
    return true;
}

static uint32_t pixels_fnv1a32(uint32_t hash, const uint32_t *pixels,
                               size_t pixel_count)
{
    size_t pixel;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        unsigned shift;
        for (shift = 0U; shift < 32U; shift += 8U) {
            hash ^= (pixels[pixel] >> shift) & 0xFFU;
            hash *= 16777619U;
        }
    }
    return hash;
}

static bool pixels_equal(const uint32_t *pixels, size_t pixel_count,
                         uint32_t value)
{
    size_t pixel;
    for (pixel = 0U; pixel < pixel_count; ++pixel) {
        if (pixels[pixel] != value) return false;
    }
    return true;
}

static bool self_test_reject(TecmoGameplayDunkCutawayAssets *assets,
                             uint8_t *chr, uint32_t *pixels,
                             char *message, size_t message_size,
                             const char *failure)
{
    if (message != NULL && message_size != 0U) {
        (void)snprintf(message, message_size, "%s", failure);
    }
    free(pixels);
    tecmo_asset_pack_free(chr);
    tecmo_gameplay_dunk_cutaway_destroy(assets);
    return false;
}

bool tecmo_gameplay_dunk_cutaway_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    static const uint8_t record_counts[TECMO_GAMEPLAY_DUNK_SIDE_COUNT]
                                      [TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
        {10U, 29U, 39U, 39U, 51U, 49U, 48U},
        {10U, 29U, 39U, 39U, 51U, 49U, 47U}
    };
    TecmoGameplayDunkCutawayAssets assets;
    TecmoFramebuffer framebuffer;
    uint8_t *chr = NULL;
    uint64_t chr_size = 0U;
    uint32_t *pixels = NULL;
    uint32_t render_hash = 2166136261U;
    uint8_t palette[32];
    uint8_t stage_index;
    size_t side;
    size_t stage;
    size_t frame;
    const size_t pixel_count = 256U * 240U;

    tecmo_gameplay_dunk_cutaway_init(&assets);
    if (tecmo_gameplay_dunk_cutaway_stage_for_frame(
            &assets, TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME,
            &stage_index) ||
        tecmo_gameplay_dunk_cutaway_palette(&assets, 1U, 0x30U,
                                            palette) ||
        asset_pack_path == NULL ||
        !tecmo_gameplay_dunk_cutaway_load(&assets, asset_pack_path) ||
        !tecmo_gameplay_dunk_cutaway_load(&assets, asset_pack_path)) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                asset_pack_path == NULL
                                    ? "TGDK-1 asset pack path required"
                                    : assets.status);
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE,
            &chr, &chr_size) != 0 ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                "TGDK-1 self-test chr/all read failed");
    }
    if (assets.storage_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE ||
        fnv1a32(assets.storage, assets.storage_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_FNV1A32 ||
        !tecmo_gameplay_dunk_cutaway_palette(&assets, 1U, 0x30U,
                                             palette) ||
        memcmp(palette, assets.reference_palette, sizeof(palette)) != 0 ||
        tecmo_gameplay_dunk_cutaway_palette(&assets, 2U, 0x30U,
                                            palette) ||
        tecmo_gameplay_dunk_cutaway_palette(&assets, 1U, 0x40U,
                                            palette) ||
        tecmo_gameplay_dunk_cutaway_stage_for_frame(
            &assets, TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME - 1U,
            &stage_index) ||
        tecmo_gameplay_dunk_cutaway_stage_for_frame(
            &assets, TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME + 1U,
            &stage_index)) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                "TGDK-1 parser/palette boundary failed");
    }
    for (frame = TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME;
         frame <= TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME; ++frame) {
        uint8_t expected = 0U;
        size_t expected_stage;
        for (expected_stage = 1U;
             expected_stage < TECMO_GAMEPLAY_DUNK_STAGE_COUNT;
             ++expected_stage) {
            if (frame < dunk_visible_frame[expected_stage]) break;
            expected = (uint8_t)expected_stage;
        }
        if (!tecmo_gameplay_dunk_cutaway_stage_for_frame(
                &assets, (uint16_t)frame, &stage_index) ||
            stage_index != expected) {
            return self_test_reject(
                &assets, chr, pixels, message, message_size,
                "TGDK-1 visible-frame stage schedule failed");
        }
    }
    for (side = 0U; side < TECMO_GAMEPLAY_DUNK_SIDE_COUNT; ++side) {
        for (stage = 0U; stage < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++stage) {
            if (assets.records[side][stage].piece_count !=
                    record_counts[side][stage] ||
                assets.records[side][stage].pieces == NULL) {
                return self_test_reject(
                    &assets, chr, pixels, message, message_size,
                    "TGDK-1 side/stage geometry contract failed");
            }
        }
    }

    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    if (pixels == NULL) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                "TGDK-1 render allocation failed");
    }
    framebuffer.pixels = pixels;
    framebuffer.width = 256;
    framebuffer.height = 240;
    framebuffer.pitch_pixels = 256;
    for (side = 0U; side < TECMO_GAMEPLAY_DUNK_SIDE_COUNT; ++side) {
        for (stage = 0U; stage < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++stage) {
            memset(pixels, 0xA5, pixel_count * sizeof(*pixels));
            if (!tecmo_gameplay_dunk_cutaway_draw(
                    &assets, chr, (size_t)chr_size, &framebuffer,
                    0, 0, 1, (uint8_t)side, 1U, 0x30U,
                    (uint8_t)stage)) {
                return self_test_reject(&assets, chr, pixels, message,
                                        message_size,
                                        "TGDK-1 canonical render rejected");
            }
            render_hash = pixels_fnv1a32(render_hash, pixels, pixel_count);
        }
    }
    memset(pixels, 0xA5, pixel_count * sizeof(*pixels));
    assets.records[0U][0U].piece_count =
        TECMO_GAMEPLAY_DUNK_MAX_PIECES + 1U;
    if (tecmo_gameplay_dunk_cutaway_draw(
            &assets, chr, (size_t)chr_size, &framebuffer,
            0, 0, 1, 0U, 1U, 0x30U, 0U) ||
        !pixels_equal(pixels, pixel_count, 0xA5A5A5A5U)) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                "TGDK-1 failed render modified pixels");
    }
    assets.records[0U][0U].piece_count = record_counts[0U][0U];
    framebuffer.width = 255;
    if (tecmo_gameplay_dunk_cutaway_draw(
            &assets, chr, (size_t)chr_size, &framebuffer,
            0, 0, 1, 0U, 1U, 0x30U, 0U)) {
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                "TGDK-1 undersized framebuffer accepted");
    }
    if (render_hash != DUNK_RENDER_FNV1A32) {
        char failure[128];
        (void)snprintf(failure, sizeof(failure),
                       "TGDK-1 render fingerprint mismatch: %08X",
                       (unsigned)render_hash);
        return self_test_reject(&assets, chr, pixels, message, message_size,
                                failure);
    }
    free(pixels);
    tecmo_asset_pack_free(chr);
    tecmo_gameplay_dunk_cutaway_destroy(&assets);
    tecmo_gameplay_dunk_cutaway_destroy(&assets);
    if (message != NULL && message_size != 0U) {
        (void)snprintf(
            message, message_size,
            "TGDK-1 dunk cutaway passed: sources=18 cells=1920 stages=7 sides=2 palette=939EBCBE render=%08X",
            (unsigned)render_hash);
    }
    return true;
}
