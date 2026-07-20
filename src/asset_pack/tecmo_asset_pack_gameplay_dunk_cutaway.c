#include "tecmo_asset_pack_gameplay_dunk_cutaway.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define DUNK_PRG_BANK_COUNT 8U
#define DUNK_DECODED_SIZE 2048U
#define DUNK_PAGE_SIZE 1024U
#define DUNK_SCREEN_STREAM_CPU 0x9022U
#define DUNK_SCREEN_STREAM_SIZE 805U
#define DUNK_SCREEN_PALETTE_CPU 0x9346U
#define DUNK_GEOMETRY_CPU 0xB3EAU
#define DUNK_GEOMETRY_END_CPU 0xBC3BU
#define DUNK_PALETTE_GROUP1_CPU 0xB148U
#define DUNK_REFERENCE_UNIFORM 0x30U

_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCES_OFFSET +
            TECMO_GAMEPLAY_DUNK_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCE_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET,
    "TGDK-1 source table must end at the raw-source region");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_SIZE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_PADDING_OFFSET,
    "TGDK-1 raw-source region must end at the reserved padding");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_PADDING_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_PADDING_SIZE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET,
    "TGDK-1 reserved padding must end at the cell table");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET +
            TECMO_GAMEPLAY_DUNK_CELL_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET,
    "TGDK-1 cell table must end at the palette");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_SIZE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET,
    "TGDK-1 palette must end at the stage table");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET +
            TECMO_GAMEPLAY_DUNK_STAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE,
    "TGDK-1 stage table must end at the payload boundary");

const TecmoGameplayDunkExpectedSource
    tecmo_gameplay_dunk_expected_sources[TECMO_GAMEPLAY_DUNK_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_DUNK_SOURCE_TRIGGER, 5U, 0U, 0x856BU, 61U, 832U,
         0x1E295DA9U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_CLEAR_LANE_HELPER, 5U, 0U, 0x85F3U,
         78U, 893U, 0xEE19230EU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_FIXED_DISPATCH, 7U, 1U, 0xE770U,
         30U, 971U, 0xDFBAA89CU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SCREEN_DESCRIPTOR, 7U, 1U, 0xDCD2U,
         7U, 1001U, 0xB0CE0D3DU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SCREEN_STREAM, 0U, 0U, 0x9022U,
         805U, 1008U, 0xC2FF0BCAU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_BASE_PALETTE, 0U, 0U, 0x9346U,
         16U, 1813U, 0xDEFFF0C1U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_CONTROLLER, 1U, 0U, 0xB002U,
         137U, 1829U, 0x73DEE21DU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_STAGE_SETUP, 1U, 0U, 0xB08BU,
         60U, 1966U, 0xBD5410FCU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_STAGE_TABLES, 1U, 0U, 0xB0C7U,
         38U, 2026U, 0x0C95FF12U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_DISPATCH, 7U, 1U, 0xC711U,
         43U, 2064U, 0xAF434105U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_BANK_TABLE, 7U, 1U, 0xCAF5U,
         62U, 2107U, 0x798F7231U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_ADDRESS_LOW, 7U, 1U, 0xCB33U,
         62U, 2169U, 0xCB4B3C42U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_ADDRESS_HIGH, 7U, 1U, 0xCB71U,
         62U, 2231U, 0xCD228EDDU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_EMITTER, 6U, 0U, 0xB37CU,
         72U, 2293U, 0x0C7AA2F7U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_POINTERS, 6U, 0U, 0xB3C4U,
         38U, 2365U, 0xC5920587U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_GEOMETRY, 6U, 0U, 0xB3EAU,
         2130U, 2403U, 0xD9F05A73U},
        {TECMO_GAMEPLAY_DUNK_SOURCE_PALETTE_RECIPE, 1U, 0U, 0xB0EDU,
         107U, 4533U, 0xB621395DU},
        {TECMO_GAMEPLAY_DUNK_SOURCE_COURT_RESTORE, 7U, 1U, 0xEB8DU,
         121U, 4640U, 0x32E920E6U}
    };

static const uint16_t dunk_record_cpu[TECMO_GAMEPLAY_DUNK_SIDE_COUNT]
                                     [TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    {0xB3EAU, 0xB413U, 0xB488U, 0xB525U, 0xB5C2U, 0xB68FU, 0xB754U},
    {0xB815U, 0xB83EU, 0xB8B3U, 0xB950U, 0xB9EDU, 0xBABAU, 0xBB7FU}
};

