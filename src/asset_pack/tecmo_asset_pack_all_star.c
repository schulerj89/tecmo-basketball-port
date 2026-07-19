#include "tecmo_asset_pack_all_star.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define ALL_STAR_BANK 3U
#define ALL_STAR_BG_R0 0xFAU
#define ALL_STAR_BG_R1 0xFAU

typedef struct AllStarSpan {
    uint32_t bank;
    uint32_t cpu;
    size_t size;
    uint32_t fingerprint;
    bool fixed;
} AllStarSpan;

static bool range_ok(uint64_t offset, uint64_t size, uint64_t total)
{
    return offset <= total && size <= total - offset;
}

static uint64_t bank_cpu_offset(uint64_t prg_offset, uint32_t bank, uint32_t cpu)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0x8000U);
}

static uint64_t fixed_cpu_offset(uint64_t prg_offset, uint32_t prg_banks,
                                 uint32_t cpu)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) *
           TECMO_ASSET_PACK_PRG_BANK_BYTES + (uint64_t)(cpu - 0xC000U);
}

static int verify_span(const uint8_t *rom,
                       uint64_t rom_size,
                       uint64_t prg_offset,
                       uint32_t prg_banks,
                       const AllStarSpan *span,
                       int enforce,
                       uint64_t *offset_out)
{
    uint64_t offset = span->fixed
        ? fixed_cpu_offset(prg_offset, prg_banks, span->cpu)
        : bank_cpu_offset(prg_offset, span->bank, span->cpu);
    if (!range_ok(offset, span->size, rom_size)) return -1;
    if (enforce != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)offset, span->size) !=
            span->fingerprint)
        return -1;
    if (offset_out != NULL) *offset_out = offset;
    return 0;
}

static int build_record_tiles(const uint8_t *record,
                              size_t record_size,
                              const uint8_t *char_map,
                              uint8_t *tiles,
                              size_t tile_capacity,
                              unsigned *width_out,
                              unsigned *height_out,
                              char *message,
                              size_t message_size)
{
    unsigned width;
    unsigned height;
    unsigned x;
    unsigned y = 0U;
    size_t source = 4U;

    if (record == NULL || char_map == NULL || tiles == NULL ||
        width_out == NULL || height_out == NULL || record_size < 5U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 menu record is missing.");
        return -1;
    }
    width = record[0];
    height = record[1];
    if (width < 3U || height < 3U || (size_t)width * height > tile_capacity) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 menu rectangle is outside its native bounds.");
        return -1;
    }

    memset(tiles, 0xFF, (size_t)width * height);
    tiles[0] = 0xAEU;
    for (x = 1U; x + 1U < width; ++x) tiles[x] = 0xAAU;
    tiles[width - 1U] = 0xA7U;
    for (y = 1U; y + 1U < height; ++y) {
        tiles[(size_t)y * width] = 0xADU;
        tiles[(size_t)y * width + width - 1U] = 0xABU;
    }
    tiles[(size_t)(height - 1U) * width] = 0xA8U;
    for (x = 1U; x + 1U < width; ++x)
        tiles[(size_t)(height - 1U) * width + x] = 0xACU;
    tiles[(size_t)height * width - 1U] = 0xA9U;

    x = 1U;
    y = 0U;
    while (source < record_size && record[source] != 0xFFU) {
        uint8_t character = record[source++];
        if (character == 0x26U) {
            x = 1U;
            ++y;
            if (y >= height) {
                tecmo_asset_pack_set_message(message, message_size,
                                             "TALL-1 menu record has too many rows.");
                return -1;
            }
            continue;
        }
        if (character > 0x5AU || y >= height || x + 1U >= width) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TALL-1 menu character or row width was rejected.");
            return -1;
        }
        tiles[(size_t)y * width + x++] = char_map[character];
    }
    if (source >= record_size || record[source] != 0xFFU) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 menu record lacks its terminator.");
        return -1;
    }
    *width_out = width;
    *height_out = height;
    return 0;
}

