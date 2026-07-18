#include "tecmo_asset_pack_preseason.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define PRESEASON_BANK 3U
#define PRESEASON_TEAM_SCREEN_FIRST_ID 6U
#define PRESEASON_BG_R0 0xFAU
#define PRESEASON_BG_R1 0xFAU
#define PRESEASON_MARKER_CHR_FNV1A32 0x1E505537U

typedef struct PreseasonSpan {
    uint32_t bank;
    uint32_t cpu;
    size_t size;
    uint32_t fingerprint;
} PreseasonSpan;

static bool range_ok(uint64_t offset, uint64_t size, uint64_t total)
{
    return offset <= total && size <= total - offset;
}

static uint64_t bank_cpu_offset(uint64_t prg_offset, uint32_t bank, uint32_t cpu)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0x8000U);
}

static uint64_t fixed_cpu_offset(uint64_t prg_offset, uint32_t prg_banks, uint32_t cpu)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0xC000U);
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

    if (record == NULL || char_map == NULL || tiles == NULL || record_size < 5U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 menu record is missing.");
        return -1;
    }
    width = record[0];
    height = record[1];
    if (width < 3U || height < 3U || (size_t)width * height > tile_capacity) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 menu rectangle is outside its native bounds.");
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
                                             "TPRE-1 menu record has too many rows.");
                return -1;
            }
            continue;
        }
        if (character > 0x5AU || y >= height || x + 1U >= width) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 menu character or row width was rejected.");
            return -1;
        }
        tiles[(size_t)y * width + x++] = char_map[character];
    }
    if (source >= record_size || record[source] != 0xFFU) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 menu record lacks its terminator.");
        return -1;
    }
    *width_out = width;
    *height_out = height;
    return 0;
}

static void store_cell(uint8_t *cell,
                       uint8_t tile,
                       uint8_t palette_index,
                       uint8_t r0,
                       uint8_t r1)
{
    cell[0] = tile;
    cell[1] = palette_index;
    tecmo_asset_pack_store_u32(cell + 2U,
                               tecmo_asset_pack_bg_chr_offset(tile, r0, r1));
}

static int build_overlay(uint8_t *payload,
                         size_t overlay_index,
                         size_t cell_start,
                         const uint8_t *record,
                         size_t record_size,
                         const uint8_t *char_map,
                         const uint8_t base_screen[2048],
                         char *message,
                         size_t message_size)
{
    uint8_t tiles[256];
    unsigned width;
    unsigned height;
    unsigned origin_x;
    unsigned origin_y;
    size_t cell_count;
    uint8_t *desc;

    if (overlay_index >= TECMO_ASSET_PACK_PRESEASON_OVERLAY_COUNT ||
        build_record_tiles(record, record_size, char_map, tiles, sizeof(tiles),
                           &width, &height, message, message_size) != 0)
        return -1;
    origin_x = record[2];
    origin_y = record[3];
    cell_count = (size_t)width * height;
    if (origin_x + width > 64U || origin_y + height > 30U ||
        cell_start + cell_count > TECMO_ASSET_PACK_PRESEASON_OVERLAY_CELL_COUNT) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 overlay placement was rejected.");
        return -1;
    }
    desc = payload + TECMO_ASSET_PACK_PRESEASON_OVERLAY_DESCS_OFFSET +
           overlay_index * TECMO_ASSET_PACK_PRESEASON_OVERLAY_DESC_STRIDE;
    tecmo_asset_pack_store_u32(desc, (uint32_t)cell_start);
    tecmo_asset_pack_store_u16(desc + 4U, (uint16_t)cell_count);
    tecmo_asset_pack_store_u16(desc + 6U, (uint16_t)width);
    tecmo_asset_pack_store_u16(desc + 8U, (uint16_t)height);
    tecmo_asset_pack_store_u16(desc + 10U, (uint16_t)origin_x);
    tecmo_asset_pack_store_u16(desc + 12U, (uint16_t)origin_y);
    desc[14U] = (uint8_t)overlay_index;

    for (unsigned y = 0U; y < height; ++y) {
        for (unsigned x = 0U; x < width; ++x) {
            unsigned gx = origin_x + x;
            const uint8_t *page = base_screen + (gx / 32U) * 1024U;
            uint8_t palette = tecmo_asset_pack_decoded_palette_index(
                page, origin_y + y, gx % 32U);
            uint8_t *cell = payload + TECMO_ASSET_PACK_PRESEASON_OVERLAY_CELLS_OFFSET +
                            (cell_start + (size_t)y * width + x) *
                                TECMO_ASSET_PACK_PRESEASON_CELL_STRIDE;
            store_cell(cell, tiles[(size_t)y * width + x], palette,
                       PRESEASON_BG_R0, PRESEASON_BG_R1);
        }
    }
    return 0;
}