static const uint8_t dunk_record_count[TECMO_GAMEPLAY_DUNK_SIDE_COUNT]
                                      [TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    {10U, 29U, 39U, 39U, 51U, 49U, 48U},
    {10U, 29U, 39U, 39U, 51U, 49U, 47U}
};

static const uint32_t dunk_record_fingerprint[TECMO_GAMEPLAY_DUNK_SIDE_COUNT]
                                             [TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    {0x2B5B39EBU, 0x96293466U, 0x35698748U, 0xA3E2FB6DU,
     0x81095953U, 0x8096EF88U, 0xF5EAAD6FU},
    {0x39C0D763U, 0x9F9915AEU, 0x0E3CAE1CU, 0x94F58676U,
     0x4A6FE05CU, 0xC64BD22FU, 0xAE633760U}
};

static const uint8_t dunk_anchor_y[TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    0x6FU, 0x4FU, 0x3FU, 0x2FU, 0x1FU, 0x10U, 0x1FU
};
static const uint8_t dunk_anchor_x[TECMO_GAMEPLAY_DUNK_SIDE_COUNT]
                                  [TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    {0x90U, 0x88U, 0x80U, 0x78U, 0x70U, 0x68U, 0x50U},
    {0x20U, 0x30U, 0x40U, 0x50U, 0x58U, 0x60U, 0x50U}
};
static const uint8_t dunk_sprite_chr_page[TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    0xD4U, 0xD4U, 0xD4U, 0xD4U, 0xD8U, 0xD8U, 0xDCU
};
static const uint8_t dunk_visible_frame[TECMO_GAMEPLAY_DUNK_STAGE_COUNT] = {
    28U, 32U, 37U, 42U, 47U, 52U, 57U
};

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
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