static void store_cell(uint8_t *cell, uint8_t tile, uint8_t palette_index)
{
    cell[0] = tile;
    cell[1] = palette_index;
    tecmo_asset_pack_store_u32(
        cell + 2U,
        tecmo_asset_pack_bg_chr_offset(tile, ALL_STAR_BG_R0, ALL_STAR_BG_R1));
}

static int build_team_overlay(uint8_t *payload,
                              const uint8_t *record,
                              const uint8_t *char_map,
                              const uint8_t base_screen[2048],
                              char *message,
                              size_t message_size)
{
    uint8_t tiles[64];
    uint8_t *desc = payload + TECMO_ASSET_PACK_ALL_STAR_OVERLAY_DESCS_OFFSET;
    unsigned width;
    unsigned height;
    unsigned origin_x = record[2];
    unsigned origin_y = record[3];
    size_t cell_count;

    if (build_record_tiles(record, TECMO_ASSET_PACK_ALL_STAR_TEAM_RECORD_SIZE,
                           char_map, tiles, sizeof(tiles), &width, &height,
                           message, message_size) != 0)
        return -1;
    cell_count = (size_t)width * height;
    if (width != 7U || height != 6U || origin_x != 7U || origin_y != 17U ||
        origin_x + width > 64U || origin_y + height > 30U ||
        cell_count != TECMO_ASSET_PACK_ALL_STAR_OVERLAY_CELL_COUNT) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 TEAM overlay placement was rejected.");
        return -1;
    }
    tecmo_asset_pack_store_u32(desc, 0U);
    tecmo_asset_pack_store_u16(desc + 4U, (uint16_t)cell_count);
    tecmo_asset_pack_store_u16(desc + 6U, (uint16_t)width);
    tecmo_asset_pack_store_u16(desc + 8U, (uint16_t)height);
    tecmo_asset_pack_store_u16(desc + 10U, (uint16_t)origin_x);
    tecmo_asset_pack_store_u16(desc + 12U, (uint16_t)origin_y);
    desc[14U] = 0U;

    for (unsigned y = 0U; y < height; ++y) {
        for (unsigned x = 0U; x < width; ++x) {
            unsigned gx = origin_x + x;
            const uint8_t *page = base_screen + (gx / 32U) * 1024U;
            uint8_t palette = tecmo_asset_pack_decoded_palette_index(
                page, origin_y + y, gx % 32U);
            uint8_t *cell = payload + TECMO_ASSET_PACK_ALL_STAR_OVERLAY_CELLS_OFFSET +
                            ((size_t)y * width + x) *
                                TECMO_ASSET_PACK_ALL_STAR_CELL_STRIDE;
            store_cell(cell, tiles[(size_t)y * width + x], palette);
        }
    }
    return 0;
}