static void store_marker(uint8_t *piece,
                         int16_t dx,
                         int16_t dy,
                         uint8_t selector,
                         uint8_t raw_tile)
{
    uint32_t top = (uint32_t)selector * 1024U +
                   (uint32_t)raw_tile * 16U;
    tecmo_asset_pack_store_u16(piece, (uint16_t)dx);
    tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)dy);
    tecmo_asset_pack_store_u32(piece + 4U, top);
    tecmo_asset_pack_store_u32(piece + 8U, top + 16U);
}

static int verify_span(const uint8_t *rom,
                       uint64_t rom_size,
                       uint64_t prg_offset,
                       uint32_t prg_banks,
                       const PreseasonSpan *span,
                       int fixed,
                       int enforce,
                       uint64_t *offset_out)
{
    uint64_t offset = fixed
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

int tecmo_asset_pack_preseason_self_test(char *message, size_t message_size)
{
    static const uint8_t invalid_record[5] = {2U, 2U, 0U, 0U, 0xFFU};
    static const uint8_t invalid_newlines[8] = {
        3U, 3U, 0U, 0U, 0x26U, 0x26U, 0x26U, 0xFFU
    };
    uint8_t tiles[16];
    uint8_t char_map[91];
    unsigned width = 0U;
    unsigned height = 0U;
    memset(char_map, 0, sizeof(char_map));
    if (build_record_tiles(invalid_record, sizeof(invalid_record), char_map,
                           tiles, sizeof(tiles), &width, &height,
                           NULL, 0U) == 0 ||
        build_record_tiles(invalid_newlines, sizeof(invalid_newlines), char_map,
                           tiles, sizeof(tiles), &width, &height,
                           NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 record bounds self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TPRE-1 record bounds self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_preseason_menu(const uint8_t *rom,
                                          uint64_t rom_size,
                                          uint64_t prg_offset,
                                          uint32_t prg_banks,
                                          uint64_t chr_size,
                                          int enforce_revision_fingerprints,
                                          uint8_t *payload,
                                          size_t payload_size,
                                          TecmoPreseasonMenuProvenance *provenance,
                                          char *message,
                                          size_t message_size)
{
    static const PreseasonSpan bank_spans[] = {
        {3U, 0x81B8U, 14U, 0xDCA1F834U},
        {3U, 0x9966U, 116U, 0x0520BA1DU},
        {3U, 0x99DAU, 12U, 0x82A498DFU},
        {3U, 0x99E6U, 62U, 0x8759504BU},
        {3U, 0x9A24U, 164U, 0x4325EDF8U},
        {3U, 0x9AA8U, 91U, 0x724F80CEU},
        {3U, 0x9C5EU, 91U, 0xC4F89AF6U},
        {3U, 0x9CB9U, 68U, 0xB92F396EU},
        {3U, 0x9D22U, 40U, 0x8861F7F3U},
        {3U, 0x9EC6U, 77U, 0x6C2709EBU},
        {3U, 0x9F13U, 145U, 0xDE568C7BU},
        {3U, 0x9FA4U, 116U, 0xAAF64989U},
        {3U, 0xA018U, 131U, 0x7529BA7FU},
        {3U, 0xB1CCU, 167U, 0x46A305ADU},
        {3U, 0xB273U, 47U, 0x1F0402B0U},
        {1U, 0x8031U, 5U, 0x7D5835D4U},
        {1U, 0x8036U, 20U, 0x5E246059U},
        {0U, 0x9386U, 64U, 0x79FF4531U}
    };
    static const PreseasonSpan fixed_spans[] = {
        {0U, 0xDCAFU, 28U, 0xC05D4638U},
        {0U, 0xD317U, 9U, 0x8868D9B5U},
        {0U, 0xD723U, 273U, 0xAE47C4A0U},
        {0U, 0xD92BU, 122U, 0x835283BEU},
        {0U, 0xDB25U, 99U, 0x5D98AB7AU},
        {0U, 0xDB88U, 72U, 0xD75D6EEAU},
        {0U, 0xE481U, 41U, 0x76C592FCU}
    };
    static const uint32_t stream_cpus[4] = {0x8973U, 0x8AFDU, 0x8CC2U, 0x8E4CU};
    static const size_t stream_sizes[4] = {395U, 454U, 395U, 471U};
    static const uint32_t stream_fingerprints[4] = {
        0xA06C1E1FU, 0xBB5D6EA8U, 0xFAD48944U, 0x5E41604DU
    };
    static const uint32_t decoded_fingerprints[4] = {
        0xAF69FFC7U, 0x727E1AD2U, 0x41877C8FU, 0x0B5CB115U
    };
    static const uint32_t descriptor_fingerprints[4] = {
        0xC62DA3E3U, 0x5ABDD3EAU, 0x53633C79U, 0x4824AE3DU
    };
    static const uint32_t palette_fingerprints[4] = {
        0xFAAF07AEU, 0x34F6B8DCU, 0x7574D664U, 0xCE09A13AU
    };
    static const uint8_t expected_descriptors[4][7] = {
        {0x70U, 0x71U, 0x86U, 0x93U, 0x73U, 0x89U, 0x00U},
        {0x72U, 0x73U, 0x96U, 0x93U, 0xFDU, 0x8AU, 0x00U},
        {0x74U, 0x75U, 0xA6U, 0x93U, 0xC2U, 0x8CU, 0x00U},
        {0x76U, 0x77U, 0xB6U, 0x93U, 0x4CU, 0x8EU, 0x00U}
    };
    uint8_t configs[7];
    uint64_t offsets[sizeof(bank_spans) / sizeof(bank_spans[0])];
    uint64_t fixed_offsets[sizeof(fixed_spans) / sizeof(fixed_spans[0])];
    uint64_t chr_offset;
    uint64_t base_descriptor_offset;
    uint64_t base_stream_offset;
    uint8_t base_screen[2048];
    uint8_t decoded[1024];
    uint8_t marker_chr[224];
    size_t encoded_size;
    size_t base_encoded_size;
    size_t marker_chr_size = 0U;
    uint8_t marker_selector = 0U;
    const uint8_t *char_map;
    const uint8_t *records[3];
    const uint8_t *cursor_record;
    size_t record_sizes[3];
    size_t cell_starts[3] = {0U, 210U, 406U};

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks < 8U ||
        chr_size != TECMO_ASSET_PACK_PRESEASON_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_PRESEASON_SIZE) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    memset(provenance, 0, sizeof(*provenance));
    if ((uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES > UINT64_MAX - prg_offset) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 CHR source offset overflowed.");
        return -1;
    }
    chr_offset = prg_offset + (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    base_descriptor_offset = fixed_cpu_offset(
        prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_DESCRIPTOR_CPU);
    base_stream_offset = bank_cpu_offset(
        prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_STREAM_CPU);
    if (!range_ok(chr_offset, chr_size, rom_size)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 CHR source is outside the ROM.");
        return -1;
    }
    for (size_t i = 0U; i < sizeof(bank_spans) / sizeof(bank_spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &bank_spans[i], 0,
                        enforce_revision_fingerprints, &offsets[i]) != 0) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 switched-bank source fingerprint mismatch.");
            return -1;
        }
    }
    cursor_record = rom + (size_t)bank_cpu_offset(prg_offset, 1U, 0x8031U);
    for (size_t i = 0U; i < sizeof(fixed_spans) / sizeof(fixed_spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &fixed_spans[i], 1,
                        enforce_revision_fingerprints, &fixed_offsets[i]) != 0) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 fixed-bank source fingerprint mismatch.");
            return -1;
        }
    }
    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_PRESEASON_CHR_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 full CHR fingerprint mismatch.");
        return -1;
    }
    if (!range_ok(base_descriptor_offset, 7U, rom_size) ||
        !range_ok(base_stream_offset, TECMO_ASSET_PACK_START_MENU_STREAM_SIZE, rom_size) ||
        memcmp(rom + (size_t)base_descriptor_offset,
               "\x7D\x7D\x56\x93\x98\x88\x00", 7U) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)prg_offset, TECMO_ASSET_PACK_PRG_BANK_BYTES,
            TECMO_ASSET_PACK_START_MENU_STREAM_CPU - 0x8000U,
            base_screen, sizeof(base_screen), &encoded_size,
            message, message_size) != 0 ||
        encoded_size != TECMO_ASSET_PACK_START_MENU_STREAM_SIZE) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 base start-menu screen was rejected.");
        return -1;
    }
    if (enforce_revision_fingerprints != 0 &&
        (tecmo_asset_pack_fnv1a32(rom + (size_t)base_descriptor_offset, 7U) !=
             0x0A5B3B88U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)base_stream_offset, encoded_size) !=
             0x8047E031U ||
         tecmo_asset_pack_fnv1a32(base_screen, sizeof(base_screen)) != 0xE1840CFEU)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 base start-menu source fingerprint mismatch.");
        return -1;
    }
    base_encoded_size = encoded_size;

    {
        const uint8_t *control_config = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0x9989U);
        const uint8_t *division_config = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0x99C4U);
        const uint8_t *difficulty_config = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0x99F4U);
        const uint8_t *team_base_config = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0xB20BU);
        if (control_config[0] != 0xA2U || division_config[0] != 0xA2U ||
            difficulty_config[0] != 0xA2U || team_base_config[0] != 0xA9U ||
            control_config[1] != 2U || division_config[1] != 3U ||
            difficulty_config[1] != 0x13U || team_base_config[1] != 4U ||
            team_base_config[2] != 0x18U || team_base_config[3] != 0x65U ||
            team_base_config[4] != 0xE2U || team_base_config[5] != 0xAAU) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 input configuration operands were rejected.");
            return -1;
        }
        configs[0] = control_config[1];
        configs[1] = difficulty_config[1];
        configs[2] = division_config[1];
        for (size_t division = 0U; division < 4U; ++division)
            configs[3U + division] = (uint8_t)(team_base_config[1] + division);
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TPRE", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_PRESEASON_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_PRESEASON_OVERLAY_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_PRESEASON_TEAM_SCREEN_COUNT);
    tecmo_asset_pack_store_u32(payload + 16U, TECMO_ASSET_PACK_PRESEASON_OVERLAY_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_PRESEASON_TEAM_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_PRESEASON_OVERLAY_DESCS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_PRESEASON_OVERLAY_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_PRESEASON_TEAM_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_PRESEASON_TEAM_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U, TECMO_ASSET_PACK_PRESEASON_MARKERS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_PRESEASON_COORDS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 48U, TECMO_ASSET_PACK_PRESEASON_DIVISION_OFFSETS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 52U, TECMO_ASSET_PACK_PRESEASON_DIVISION_COUNTS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 56U, TECMO_ASSET_PACK_PRESEASON_TEAM_IDS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 60U, TECMO_ASSET_PACK_PRESEASON_OWNERSHIP_OFFSET);
    tecmo_asset_pack_store_u32(payload + 64U, TECMO_ASSET_PACK_PRESEASON_CONFIGS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 68U, TECMO_ASSET_PACK_PRESEASON_SIZE);
    tecmo_asset_pack_store_u32(payload + 72U, TECMO_ASSET_PACK_PRESEASON_TSGM_SIZE);
    tecmo_asset_pack_store_u32(payload + 76U, TECMO_ASSET_PACK_PRESEASON_TSGM_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 80U, TECMO_ASSET_PACK_PRESEASON_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 84U, TECMO_ASSET_PACK_PRESEASON_CHR_FNV1A32);
    tecmo_asset_pack_store_u16(payload + 88U, 5U);
    tecmo_asset_pack_store_u16(payload + 90U, TECMO_ASSET_PACK_PRESEASON_MARKER_STRIDE);
    tecmo_asset_pack_store_u16(payload + 92U, TECMO_ASSET_PACK_PRESEASON_TEAM_COUNT);
    payload[94U] = 7U;
    payload[95U] = 3U;
    payload[96U] = 4U;
    payload[97U] = 1U;
    payload[98U] = 1U;
    payload[99U] = 1U;
    payload[100U] = 1U;
    payload[101U] = 5U;
    payload[102U] = 8U;
    {
        const uint8_t *params = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0x9F13U);
        const uint8_t *pointers = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, 0x9FA4U);
        const uint8_t config_ids[3] = {configs[0], configs[2], configs[1]};
        const uint8_t coordinate_counts[3] = {7U, 4U, 3U};
        int cursor_dx = (int)(int8_t)cursor_record[1U];
        int cursor_dy = (int)(int8_t)cursor_record[2U];
        uint16_t cursor_x[3];
        uint16_t cursor_y[3];
        uint16_t cursor_stride[3];
        if (cursor_record[0U] != 0x11U || cursor_dx != 0 || cursor_dy != -4 ||
            cursor_record[3U] != 0x30U || cursor_record[4U] != 0x24U) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 cursor record was rejected.");
            return -1;
        }
        for (size_t i = 0U; i < 3U; ++i) {
            uint8_t config = config_ids[i];
            uint16_t delta_cpu = tecmo_asset_pack_read_u16(pointers + config * 2U);
            uint64_t delta_offset;
            int resolved_x = (int)params[(0x9F30U - 0x9F13U) + config] + cursor_dx;
            int resolved_y = (int)params[config] + cursor_dy;
            if (resolved_x < 0 || resolved_x > 255 ||
                resolved_y < 0 || resolved_y > 239 ||
                delta_cpu < 0xA018U ||
                (uint32_t)delta_cpu + coordinate_counts[i] > 0xA09BU)
                return -1;
            delta_offset = bank_cpu_offset(prg_offset, 3U, delta_cpu);
            if (!range_ok(delta_offset, coordinate_counts[i], rom_size) ||
                rom[(size_t)delta_offset + 1U] <= rom[(size_t)delta_offset])
                return -1;
            cursor_x[i] = (uint16_t)resolved_x;
            cursor_y[i] = (uint16_t)resolved_y;
            cursor_stride[i] = (uint16_t)(rom[(size_t)delta_offset + 1U] -
                                          rom[(size_t)delta_offset]);
        }
        tecmo_asset_pack_store_u16(payload + 104U, cursor_x[0]);
        tecmo_asset_pack_store_u16(payload + 106U, cursor_y[0]);
        tecmo_asset_pack_store_u16(payload + 108U, cursor_stride[0]);
        tecmo_asset_pack_store_u16(payload + 110U, cursor_x[1]);
        {
            uint8_t division_config = config_ids[1];
            uint16_t delta_cpu = tecmo_asset_pack_read_u16(
                pointers + division_config * 2U);
            uint64_t delta_offset = bank_cpu_offset(prg_offset, 3U, delta_cpu);
            for (size_t i = 0U; i < 4U; ++i) {
                uint16_t resolved = (uint16_t)(cursor_y[1] +
                    rom[(size_t)delta_offset + i]);
                if (resolved > 239U) return -1;
                payload[112U + i] = (uint8_t)resolved;
            }
        }
        tecmo_asset_pack_store_u16(payload + 133U, cursor_x[2]);
        tecmo_asset_pack_store_u16(payload + 135U, cursor_y[2]);
        tecmo_asset_pack_store_u16(payload + 137U, cursor_stride[2]);
    }
    tecmo_asset_pack_store_u16(payload + 116U, TECMO_ASSET_PACK_PRESEASON_TEAM_ENTRY_FRAMES);
    tecmo_asset_pack_store_u16(payload + 118U, TECMO_ASSET_PACK_PRESEASON_TEAM_EXIT_FRAMES);
    tecmo_asset_pack_store_u32(payload + 120U, 0xC240U);
    tecmo_asset_pack_store_u32(payload + 124U, 0xC250U);
    payload[129U] = 0x03U;
    payload[130U] = 0xC0U;
    payload[132U] = TECMO_ASSET_PACK_PRESEASON_TEAM_PALETTE_FULL_FRAMES;
    payload[139U] = TECMO_ASSET_PACK_PRESEASON_TEAM_FIRST_VISIBLE_FRAME;
    payload[140U] = TECMO_ASSET_PACK_PRESEASON_TEAM_PALETTE_STEP_FRAMES;
    payload[141U] = TECMO_ASSET_PACK_PRESEASON_TEAM_VISIBLE_FULL_FRAME;
    payload[142U] = TECMO_ASSET_PACK_PRESEASON_RETURN_FIRST_VISIBLE_FRAME;
    payload[143U] = TECMO_ASSET_PACK_PRESEASON_RETURN_PALETTE_STEP_FRAMES;
    payload[144U] = TECMO_ASSET_PACK_PRESEASON_RETURN_VISIBLE_FULL_FRAME;
    payload[145U] = TECMO_ASSET_PACK_PRESEASON_TEAM_ENTRY_STAGE7_FRAME;
    payload[146U] = TECMO_ASSET_PACK_PRESEASON_TEAM_ENTRY_STAGE6_FRAME;
    payload[147U] = TECMO_ASSET_PACK_PRESEASON_TEAM_ENTRY_STAGE5_FRAME;
    payload[148U] = TECMO_ASSET_PACK_PRESEASON_TEAM_ENTRY_BLACK_FRAME;
    payload[149U] = TECMO_ASSET_PACK_PRESEASON_TEAM_EXIT_CAP2_FRAME;
    payload[150U] = TECMO_ASSET_PACK_PRESEASON_TEAM_EXIT_CAP1_FRAME;
    payload[151U] = TECMO_ASSET_PACK_PRESEASON_TEAM_EXIT_CAP0_FRAME;
    payload[152U] = TECMO_ASSET_PACK_PRESEASON_TEAM_EXIT_BLACK_FRAME;

    char_map = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x9AA8U);
    records[0] = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x9C5EU);
    records[1] = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x9CB9U);
    records[2] = rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0x9D22U);
    record_sizes[0] = 91U;
    record_sizes[1] = 68U;
    record_sizes[2] = 40U;
    for (size_t i = 0U; i < 3U; ++i)
        if (build_overlay(payload, i, cell_starts[i], records[i], record_sizes[i],
                          char_map, base_screen, message, message_size) != 0)
            return -1;

    for (size_t screen = 0U; screen < 4U; ++screen) {
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks,
            TECMO_ASSET_PACK_PRESEASON_DESCRIPTOR_CPU + (uint32_t)screen * 7U);
        uint64_t stream_offset = bank_cpu_offset(prg_offset, 0U, stream_cpus[screen]);
        uint64_t palette_offset = bank_cpu_offset(
            prg_offset, 0U, TECMO_ASSET_PACK_PRESEASON_PALETTE_CPU + (uint32_t)screen * 16U);
        const uint8_t *descriptor = rom + (size_t)descriptor_offset;
        uint8_t r0 = (uint8_t)(descriptor[0] * 2U);
        uint8_t r1 = (uint8_t)(descriptor[1] * 2U);
        if (!range_ok(stream_offset, stream_sizes[screen], rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(descriptor, expected_descriptors[screen], 7U) != 0 ||
            tecmo_asset_pack_validate_chr_pair(r0, r1, chr_size,
                                               "preseason team screen", message,
                                               message_size) != 0 ||
            tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)prg_offset, TECMO_ASSET_PACK_PRG_BANK_BYTES,
                stream_cpus[screen] - 0x8000U, decoded, sizeof(decoded),
                &encoded_size, message, message_size) != 0 ||
            encoded_size != stream_sizes[screen]) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 team screen descriptor or stream was rejected.");
            return -1;
        }
        if (enforce_revision_fingerprints != 0 &&
            (tecmo_asset_pack_fnv1a32(descriptor, 7U) != descriptor_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset, encoded_size) !=
                 stream_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
                 decoded_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(rom + (size_t)palette_offset, 16U) !=
                 palette_fingerprints[screen])) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 team screen fingerprint mismatch.");
            return -1;
        }
        for (size_t i = 0U; i < 960U; ++i) {
            uint8_t *cell = payload + TECMO_ASSET_PACK_PRESEASON_TEAM_CELLS_OFFSET +
                            (screen * 960U + i) * TECMO_ASSET_PACK_PRESEASON_CELL_STRIDE;
            store_cell(cell, decoded[i],
                       tecmo_asset_pack_decoded_palette_index(
                           decoded, (unsigned)(i / 32U), (unsigned)(i % 32U)),
                       r0, r1);
        }
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)palette_offset + i];
            if (color > 0x3FU) {
                tecmo_asset_pack_set_message(message, message_size,
                                             "TPRE-1 team palette color was rejected.");
                return -1;
            }
            payload[TECMO_ASSET_PACK_PRESEASON_TEAM_PALETTES_OFFSET +
                    screen * 16U + i] = color;
        }
        provenance->descriptor_offsets[screen] = descriptor_offset;
        provenance->stream_offsets[screen] = stream_offset;
        provenance->stream_sizes[screen] = encoded_size;
        provenance->palette_offsets[screen] = palette_offset;
    }

    {
        const uint8_t *marker_records = rom + (size_t)bank_cpu_offset(
            prg_offset, 1U, TECMO_ASSET_PACK_PRESEASON_MARKERS_CPU);
        uint8_t unique_tiles[10];
        size_t unique_tile_count = 0U;
        marker_selector = marker_records[3U];
        if (marker_selector != 0x30U || marker_records[13U] != marker_selector) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 team marker CHR selector was rejected.");
            return -1;
        }
        payload[128U] = marker_selector;
        for (size_t player = 0U; player < 2U; ++player) {
            const uint8_t *record = marker_records + player * 10U;
            unsigned width = record[0] & 0x0FU;
            unsigned height = record[0] >> 4U;
            int16_t anchor_dx = (int16_t)(int8_t)record[1];
            int16_t anchor_dy = (int16_t)(int8_t)record[2];
            if (width != 3U || height != 2U || record[3] != marker_selector ||
                record[9] != 0x80U) {
                tecmo_asset_pack_set_message(message, message_size,
                                             "TPRE-1 team marker geometry was rejected.");
                return -1;
            }
            for (size_t piece = 0U; piece < 5U; ++piece) {
                uint8_t raw_tile = record[4U + piece];
                uint8_t *dest = payload + TECMO_ASSET_PACK_PRESEASON_MARKERS_OFFSET +
                                (player * 5U + piece) *
                                    TECMO_ASSET_PACK_PRESEASON_MARKER_STRIDE;
                bool seen = false;
                if ((raw_tile & 1U) != 0U) {
                    tecmo_asset_pack_set_message(message, message_size,
                                                 "TPRE-1 marker tile alignment was rejected.");
                    return -1;
                }
                store_marker(dest,
                             (int16_t)(anchor_dx +
                                 (int16_t)(piece % width) * 8),
                             (int16_t)(anchor_dy +
                                 (int16_t)(piece / width) * 16),
                             marker_selector,
                             raw_tile);
                for (size_t unique = 0U; unique < unique_tile_count; ++unique)
                    if (unique_tiles[unique] == raw_tile) seen = true;
                if (!seen) unique_tiles[unique_tile_count++] = raw_tile;
            }
        }
        if (unique_tile_count != 7U) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 marker tile set was rejected.");
            return -1;
        }
        for (size_t i = 0U; i < unique_tile_count; ++i) {
            uint64_t top = (uint64_t)marker_selector * 1024U +
                           (uint64_t)unique_tiles[i] * 16U;
            if (!range_ok(top, 32U, chr_size)) return -1;
            memcpy(marker_chr + marker_chr_size,
                   rom + (size_t)(chr_offset + top), 32U);
            marker_chr_size += 32U;
        }
        if (enforce_revision_fingerprints != 0 &&
            tecmo_asset_pack_fnv1a32(marker_chr, marker_chr_size) !=
                PRESEASON_MARKER_CHR_FNV1A32) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 resolved marker CHR fingerprint mismatch.");
            return -1;
        }
    }

    {
        const uint8_t *coord_tables = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, TECMO_ASSET_PACK_PRESEASON_COORD_TABLES_CPU);
        const uint8_t *ys = coord_tables + (0xA02BU - 0xA018U);
        const uint8_t *xs = coord_tables + (0xA05FU - 0xA018U);
        for (size_t i = 0U; i < TECMO_ASSET_PACK_PRESEASON_TEAM_COUNT; ++i) {
            payload[TECMO_ASSET_PACK_PRESEASON_COORDS_OFFSET + i * 2U] = xs[i];
            payload[TECMO_ASSET_PACK_PRESEASON_COORDS_OFFSET + i * 2U + 1U] = ys[i];
        }
    }
    {
        const uint8_t *division_offsets = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, TECMO_ASSET_PACK_PRESEASON_DIVISION_OFFSETS_CPU);
        const uint8_t *team_ids = rom + (size_t)bank_cpu_offset(
            prg_offset, 3U, TECMO_ASSET_PACK_PRESEASON_TEAM_IDS_CPU);
        bool seen[TECMO_ASSET_PACK_PRESEASON_TEAM_COUNT] = {false};
        uint8_t counts[4];
        if (enforce_revision_fingerprints != 0 &&
            (tecmo_asset_pack_fnv1a32(division_offsets, 4U) != 0x3B420652U ||
             tecmo_asset_pack_fnv1a32(team_ids, 27U) != 0xB33FB72AU)) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TPRE-1 focused division/team map fingerprint mismatch.");
            return -1;
        }
        if (division_offsets[0] != 0U) return -1;
        for (size_t i = 0U; i < 4U; ++i) {
            uint8_t end = i + 1U < 4U ? division_offsets[i + 1U] : 27U;
            if (division_offsets[i] >= end || end > 27U) return -1;
            counts[i] = (uint8_t)(end - division_offsets[i]);
        }
        for (size_t i = 0U; i < 27U; ++i) {
            if (team_ids[i] >= 27U || seen[team_ids[i]]) return -1;
            seen[team_ids[i]] = true;
        }
        memcpy(payload + TECMO_ASSET_PACK_PRESEASON_DIVISION_OFFSETS_OFFSET,
               division_offsets, 4U);
        memcpy(payload + TECMO_ASSET_PACK_PRESEASON_DIVISION_COUNTS_OFFSET,
               counts, 4U);
        memcpy(payload + TECMO_ASSET_PACK_PRESEASON_TEAM_IDS_OFFSET,
               team_ids, 27U);
    }
    memcpy(payload + TECMO_ASSET_PACK_PRESEASON_OWNERSHIP_OFFSET,
           rom + (size_t)bank_cpu_offset(prg_offset, 3U,
                                         TECMO_ASSET_PACK_PRESEASON_OWNERSHIP_CPU),
           TECMO_ASSET_PACK_PRESEASON_OWNERSHIP_SIZE);
    memcpy(payload + TECMO_ASSET_PACK_PRESEASON_CONFIGS_OFFSET,
           configs, sizeof(configs));

    provenance->chr_offset = chr_offset;
    provenance->base_descriptor_offset = base_descriptor_offset;
    provenance->base_stream_offset = base_stream_offset;
    provenance->base_stream_size = base_encoded_size;
    provenance->root_vector_offset = bank_cpu_offset(prg_offset, 3U, 0x81B8U);
    provenance->flow_offset = bank_cpu_offset(prg_offset, 3U, 0x9966U);
    provenance->ownership_offset = bank_cpu_offset(prg_offset, 3U, 0x99DAU);
    provenance->difficulty_flow_offset = bank_cpu_offset(prg_offset, 3U, 0x99E6U);
    provenance->popup_builder_offset = bank_cpu_offset(prg_offset, 3U, 0x9A24U);
    provenance->char_map_offset = bank_cpu_offset(prg_offset, 3U, 0x9AA8U);
    provenance->control_record_offset = bank_cpu_offset(prg_offset, 3U, 0x9C5EU);
    provenance->division_record_offset = bank_cpu_offset(prg_offset, 3U, 0x9CB9U);
    provenance->difficulty_record_offset = bank_cpu_offset(prg_offset, 3U, 0x9D22U);
    provenance->input_wrapper_offset = bank_cpu_offset(prg_offset, 3U, 0x9EC6U);
    provenance->input_params_offset = bank_cpu_offset(prg_offset, 3U, 0x9F13U);
    provenance->input_coord_pointers_offset = bank_cpu_offset(prg_offset, 3U, 0x9FA4U);
    provenance->coord_tables_offset = bank_cpu_offset(prg_offset, 3U, 0xA018U);
    provenance->team_driver_offset = bank_cpu_offset(prg_offset, 3U, 0xB1CCU);
    provenance->team_maps_offset = bank_cpu_offset(prg_offset, 3U, 0xB273U);
    provenance->marker_records_offset = bank_cpu_offset(prg_offset, 1U, 0x8036U);
    provenance->cursor_record_offset = bank_cpu_offset(prg_offset, 1U, 0x8031U);
    provenance->controller_poll_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xD317U);
    provenance->input_helper_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xD723U);
    provenance->screen_loader_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xD92BU);
    provenance->fade_out_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xDB25U);
    provenance->fade_in_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xDB88U);
    provenance->post_return_exit_chain_offset = fixed_cpu_offset(prg_offset, prg_banks, 0xE481U);

    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_PRESEASON_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPRE-1 canonical payload fingerprint mismatch.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "Built strict ROM-derived TPRE-1 preseason menu asset.");
    return 0;
}