static void store_u64(uint8_t *bytes, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t source_offset(uint64_t prg_offset,
                              const TecmoGameplayDunkExpectedSource *source)
{
    uint16_t base = source->fixed_bank != 0U ? 0xC000U : 0x8000U;
    return prg_offset + (uint64_t)source->bank * 0x4000ULL +
           (uint64_t)(source->cpu_start - base);
}

static int validate_geometry(const uint8_t *pointers,
                             const uint8_t *geometry,
                             uint64_t chr_size,
                             char *message,
                             size_t message_size)
{
    size_t side;
    if (pointers[0] != 0U || pointers[1] != 0x12U ||
        tecmo_asset_pack_fnv1a32(pointers + 2U, 36U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_ONLY_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGDK-1 side pointer offsets rejected.");
        return -1;
    }
    for (side = 0U; side < TECMO_GAMEPLAY_DUNK_SIDE_COUNT; ++side) {
        size_t slot;
        size_t pointer_base = 2U + side * 18U;
        for (slot = 0U; slot < 9U; ++slot) {
            uint16_t pointer = tecmo_asset_pack_read_u16(
                pointers + pointer_base + slot * 2U);
            if (slot >= TECMO_GAMEPLAY_DUNK_STAGE_COUNT) {
                if (pointer != 0U) {
                    tecmo_asset_pack_set_message(
                        message, message_size,
                        "TGDK-1 required null pointer slot is nonzero.");
                    return -1;
                }
                continue;
            }
            if (pointer != dunk_record_cpu[side][slot] ||
                pointer < DUNK_GEOMETRY_CPU || pointer > DUNK_GEOMETRY_END_CPU) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TGDK-1 sprite pointer escaped its geometry span.");
                return -1;
            }
            {
                size_t record_offset = (size_t)(pointer - DUNK_GEOMETRY_CPU);
                uint8_t count = geometry[record_offset];
                size_t record_size = 1U + (size_t)count * 4U;
                uint16_t expected_end = slot + 1U <
                        TECMO_GAMEPLAY_DUNK_STAGE_COUNT
                    ? dunk_record_cpu[side][slot + 1U]
                    : (side == 0U ? 0xB815U : 0xBC3CU);
                size_t piece;
                if (count == 0U || count > TECMO_GAMEPLAY_DUNK_MAX_PIECES ||
                    count != dunk_record_count[side][slot] ||
                    record_size != (size_t)(expected_end - pointer) ||
                    record_offset > 2130U ||
                    record_size > 2130U - record_offset ||
                    tecmo_asset_pack_fnv1a32(geometry + record_offset,
                                             record_size) !=
                        dunk_record_fingerprint[side][slot]) {
                    tecmo_asset_pack_set_message(
                        message, message_size,
                        "TGDK-1 sprite record count/range rejected.");
                    return -1;
                }
                for (piece = 0U; piece < count; ++piece) {
                    const uint8_t *record = geometry + record_offset + 1U +
                                            piece * 4U;
                    uint16_t tile = (uint16_t)record[1U] + 1U;
                    size_t stage_page = dunk_sprite_chr_page[slot];
                    size_t page = stage_page + ((tile & 0xFEU) >> 6U);
                    size_t offset = page * 1024U +
                                    (size_t)(tile & 0x3EU) * 16U;
                    if ((tile & 1U) == 0U || page < stage_page ||
                        page > stage_page + 3U || offset + 32U > chr_size) {
                        tecmo_asset_pack_set_message(
                            message, message_size,
                            "TGDK-1 sprite CHR resolution rejected.");
                        return -1;
                    }
                }
            }
        }
    }
    return 0;
}

static void build_reference_palette(const uint8_t *base_palette,
                                    const uint8_t *palette_recipe,
                                    uint8_t palette[32])
{
    const uint8_t *group = palette_recipe +
        (DUNK_PALETTE_GROUP1_CPU - 0xB0EDU);
    memcpy(palette, base_palette, 16U);
    memcpy(palette + 16U, group, 16U);
    palette[4U] = group[0U];
    palette[8U] = group[0U];
    palette[12U] = group[0U];
    palette[16U] = palette[0U];
    palette[22U] = DUNK_REFERENCE_UNIFORM;
    palette[23U] = DUNK_REFERENCE_UNIFORM;
    palette[25U] = DUNK_REFERENCE_UNIFORM;
    palette[29U] = (uint8_t)((DUNK_REFERENCE_UNIFORM + 0x10U) & 0x3FU);
}

int tecmo_asset_pack_build_gameplay_dunk_cutaway(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayDunkProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t decoded[DUNK_DECODED_SIZE];
    uint8_t reference_palette[32];
    size_t encoded_size = 0U;
    size_t index;
    const uint8_t *descriptor;
    const uint8_t *base_palette;
    const uint8_t *palette_recipe;
    const uint8_t *pointers;
    const uint8_t *geometry;

    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE ||
        prg_banks != DUNK_PRG_BANK_COUNT ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE ||
        !range_ok(chr_offset, chr_size, rom_size) ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGDK-1 import requires the exact Rev1 ROM and CHR contract.");
        return -1;
    }
    if (tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset,
                                 (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32 ||
        fnv1a64(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A64 ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset + 0xD0U * 1024U, 16U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIDE0_CHR_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset + 0x04U * 1024U, 4U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIDE1_BG_CHR_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset + 0xD4U * 1024U, 4U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE0_CHR_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset + 0xD8U * 1024U, 4U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE1_CHR_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            rom + (size_t)chr_offset + 0xDCU * 1024U, 4U * 1024U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE2_CHR_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGDK-1 CHR revision fingerprint rejected.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_SOURCE_COUNT; ++index) {
        const TecmoGameplayDunkExpectedSource *expected =
            &tecmo_gameplay_dunk_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, expected);
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (expected->bank >= prg_banks ||
            expected->cpu_start < (expected->fixed_bank ? 0xC000U : 0x8000U) ||
            cpu_end >= (expected->fixed_bank ? 0x10000U : 0xC000U) ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            expected->payload_offset < TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET ||
            expected->payload_offset > payload_size ||
            expected->byte_count > payload_size - expected->payload_offset ||
            tecmo_asset_pack_fnv1a32(rom + (size_t)offset,
                                     expected->byte_count) !=
                expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGDK-1 source $%04X-$%04X fingerprint/range rejected.",
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, expected->fingerprint);
        tecmo_asset_pack_store_u16(record + 20U, (uint16_t)index);
        memcpy(payload + expected->payload_offset, rom + (size_t)offset,
               expected->byte_count);
        provenance->source_offsets[index] = offset;
    }
    provenance->chr_offset = chr_offset;

    descriptor = payload + tecmo_gameplay_dunk_expected_sources[3].payload_offset;
    base_palette = payload + tecmo_gameplay_dunk_expected_sources[5].payload_offset;
    palette_recipe = payload + tecmo_gameplay_dunk_expected_sources[16].payload_offset;
    pointers = payload + tecmo_gameplay_dunk_expected_sources[14].payload_offset;
    geometry = payload + tecmo_gameplay_dunk_expected_sources[15].payload_offset;
    if ((uint8_t)(descriptor[0U] * 2U) != 0xD0U ||
        (uint8_t)(descriptor[1U] * 2U) != 0xD2U ||
        tecmo_asset_pack_read_u16(descriptor + 2U) !=
            DUNK_SCREEN_PALETTE_CPU ||
        tecmo_asset_pack_read_u16(descriptor + 4U) != DUNK_SCREEN_STREAM_CPU ||
        descriptor[6U] != 0U ||
        payload[tecmo_gameplay_dunk_expected_sources[4].payload_offset +
                DUNK_SCREEN_STREAM_SIZE - 1U] != base_palette[0U] ||
        tecmo_asset_pack_fnv1a32(base_palette, 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_BASE_PALETTE_FNV1A32 ||
        tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)prg_offset, 0x4000U,
            DUNK_SCREEN_STREAM_CPU - 0x8000U, decoded, sizeof(decoded),
            &encoded_size, message, message_size) != 0 ||
        encoded_size != DUNK_SCREEN_STREAM_SIZE ||
        tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_DECODED_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(decoded, DUNK_PAGE_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE0_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(decoded + DUNK_PAGE_SIZE, DUNK_PAGE_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE1_FNV1A32 ||
        validate_geometry(pointers, geometry, chr_size,
                          message, message_size) != 0 ||
        tecmo_asset_pack_fnv1a32(pointers, 38U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTERS_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(geometry, 2130U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_GEOMETRY_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(pointers, 38U + 2130U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_GEOMETRY_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGDK-1 decoded screen/palette/geometry contract rejected.");
        return -1;
    }

    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_CELL_COUNT; ++index) {
        size_t side = index / TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT;
        size_t local = index % TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT;
        unsigned row = (unsigned)(local / 32U);
        unsigned column = (unsigned)(local % 32U);
        const uint8_t *page = decoded + side * DUNK_PAGE_SIZE;
        uint8_t tile = page[local];
        uint8_t page_base = side == 0U ? 0xD0U : 0x04U;
        uint32_t chr_cell_offset =
            ((uint32_t)page_base + (uint32_t)(tile >> 6U)) * 1024U +
            (uint32_t)(tile & 0x3FU) * 16U;
        uint8_t *cell = payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET +
                        index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE;
        if ((uint64_t)chr_cell_offset + 16U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TGDK-1 background CHR escaped chr/all.");
            return -1;
        }
        tecmo_asset_pack_store_u32(cell, chr_cell_offset);
        cell[4U] = tecmo_asset_pack_decoded_palette_index(page, row, column);
        cell[5U] = (uint8_t)side;
    }

    build_reference_palette(base_palette, palette_recipe, reference_palette);
    if (tecmo_asset_pack_fnv1a32(reference_palette, 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_BG_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(reference_palette + 16U, 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_SPRITE_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(reference_palette, 32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_PALETTE_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGDK-1 reference palette recipe rejected.");
        return -1;
    }
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET,
           reference_palette, sizeof(reference_palette));

    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_STAGE_COUNT; ++index) {
        uint8_t *stage = payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET +
                         index * TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE;
        tecmo_asset_pack_store_u16(
            stage, (uint16_t)(TECMO_GAMEPLAY_DUNK_FIRST_ASSIGN_FRAME +
                              index * TECMO_GAMEPLAY_DUNK_STAGE_CADENCE));
        tecmo_asset_pack_store_u16(
            stage + 2U, dunk_visible_frame[index]);
        stage[4U] = dunk_anchor_y[index];
        stage[5U] = dunk_anchor_x[0U][index];
        stage[6U] = dunk_anchor_x[1U][index];
        stage[7U] = dunk_sprite_chr_page[index];
        stage[8U] = (uint8_t)index;
    }

    memcpy(payload, "TGDK", 4U);
    tecmo_asset_pack_store_u16(payload + 4U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_VERSION);
    tecmo_asset_pack_store_u16(payload + 6U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 8U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE);
    tecmo_asset_pack_store_u16(payload + 12U,
                               TECMO_GAMEPLAY_DUNK_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(payload + 16U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 20U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_SIZE);
    tecmo_asset_pack_store_u32(payload + 28U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_GAMEPLAY_DUNK_CELL_COUNT);
    tecmo_asset_pack_store_u16(payload + 36U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 38U,
                               TECMO_GAMEPLAY_DUNK_SIDE_COUNT);
    tecmo_asset_pack_store_u32(payload + 40U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PALETTE_SIZE);
    tecmo_asset_pack_store_u32(payload + 48U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 52U,
                               TECMO_GAMEPLAY_DUNK_STAGE_COUNT);
    tecmo_asset_pack_store_u16(payload + 54U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE);
    tecmo_asset_pack_store_u32(payload + 56U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 60U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32);
    store_u64(payload + 64U, TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A64);
    tecmo_asset_pack_store_u32(payload + 72U, DUNK_DECODED_SIZE);
    tecmo_asset_pack_store_u32(payload + 76U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_DECODED_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 80U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE0_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 84U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_PAGE1_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 88U,
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_RAW_SIZE));
    tecmo_asset_pack_store_u32(payload + 92U,
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELLS_OFFSET,
            TECMO_GAMEPLAY_DUNK_CELL_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_DUNK_CELL_STRIDE));
    tecmo_asset_pack_store_u32(payload + 96U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_BASE_PALETTE_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 100U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_BG_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 104U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_SPRITE_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 108U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_REFERENCE_PALETTE_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 112U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTERS_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 116U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_ONLY_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 120U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_GEOMETRY_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 124U,
                               TECMO_ASSET_PACK_GAMEPLAY_DUNK_POINTER_GEOMETRY_FNV1A32);
    tecmo_asset_pack_store_u16(payload + 128U,
                               TECMO_GAMEPLAY_DUNK_DISPATCH_FRAME);
    tecmo_asset_pack_store_u16(payload + 130U,
                               TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME);
    tecmo_asset_pack_store_u16(payload + 132U,
                               TECMO_GAMEPLAY_DUNK_FIRST_ASSIGN_FRAME);
    tecmo_asset_pack_store_u16(payload + 134U,
                               TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME);
    tecmo_asset_pack_store_u16(payload + 136U,
                               TECMO_GAMEPLAY_DUNK_STAGE_CADENCE);
    tecmo_asset_pack_store_u16(payload + 138U,
                               TECMO_GAMEPLAY_DUNK_LAST_ASSIGN_FRAME);
    tecmo_asset_pack_store_u16(payload + 140U,
                               TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME);
    tecmo_asset_pack_store_u16(payload + 142U,
                               TECMO_GAMEPLAY_DUNK_PALETTE_BLACK_FRAME);
    tecmo_asset_pack_store_u16(payload + 144U,
                               TECMO_GAMEPLAY_DUNK_CLEAR_FRAME);
    tecmo_asset_pack_store_u16(payload + 146U,
                               TECMO_GAMEPLAY_DUNK_COURT_REBUILD_FRAME);
    tecmo_asset_pack_store_u16(payload + 148U,
                               TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME);
    tecmo_asset_pack_store_u16(payload + 150U,
                               TECMO_GAMEPLAY_DUNK_ROUTE_RESUME_FRAME);
    tecmo_asset_pack_store_u16(payload + 152U,
                               TECMO_GAMEPLAY_DUNK_A9C5_FRAME);
    tecmo_asset_pack_store_u16(payload + 154U,
                               TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME);
    payload[156U] = TECMO_GAMEPLAY_DUNK_MAX_PIECES;
    payload[157U] = DUNK_REFERENCE_UNIFORM;
    payload[158U] = 1U;
    payload[159U] = 0xD0U;
    payload[160U] = 0x04U;
    payload[161U] = 0xD4U;
    tecmo_asset_pack_store_u32(payload + 164U,
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGES_OFFSET,
            TECMO_GAMEPLAY_DUNK_STAGE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_DUNK_STAGE_STRIDE));

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_DUNK_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGDK-1 canonical payload mismatch.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGDK-1 dunk cutaway imported.");
    return 0;
}

int tecmo_asset_pack_gameplay_dunk_cutaway_self_test(char *message,
                                                      size_t message_size)
{
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGDK-1 payload layout self-test passed.");
    return 0;
}