static int derive_cursor(const uint8_t *rom,
                         uint64_t rom_size,
                         uint64_t prg_offset,
                         uint8_t config,
                         uint8_t selection_count,
                         int cursor_dx,
                         int cursor_dy,
                         uint16_t *x_out,
                         uint16_t *y_out,
                         uint16_t *stride_out)
{
    uint64_t params_offset = bank_cpu_offset(
        prg_offset, ALL_STAR_BANK, TECMO_ASSET_PACK_ALL_STAR_INPUT_PARAMS_CPU);
    uint64_t pointers_offset = bank_cpu_offset(
        prg_offset, ALL_STAR_BANK,
        TECMO_ASSET_PACK_ALL_STAR_INPUT_COORD_POINTERS_CPU);
    uint16_t delta_cpu;
    uint64_t delta_offset;
    int x;
    int y;
    uint8_t first;
    uint8_t second;

    if (!range_ok(params_offset, TECMO_ASSET_PACK_ALL_STAR_INPUT_PARAMS_SIZE,
                  rom_size) ||
        !range_ok(pointers_offset,
                  TECMO_ASSET_PACK_ALL_STAR_INPUT_COORD_POINTERS_SIZE,
                  rom_size) ||
        config >= TECMO_ASSET_PACK_ALL_STAR_INPUT_COORD_POINTERS_SIZE / 2U)
        return -1;
    delta_cpu = tecmo_asset_pack_read_u16(
        rom + (size_t)pointers_offset + (size_t)config * 2U);
    if (delta_cpu < TECMO_ASSET_PACK_ALL_STAR_COORD_TABLES_CPU ||
        (uint32_t)delta_cpu + selection_count >
            TECMO_ASSET_PACK_ALL_STAR_COORD_TABLES_CPU +
                TECMO_ASSET_PACK_ALL_STAR_COORD_TABLES_SIZE)
        return -1;
    delta_offset = bank_cpu_offset(prg_offset, ALL_STAR_BANK, delta_cpu);
    if (!range_ok(delta_offset, selection_count, rom_size) || selection_count < 2U)
        return -1;
    first = rom[(size_t)delta_offset];
    second = rom[(size_t)delta_offset + 1U];
    x = (int)rom[(size_t)params_offset +
                 (0x9F30U - TECMO_ASSET_PACK_ALL_STAR_INPUT_PARAMS_CPU) +
                 config] +
        cursor_dx;
    y = (int)rom[(size_t)params_offset + config] + cursor_dy;
    if (x < 0 || x > 255 || y < 0 || y > 239 || second <= first)
        return -1;
    *x_out = (uint16_t)x;
    *y_out = (uint16_t)y;
    *stride_out = (uint16_t)(second - first);
    return 0;
}

int tecmo_asset_pack_all_star_self_test(char *message, size_t message_size)
{
    static const uint8_t invalid_record[5] = {2U, 2U, 0U, 0U, 0xFFU};
    uint8_t tiles[16];
    uint8_t char_map[91];
    unsigned width = 0U;
    unsigned height = 0U;
    memset(char_map, 0, sizeof(char_map));
    if (build_record_tiles(invalid_record, sizeof(invalid_record), char_map,
                           tiles, sizeof(tiles), &width, &height,
                           NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 record bounds self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TALL-1 record bounds self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_all_star_menu(const uint8_t *rom,
                                         uint64_t rom_size,
                                         uint64_t prg_offset,
                                         uint32_t prg_banks,
                                         uint64_t chr_size,
                                         int enforce_revision_fingerprints,
                                         uint8_t *payload,
                                         size_t payload_size,
                                         TecmoAllStarMenuProvenance *provenance,
                                         char *message,
                                         size_t message_size)
{
    static const AllStarSpan spans[] = {
        {3U, 0x8221U, 182U, 0x85DFCDE5U, false},
        {3U, 0x82D7U, 21U, 0x369626B2U, false},
        {3U, 0x99DAU, 12U, 0x82A498DFU, false},
        {3U, 0x99E6U, 62U, 0x8759504BU, false},
        {3U, 0x9A24U, 164U, 0x4325EDF8U, false},
        {3U, 0x9AA8U, 91U, 0x724F80CEU, false},
        {3U, 0x9B3EU, 23U, 0x28D1A422U, false},
        {3U, 0x9C5EU, 91U, 0xC4F89AF6U, false},
        {3U, 0x9D22U, 40U, 0x8861F7F3U, false},
        {3U, 0x9EC6U, 77U, 0x6C2709EBU, false},
        {3U, 0x9F13U, 145U, 0xDE568C7BU, false},
        {3U, 0x9FA4U, 116U, 0xAAF64989U, false},
        {3U, 0xA018U, 131U, 0x7529BA7FU, false},
        {3U, 0xB27FU, 4U, 0x8145527EU, false},
        {1U, 0x8031U, 5U, 0x7D5835D4U, false},
        {0U, 0xD317U, 9U, 0x8868D9B5U, true},
        {0U, 0xD723U, 273U, 0xAE47C4A0U, true}
    };
    static const uint8_t configs[3] = {0x14U, 0x13U, 0x1CU};
    static const uint8_t selection_counts[3] = {7U, 3U, 2U};
    uint64_t offsets[sizeof(spans) / sizeof(spans[0])];
    uint64_t chr_offset;
    uint64_t descriptor_offset;
    uint64_t stream_offset;
    uint8_t base_screen[2048];
    size_t encoded_size = 0U;
    const uint8_t *cursor;
    const uint8_t *flow;
    const uint8_t *difficulty_flow;
    const uint8_t *char_map;
    const uint8_t *team_record;
    int cursor_dx;
    int cursor_dy;
    uint16_t cursor_x[3];
    uint16_t cursor_y[3];
    uint16_t cursor_stride[3];

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks < 8U ||
        chr_size != TECMO_ASSET_PACK_ALL_STAR_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_ALL_STAR_SIZE) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    memset(provenance, 0, sizeof(*provenance));
    if ((uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES >
        UINT64_MAX - prg_offset) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 CHR source offset overflowed.");
        return -1;
    }
    chr_offset = prg_offset + (uint64_t)prg_banks *
                 TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (!range_ok(chr_offset, chr_size, rom_size)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 CHR source is outside the ROM.");
        return -1;
    }
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &spans[i],
                        enforce_revision_fingerprints, &offsets[i]) != 0) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TALL-1 source fingerprint mismatch.");
            return -1;
        }
    }
    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_ALL_STAR_CHR_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 full CHR fingerprint mismatch.");
        return -1;
    }

    descriptor_offset = fixed_cpu_offset(
        prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_DESCRIPTOR_CPU);
    stream_offset = bank_cpu_offset(
        prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_STREAM_CPU);
    if (!range_ok(descriptor_offset, 7U, rom_size) ||
        !range_ok(stream_offset, TECMO_ASSET_PACK_START_MENU_STREAM_SIZE,
                  rom_size) ||
        tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)prg_offset, TECMO_ASSET_PACK_PRG_BANK_BYTES,
            TECMO_ASSET_PACK_START_MENU_STREAM_CPU - 0x8000U,
            base_screen, sizeof(base_screen), &encoded_size,
            message, message_size) != 0 ||
        encoded_size != TECMO_ASSET_PACK_START_MENU_STREAM_SIZE ||
        (enforce_revision_fingerprints != 0 &&
         (tecmo_asset_pack_fnv1a32(rom + (size_t)descriptor_offset, 7U) !=
              0x0A5B3B88U ||
          tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset,
                                   encoded_size) != 0x8047E031U ||
          tecmo_asset_pack_fnv1a32(base_screen, sizeof(base_screen)) !=
              0xE1840CFEU))) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 base start-menu screen was rejected.");
        return -1;
    }

    flow = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x8221U);
    difficulty_flow = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x99E6U);
    cursor = rom + (size_t)bank_cpu_offset(prg_offset, 1U, 0x8031U);
    if (flow[0x20U] != 0xA2U || flow[0x21U] != configs[0] ||
        flow[0x70U] != 0xA2U || flow[0x71U] != configs[2] ||
        difficulty_flow[0x0EU] != 0xA2U ||
        difficulty_flow[0x0FU] != configs[1] ||
        cursor[0U] != 0x11U || cursor[3U] != 0x30U || cursor[4U] != 0x24U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 route/config operands were rejected.");
        return -1;
    }
    cursor_dx = (int)(int8_t)cursor[1U];
    cursor_dy = (int)(int8_t)cursor[2U];
    if (cursor_dx != 0 || cursor_dy != -4) return -1;
    for (size_t i = 0U; i < 3U; ++i) {
        if (derive_cursor(rom, rom_size, prg_offset, configs[i],
                          selection_counts[i], cursor_dx, cursor_dy,
                          &cursor_x[i], &cursor_y[i],
                          &cursor_stride[i]) != 0)
            return -1;
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TALL", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U,
                               TECMO_ASSET_PACK_ALL_STAR_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U,
                               TECMO_ASSET_PACK_ALL_STAR_OVERLAY_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U,
                               TECMO_ASSET_PACK_ALL_STAR_OVERLAY_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 16U,
                               TECMO_ASSET_PACK_ALL_STAR_OVERLAY_DESCS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 20U,
                               TECMO_ASSET_PACK_ALL_STAR_OVERLAY_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U,
                               TECMO_ASSET_PACK_ALL_STAR_SIZE);
    tecmo_asset_pack_store_u32(payload + 28U,
                               TECMO_ASSET_PACK_ALL_STAR_TPRE_SIZE);
    tecmo_asset_pack_store_u32(payload + 32U,
                               TECMO_ASSET_PACK_ALL_STAR_TPRE_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 36U,
                               TECMO_ASSET_PACK_ALL_STAR_TSGM_SIZE);
    tecmo_asset_pack_store_u32(payload + 40U,
                               TECMO_ASSET_PACK_ALL_STAR_TSGM_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 44U,
                               TECMO_ASSET_PACK_ALL_STAR_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 48U,
                               TECMO_ASSET_PACK_ALL_STAR_CHR_FNV1A32);
    payload[52U] = 1U;
    payload[53U] = 1U;
    payload[54U] = 5U;
    payload[55U] = 8U;
    payload[56U] = 7U;
    payload[57U] = 3U;
    payload[58U] = 2U;
    payload[59U] = rom[(size_t)bank_cpu_offset(prg_offset, 3U, 0x82D8U)];
    payload[60U] = rom[(size_t)bank_cpu_offset(prg_offset, 3U, 0x82DDU)];
    payload[61U] = 0xC0U;
    payload[62U] = 0x0CU;
    payload[63U] = 1U;
    payload[64U] = 1U;
    payload[65U] = 4U;
    memcpy(payload + 66U,
           rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x99DAU), 12U);
    memcpy(payload + 78U, configs, sizeof(configs));
    for (size_t i = 0U; i < 3U; ++i) {
        tecmo_asset_pack_store_u16(payload + 82U + i * 6U, cursor_x[i]);
        tecmo_asset_pack_store_u16(payload + 84U + i * 6U, cursor_y[i]);
        tecmo_asset_pack_store_u16(payload + 86U + i * 6U,
                                   cursor_stride[i]);
    }
    payload[100U] = 0U;
    payload[101U] = 1U;
    payload[102U] = 1U;
    payload[103U] = 0U;

    char_map = rom + (size_t)bank_cpu_offset(
        prg_offset, ALL_STAR_BANK, TECMO_ASSET_PACK_ALL_STAR_CHAR_MAP_CPU);
    team_record = rom + (size_t)bank_cpu_offset(
        prg_offset, ALL_STAR_BANK, TECMO_ASSET_PACK_ALL_STAR_TEAM_RECORD_CPU);
    if (build_team_overlay(payload, team_record, char_map, base_screen,
                           message, message_size) != 0)
        return -1;
    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
        TECMO_ASSET_PACK_ALL_STAR_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TALL-1 payload fingerprint mismatch.");
        return -1;
    }

    provenance->chr_offset = chr_offset;
    provenance->base_descriptor_offset = descriptor_offset;
    provenance->base_stream_offset = stream_offset;
    provenance->base_stream_size = encoded_size;
    provenance->flow_offset = offsets[0];
    provenance->final_commit_offset = offsets[1];
    provenance->ownership_offset = offsets[2];
    provenance->difficulty_flow_offset = offsets[3];
    provenance->popup_builder_offset = offsets[4];
    provenance->char_map_offset = offsets[5];
    provenance->team_record_offset = offsets[6];
    provenance->control_record_offset = offsets[7];
    provenance->difficulty_record_offset = offsets[8];
    provenance->input_wrapper_offset = offsets[9];
    provenance->input_params_offset = offsets[10];
    provenance->input_coord_pointers_offset = offsets[11];
    provenance->coord_tables_offset = offsets[12];
    provenance->launch_routine_offset = offsets[13];
    provenance->cursor_record_offset = offsets[14];
    provenance->controller_poll_offset = offsets[15];
    provenance->input_helper_offset = offsets[16];
    tecmo_asset_pack_set_message(message, message_size,
                                 "Imported strict TALL-1 ALL STAR menu asset.");
    return 0;
}
