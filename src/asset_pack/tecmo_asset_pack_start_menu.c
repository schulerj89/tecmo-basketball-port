#include "tecmo_asset_pack_start_menu.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define MENU_DESCRIPTOR_CPU 0xDCA1U
#define MENU_BG_R0 0xFAU
#define MENU_BG_R1 0xFAU
#define MENU_PAYLOAD_FNV1A32 0xDF89006BU
#define MENU_PALETTE_STAGES_FNV1A32 0xF83B6C17U
#define MENU_CURSOR_SELECTOR 0x30U
#define MENU_CURSOR_TILE 0x24U
#define MENU_CURSOR_CHR_OFFSET 0xC240U
#define MENU_CURSOR_CHR_FNV1A32 0x18F367C4U
#define MENU_INPUT_GATE_SEED 5U
#define MENU_PERIOD_SETUP_EXTRA_FRAMES 1U
#define MENU_EXIT_PALETTE_STEP_FRAMES 2U
#define MENU_EXIT_BLACK_FRAME 8U
#define MENU_EXIT_DISPATCH_FRAME 11U
#define MENU_ROOT_INPUT_MASK 0x80U
#define MENU_GENERIC_INPUT_MASK 0xC0U
#define MENU_PERIOD_INPUT_MASK 0xCCU
#define MENU_DIRECTION_MASK 0x0CU
#define MENU_INITIAL_INPUT_GATE 0U
#define MENU_OVERLAY_ROW_CADENCE 1U
#define MENU_OVERLAY_SETUP_ROW_START 0U
#define MENU_OVERLAY_TEARDOWN_ROW_START 1U
#define MENU_CURSOR_COMMIT_DELAY_FRAMES 1U
#define MENU_INPUT_PARAMETER_COUNT 29U
#define MENU_CURSOR_X_TABLE_CPU 0x9F30U

typedef struct MenuRecordSource {
    uint32_t cpu;
    size_t size;
    unsigned kind;
} MenuRecordSource;

static bool menu_range(uint64_t offset, uint64_t size, uint64_t total)
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

static int find_popup_selector(const uint8_t *flow,
                               size_t flow_size,
                               uint8_t *selector_out)
{
    size_t found = 0U;
    uint8_t selector = 0U;
    size_t i;

    if (flow == NULL || selector_out == NULL) return -1;
    for (i = 0U; i + 4U < flow_size; ++i) {
        if (flow[i] == 0xA2U && flow[i + 2U] == 0x20U &&
            flow[i + 3U] == 0xC6U && flow[i + 4U] == 0x9EU) {
            selector = flow[i + 1U];
            ++found;
        }
    }
    if (found != 1U || selector >= MENU_INPUT_PARAMETER_COUNT) return -1;
    *selector_out = selector;
    return 0;
}

static int build_popup_cursor_anchors(uint8_t *payload,
                                      const uint8_t *rom,
                                      uint64_t cursor_offset,
                                      uint64_t input_params_offset,
                                      const uint64_t flow_offsets[3],
                                      const size_t flow_sizes[3],
                                      char *message,
                                      size_t message_size)
{
    static const size_t x_header_offsets[3] = {
        TECMO_ASSET_PACK_START_MENU_MUSIC_CURSOR_X_HEADER_OFFSET,
        TECMO_ASSET_PACK_START_MENU_SPEED_CURSOR_X_HEADER_OFFSET,
        TECMO_ASSET_PACK_START_MENU_PERIOD_CURSOR_X_HEADER_OFFSET
    };
    static const size_t y_header_offsets[3] = {
        TECMO_ASSET_PACK_START_MENU_MUSIC_CURSOR_Y_HEADER_OFFSET,
        TECMO_ASSET_PACK_START_MENU_SPEED_CURSOR_Y_HEADER_OFFSET,
        TECMO_ASSET_PACK_START_MENU_PERIOD_CURSOR_Y_HEADER_OFFSET
    };
    const int cursor_dx = (int)(int8_t)rom[(size_t)cursor_offset + 1U];
    const int cursor_dy = (int)(int8_t)rom[(size_t)cursor_offset + 2U];
    size_t i;

    for (i = 0U; i < 3U; ++i) {
        uint8_t selector;
        int x;
        int y;
        if (find_popup_selector(rom + (size_t)flow_offsets[i],
                                flow_sizes[i], &selector) != 0) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TSGM-1 popup cursor selector flow was rejected.");
            return -1;
        }
        x = (int)rom[(size_t)input_params_offset +
                     (MENU_CURSOR_X_TABLE_CPU -
                      TECMO_ASSET_PACK_START_MENU_INPUT_PARAMS_CPU) + selector] +
            cursor_dx;
        y = (int)rom[(size_t)input_params_offset + selector] + cursor_dy;
        if (x < 0 || x > 255 || y < 0 || y > 239) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TSGM-1 popup cursor anchor is outside the native viewport.");
            return -1;
        }
        payload[x_header_offsets[i]] = (uint8_t)x;
        payload[y_header_offsets[i]] = (uint8_t)y;
    }
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
    unsigned x = 1U;
    unsigned y = 0U;
    size_t source = 4U;

    if (record == NULL || char_map == NULL || tiles == NULL || record_size < 5U) {
        tecmo_asset_pack_set_message(message, message_size, "Start-menu text record is missing.");
        return -1;
    }
    width = record[0];
    height = record[1];
    if (width < 3U || height < 3U || (size_t)width * height > tile_capacity) {
        tecmo_asset_pack_set_message(message, message_size, "Start-menu text rectangle is outside its native bounds.");
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
            if (y > height) {
                tecmo_asset_pack_set_message(message, message_size, "Start-menu text has too many rows.");
                return -1;
            }
            continue;
        }
        if (character > 0x5AU || y >= height || x + 1U >= width) {
            tecmo_asset_pack_set_message(message, message_size, "Start-menu character or line width was rejected.");
            return -1;
        }
        tiles[(size_t)y * width + x++] = char_map[character];
    }
    if (source >= record_size || record[source] != 0xFFU) {
        tecmo_asset_pack_set_message(message, message_size, "Start-menu text record lacks its terminator.");
        return -1;
    }
    *width_out = width;
    *height_out = height;
    return 0;
}

static int compose_record(uint8_t decoded[2048],
                          const uint8_t *record,
                          size_t record_size,
                          const uint8_t *char_map,
                          char *message,
                          size_t message_size)
{
    uint8_t tiles[256];
    unsigned width;
    unsigned height;
    unsigned global_x = record[2];
    unsigned origin_y = record[3];

    if (build_record_tiles(record, record_size, char_map, tiles, sizeof(tiles),
                           &width, &height, message, message_size) != 0) return -1;
    if (global_x + width > 64U || origin_y + height > 30U) {
        tecmo_asset_pack_set_messagef(message, message_size,
                                      "Start-menu record placement %u,%u %ux%u is outside two native pages.",
                                      global_x, origin_y, width, height);
        return -1;
    }
    for (unsigned y = 0U; y < height; ++y) {
        for (unsigned x = 0U; x < width; ++x) {
            unsigned gx = global_x + x;
            size_t page = gx / 32U;
            size_t index = page * 1024U + (size_t)(origin_y + y) * 32U + gx % 32U;
            decoded[index] = tiles[(size_t)y * width + x];
        }
    }
    return 0;
}

static void store_cell(uint8_t *cell, uint8_t tile, uint8_t palette_index)
{
    cell[0] = tile;
    cell[1] = palette_index;
    tecmo_asset_pack_store_u32(cell + 2U,
                               tecmo_asset_pack_bg_chr_offset(tile, MENU_BG_R0, MENU_BG_R1));
}

static void build_screen_cells(uint8_t *payload, const uint8_t decoded[2048])
{
    for (size_t page = 0U; page < 2U; ++page) {
        const uint8_t *source_page = decoded + page * 1024U;
        for (size_t i = 0U; i < 960U; ++i) {
            unsigned row = (unsigned)(i / 32U);
            unsigned col = (unsigned)(i % 32U);
            uint8_t *cell = payload + TECMO_ASSET_PACK_START_MENU_CELLS_OFFSET +
                            (page * 960U + i) * TECMO_ASSET_PACK_START_MENU_CELL_STRIDE;
            store_cell(cell, source_page[i],
                       tecmo_asset_pack_decoded_palette_index(source_page, row, col));
        }
    }
}

static void build_palette_stages(uint8_t *payload,
                                 const uint8_t title_bg[16],
                                 const uint8_t title_sprite[16],
                                 const uint8_t menu_bg[16],
                                 const uint8_t menu_sprite[16])
{
    uint8_t source[32];
    uint8_t *stages = payload + TECMO_ASSET_PACK_START_MENU_PALETTES_OFFSET;
    memcpy(source, title_bg, 16U);
    memcpy(source + 16U, title_sprite, 16U);
    for (unsigned stage = 0U; stage < 4U; ++stage)
        for (size_t i = 0U; i < 32U; ++i)
            stages[(size_t)stage * 32U + i] =
                tecmo_asset_pack_imported_fade_color(source[i], (uint8_t)(stage << 4U));
    memset(stages + 4U * 32U, 0x0F, 32U);
    memcpy(source, menu_bg, 16U);
    memcpy(source + 16U, menu_sprite, 16U);
    for (unsigned cap = 0U; cap < 4U; ++cap)
        for (size_t i = 0U; i < 32U; ++i)
            stages[(size_t)(5U + cap) * 32U + i] =
                tecmo_asset_pack_palette_brightness_cap(source[i], (uint8_t)cap);
}

static int build_sprite_pieces(uint8_t *payload,
                               const uint8_t *emblem,
                               const uint8_t cursor[5],
                               const uint8_t *chr,
                               uint64_t chr_size,
                               char *message,
                               size_t message_size)
{
    if (emblem[0] != TECMO_ASSET_PACK_START_MENU_EMBLEM_COUNT ||
        cursor[0] != 0x11U || cursor[1] != 0U || cursor[2] != 0xFCU ||
        cursor[3] != 0x30U || cursor[4] != 0x24U) {
        tecmo_asset_pack_set_message(message, message_size, "Start-menu sprite records were rejected.");
        return -1;
    }
    for (size_t i = 0U; i < TECMO_ASSET_PACK_START_MENU_EMBLEM_COUNT; ++i) {
        const uint8_t *record = emblem + 1U + i * 4U;
        uint8_t runtime_tile = (uint8_t)(record[1] + 1U);
        uint8_t selector = (runtime_tile & 0x40U) != 0U ? 0xF5U : 0xF4U;
        uint32_t top = (uint32_t)selector * 1024U + (uint32_t)(runtime_tile & 0x3EU) * 16U;
        uint8_t *piece = payload + TECMO_ASSET_PACK_START_MENU_EMBLEM_OFFSET + i * 16U;
        if ((uint64_t)top + 32U > chr_size || (record[2] & 0x1CU) != 0U) {
            tecmo_asset_pack_set_message(message, message_size, "Start-menu emblem resolves outside chr/all.");
            return -1;
        }
        tecmo_asset_pack_store_u16(piece, (uint16_t)(int16_t)(int8_t)record[3]);
        tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)(int16_t)(int8_t)record[0]);
        tecmo_asset_pack_store_u32(piece + 4U, top);
        tecmo_asset_pack_store_u32(piece + 8U, top + 16U);
        piece[12U] = record[2] & 3U;
        piece[13U] = (uint8_t)((record[2] >> 5U) & 7U);
    }
    {
        uint8_t *piece = payload + TECMO_ASSET_PACK_START_MENU_CURSOR_OFFSET;
        uint32_t top = (uint32_t)cursor[3] * 1024U + (uint32_t)cursor[4] * 16U;
        if (cursor[3] != MENU_CURSOR_SELECTOR || cursor[4] != MENU_CURSOR_TILE ||
            top != MENU_CURSOR_CHR_OFFSET || (uint64_t)top + 32U > chr_size ||
            chr == NULL ||
            tecmo_asset_pack_fnv1a32(chr + top, 32U) != MENU_CURSOR_CHR_FNV1A32) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "Start-menu cursor CHR pair was rejected.");
            return -1;
        }
        tecmo_asset_pack_store_u16(piece, 31U);
        tecmo_asset_pack_store_u16(piece + 2U, 88U);
        tecmo_asset_pack_store_u32(piece + 4U, top);
        tecmo_asset_pack_store_u32(piece + 8U, top + 16U);
    }
    return 0;
}

static int build_overlay(uint8_t *payload,
                         size_t overlay_index,
                         size_t cell_start,
                         const uint8_t *record,
                         size_t record_size,
                         const uint8_t *char_map,
                         const uint8_t decoded[2048],
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
    if (payload == NULL || record == NULL || char_map == NULL || decoded == NULL ||
        overlay_index >= TECMO_ASSET_PACK_START_MENU_OVERLAY_DESC_COUNT) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Start-menu overlay arguments were rejected.");
        return -1;
    }
    if (build_record_tiles(record, record_size, char_map, tiles, sizeof(tiles),
                           &width, &height, message, message_size) != 0) return -1;
    origin_x = record[2];
    origin_y = record[3];
    cell_count = (size_t)width * height;
    if (origin_x > 64U || width > 64U - origin_x ||
        origin_y > 30U || height > 30U - origin_y) {
        tecmo_asset_pack_set_messagef(message, message_size,
                                      "Start-menu overlay placement %u,%u %ux%u is outside two native pages.",
                                      origin_x, origin_y, width, height);
        return -1;
    }
    if (cell_start > TECMO_ASSET_PACK_START_MENU_OVERLAY_CELL_COUNT ||
        cell_count > TECMO_ASSET_PACK_START_MENU_OVERLAY_CELL_COUNT - cell_start) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Start-menu overlay cell span is outside its native bounds.");
        return -1;
    }
    desc = payload + TECMO_ASSET_PACK_START_MENU_OVERLAY_DESCS_OFFSET + overlay_index * 16U;
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
            const uint8_t *page = decoded + (gx / 32U) * 1024U;
            uint8_t palette = tecmo_asset_pack_decoded_palette_index(page, origin_y + y, gx % 32U);
            uint8_t *cell = payload + TECMO_ASSET_PACK_START_MENU_OVERLAY_CELLS_OFFSET +
                            (cell_start + (size_t)y * width + x) * 6U;
            store_cell(cell, tiles[(size_t)y * width + x], palette);
        }
    }
    return 0;
}

int tecmo_asset_pack_start_menu_self_test(char *message, size_t message_size)
{
    static const uint8_t invalid_x_record[5] = {7U, 6U, 58U, 23U, 0xFFU};
    static const uint8_t invalid_y_record[5] = {7U, 6U, 5U, 25U, 0xFFU};
    static const uint8_t valid_edge_record[5] = {7U, 6U, 57U, 24U, 0xFFU};
    uint8_t payload[TECMO_ASSET_PACK_START_MENU_SIZE];
    uint8_t decoded[2048];
    uint8_t char_map[91];

    memset(payload, 0, sizeof(payload));
    memset(decoded, 0, sizeof(decoded));
    memset(char_map, 0, sizeof(char_map));
    if (build_overlay(payload, 0U, 0U, invalid_x_record, sizeof(invalid_x_record),
                      char_map, decoded, NULL, 0U) == 0 ||
        build_overlay(payload, 0U, 0U, invalid_y_record, sizeof(invalid_y_record),
                      char_map, decoded, NULL, 0U) == 0 ||
        build_overlay(payload, 0U, 0U, valid_edge_record, sizeof(valid_edge_record),
                      char_map, decoded, message, message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Start-menu overlay placement self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "Start-menu overlay placement self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_start_game_menu(const uint8_t *rom,
                                           uint64_t rom_size,
                                           uint64_t prg_offset,
                                           uint32_t prg_banks,
                                           uint64_t chr_size,
                                           int enforce_revision_fingerprints,
                                           uint8_t *payload,
                                           size_t payload_size,
                                           TecmoStartGameMenuProvenance *provenance,
                                           char *message,
                                           size_t message_size)
{
    static const MenuRecordSource overlays[3] = {
        {TECMO_ASSET_PACK_START_MENU_MUSIC_RECORD_CPU, TECMO_ASSET_PACK_START_MENU_MUSIC_RECORD_SIZE, 0U},
        {TECMO_ASSET_PACK_START_MENU_SPEED_RECORD_CPU, TECMO_ASSET_PACK_START_MENU_SPEED_RECORD_SIZE, 1U},
        {TECMO_ASSET_PACK_START_MENU_PERIOD_RECORD_CPU, TECMO_ASSET_PACK_START_MENU_PERIOD_RECORD_SIZE, 2U}
    };
    static const uint16_t stage_frames[9] = {0U, 2U, 4U, 6U, 8U, 20U, 24U, 28U, 32U};
    static const uint8_t routes[7] = {1U, 2U, 3U, 4U, 5U, 6U, 7U};
    uint8_t decoded[2048];
    uint64_t descriptor_offset;
    uint64_t chr_offset;
    uint64_t stream_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_STREAM_CPU);
    uint64_t menu_palette_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_BG_PALETTE_CPU);
    uint64_t title_palette_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_TITLE_BG_PALETTE_CPU);
    uint64_t title_sprite_palette_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_TITLE_SPRITE_PALETTE_CPU);
    uint64_t sprite_setup_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_SPRITE_SETUP_CPU);
    uint64_t sprite_palette_offset = bank_cpu_offset(prg_offset, 0U, TECMO_ASSET_PACK_START_MENU_SPRITE_PALETTE_CPU);
    uint64_t emblem_offset = bank_cpu_offset(prg_offset, 1U, TECMO_ASSET_PACK_START_MENU_EMBLEM_CPU);
    uint64_t cursor_offset = bank_cpu_offset(prg_offset, 1U, TECMO_ASSET_PACK_START_MENU_CURSOR_CPU);
    uint64_t char_map_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_CHAR_MAP_CPU);
    uint64_t main_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_MAIN_RECORD_CPU);
    uint64_t season_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_SEASON_RECORD_CPU);
    uint64_t overlay_offsets[3];
    uint64_t period_values_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_PERIOD_VALUES_CPU);
    uint64_t loader_offset;
    uint64_t fade_out_offset;
    uint64_t fade_in_offset;
    uint64_t season_route_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_SEASON_ROUTE_CPU);
    uint64_t input_params_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_INPUT_PARAMS_CPU);
    uint64_t pointer_coord_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_POINTER_COORD_CPU);
    uint64_t music_flow_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_MUSIC_FLOW_CPU);
    uint64_t speed_flow_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_SPEED_FLOW_CPU);
    uint64_t period_flow_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_PERIOD_FLOW_CPU);
    uint64_t overlay_transfer_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_OVERLAY_TRANSFER_CPU);
    uint64_t record_render_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_RECORD_RENDER_CPU);
    uint64_t input_wrapper_offset = bank_cpu_offset(prg_offset, 3U, TECMO_ASSET_PACK_START_MENU_INPUT_WRAPPER_CPU);
    uint64_t controller_poll_offset;
    uint64_t input_helper_offset;
    uint64_t title_call_and_menu_session_setup_offset;
    uint64_t start_menu_call_and_post_return_exit_chain_offset;
    size_t encoded_size = 0U;
    const uint8_t *descriptor;

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks < 8U ||
        chr_size != TECMO_ASSET_PACK_START_MENU_REV1_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_START_MENU_SIZE) {
        tecmo_asset_pack_set_message(message, message_size, "TSGM-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    if ((uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES > UINT64_MAX - prg_offset) {
        tecmo_asset_pack_set_message(message, message_size, "TSGM-1 CHR source offset overflowed.");
        return -1;
    }
    chr_offset = prg_offset +
                 (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_cpu_offset(prg_offset, prg_banks, MENU_DESCRIPTOR_CPU);
    loader_offset = fixed_cpu_offset(prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_LOADER_CPU);
    fade_out_offset = fixed_cpu_offset(prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_FADE_OUT_CPU);
    fade_in_offset = fixed_cpu_offset(prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_FADE_IN_CPU);
    controller_poll_offset = fixed_cpu_offset(prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_CONTROLLER_POLL_CPU);
    input_helper_offset = fixed_cpu_offset(prg_offset, prg_banks, TECMO_ASSET_PACK_START_MENU_INPUT_HELPER_CPU);
    title_call_and_menu_session_setup_offset = fixed_cpu_offset(
        prg_offset, prg_banks,
        TECMO_ASSET_PACK_START_MENU_TITLE_CALL_AND_MENU_SESSION_SETUP_CPU);
    start_menu_call_and_post_return_exit_chain_offset = fixed_cpu_offset(
        prg_offset, prg_banks,
        TECMO_ASSET_PACK_START_MENU_CALL_AND_POST_RETURN_EXIT_CHAIN_CPU);
    for (size_t i = 0U; i < 3U; ++i) overlay_offsets[i] = bank_cpu_offset(prg_offset, 3U, overlays[i].cpu);
    if (!menu_range(descriptor_offset, 7U, rom_size) ||
        !menu_range(chr_offset, chr_size, rom_size) ||
        !menu_range(stream_offset, TECMO_ASSET_PACK_START_MENU_STREAM_SIZE, rom_size) ||
        !menu_range(menu_palette_offset, 16U, rom_size) || !menu_range(title_palette_offset, 16U, rom_size) ||
        !menu_range(title_sprite_palette_offset, 16U, rom_size) || !menu_range(sprite_palette_offset, 16U, rom_size) ||
        !menu_range(sprite_setup_offset, 7U, rom_size) ||
        !menu_range(emblem_offset, 197U, rom_size) || !menu_range(cursor_offset, 5U, rom_size) ||
        !menu_range(char_map_offset, 91U, rom_size) ||
        !menu_range(main_offset, TECMO_ASSET_PACK_START_MENU_MAIN_RECORD_SIZE, rom_size) ||
        !menu_range(season_offset, TECMO_ASSET_PACK_START_MENU_SEASON_RECORD_SIZE, rom_size) ||
        !menu_range(overlay_offsets[0], overlays[0].size, rom_size) ||
        !menu_range(overlay_offsets[1], overlays[1].size, rom_size) ||
        !menu_range(overlay_offsets[2], overlays[2].size, rom_size) ||
        !menu_range(period_values_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_VALUE_COUNT, rom_size) ||
        !menu_range(loader_offset, TECMO_ASSET_PACK_START_MENU_LOADER_SIZE, rom_size) ||
        !menu_range(fade_out_offset, TECMO_ASSET_PACK_START_MENU_FADE_OUT_SIZE, rom_size) ||
        !menu_range(fade_in_offset, TECMO_ASSET_PACK_START_MENU_FADE_IN_SIZE, rom_size) ||
        !menu_range(season_route_offset, TECMO_ASSET_PACK_START_MENU_SEASON_ROUTE_SIZE, rom_size) ||
        !menu_range(input_params_offset, TECMO_ASSET_PACK_START_MENU_INPUT_PARAMS_SIZE, rom_size) ||
        !menu_range(pointer_coord_offset, TECMO_ASSET_PACK_START_MENU_POINTER_COORD_SIZE, rom_size) ||
        !menu_range(music_flow_offset, TECMO_ASSET_PACK_START_MENU_MUSIC_FLOW_SIZE, rom_size) ||
        !menu_range(speed_flow_offset, TECMO_ASSET_PACK_START_MENU_SPEED_FLOW_SIZE, rom_size) ||
        !menu_range(period_flow_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_FLOW_SIZE, rom_size) ||
        !menu_range(overlay_transfer_offset, TECMO_ASSET_PACK_START_MENU_OVERLAY_TRANSFER_SIZE, rom_size) ||
        !menu_range(record_render_offset, TECMO_ASSET_PACK_START_MENU_RECORD_RENDER_SIZE, rom_size) ||
        !menu_range(input_wrapper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_WRAPPER_SIZE, rom_size) ||
        !menu_range(controller_poll_offset, TECMO_ASSET_PACK_START_MENU_CONTROLLER_POLL_SIZE, rom_size) ||
        !menu_range(input_helper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_HELPER_SIZE, rom_size) ||
        !menu_range(title_call_and_menu_session_setup_offset,
                    TECMO_ASSET_PACK_START_MENU_TITLE_CALL_AND_MENU_SESSION_SETUP_SIZE,
                    rom_size) ||
        !menu_range(start_menu_call_and_post_return_exit_chain_offset,
                    TECMO_ASSET_PACK_START_MENU_CALL_AND_POST_RETURN_EXIT_CHAIN_SIZE,
                    rom_size)) {
        tecmo_asset_pack_set_message(message, message_size, "TSGM-1 source range is outside the ROM.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    if (descriptor[0] != 0x7DU || descriptor[1] != 0x7DU ||
        tecmo_asset_pack_read_u16(descriptor + 2U) != TECMO_ASSET_PACK_START_MENU_BG_PALETTE_CPU ||
        tecmo_asset_pack_read_u16(descriptor + 4U) != TECMO_ASSET_PACK_START_MENU_STREAM_CPU ||
        descriptor[6] != 0U ||
        memcmp(rom + (size_t)sprite_setup_offset, "\xA2\xF4\x86\x57\xE8\x86\x58", 7U) != 0 ||
        tecmo_asset_pack_validate_chr_pair(MENU_BG_R0, MENU_BG_R1, chr_size,
                                           "start-game menu background", message, message_size) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)prg_offset,
                                            TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                            TECMO_ASSET_PACK_START_MENU_STREAM_CPU - 0x8000U,
                                            decoded, sizeof(decoded), &encoded_size,
                                            message, message_size) != 0 ||
        encoded_size != TECMO_ASSET_PACK_START_MENU_STREAM_SIZE) {
        tecmo_asset_pack_set_message(message, message_size, "TSGM-1 descriptor, CHR setup, or screen stream was rejected.");
        return -1;
    }
    if (enforce_revision_fingerprints != 0 &&
        (tecmo_asset_pack_fnv1a32(descriptor, 7U) != 0x0A5B3B88U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset, encoded_size) != 0x8047E031U ||
         tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) != 0xE1840CFEU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)menu_palette_offset, 16U) != 0xF16D31BFU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)title_palette_offset, 16U) != 0xBBF7850BU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)title_sprite_palette_offset, 16U) != 0xACF5D9A1U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_setup_offset, 7U) != 0x23BDC5CEU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_palette_offset, 16U) != 0xF85BA74AU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)emblem_offset, 197U) != 0x669E53D3U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)cursor_offset, 5U) != 0x7D5835D4U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)char_map_offset, 91U) != 0x724F80CEU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)main_offset, TECMO_ASSET_PACK_START_MENU_MAIN_RECORD_SIZE) != 0x8CFF0188U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)season_offset, TECMO_ASSET_PACK_START_MENU_SEASON_RECORD_SIZE) != 0x01620B02U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)overlay_offsets[0], overlays[0].size) != 0xA144CD78U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)overlay_offsets[1], overlays[1].size) != 0xBE4B508DU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)overlay_offsets[2], overlays[2].size) != 0x30B68A59U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)period_values_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_VALUE_COUNT) != 0x0F3A2C36U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)loader_offset, TECMO_ASSET_PACK_START_MENU_LOADER_SIZE) != 0x835283BEU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)fade_out_offset, TECMO_ASSET_PACK_START_MENU_FADE_OUT_SIZE) != 0x5D98AB7AU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)fade_in_offset, TECMO_ASSET_PACK_START_MENU_FADE_IN_SIZE) != 0xD75D6EEAU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)season_route_offset, TECMO_ASSET_PACK_START_MENU_SEASON_ROUTE_SIZE) != 0x7CBACC29U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)input_params_offset, TECMO_ASSET_PACK_START_MENU_INPUT_PARAMS_SIZE) != 0xDE568C7BU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)pointer_coord_offset, TECMO_ASSET_PACK_START_MENU_POINTER_COORD_SIZE) != 0x218E8BCBU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)music_flow_offset, TECMO_ASSET_PACK_START_MENU_MUSIC_FLOW_SIZE) != 0x051E3038U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)speed_flow_offset, TECMO_ASSET_PACK_START_MENU_SPEED_FLOW_SIZE) != 0x223D3BB4U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)period_flow_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_FLOW_SIZE) != 0x4234F755U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)overlay_transfer_offset, TECMO_ASSET_PACK_START_MENU_OVERLAY_TRANSFER_SIZE) != 0x4325EDF8U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)record_render_offset, TECMO_ASSET_PACK_START_MENU_RECORD_RENDER_SIZE) != 0x7590F31BU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)input_wrapper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_WRAPPER_SIZE) != 0x6C2709EBU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)controller_poll_offset, TECMO_ASSET_PACK_START_MENU_CONTROLLER_POLL_SIZE) != 0x8868D9B5U ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)input_helper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_HELPER_SIZE) != 0xAE47C4A0U ||
         tecmo_asset_pack_fnv1a32(
             rom + (size_t)title_call_and_menu_session_setup_offset,
             TECMO_ASSET_PACK_START_MENU_TITLE_CALL_AND_MENU_SESSION_SETUP_SIZE) != 0x4FBABE09U ||
         tecmo_asset_pack_fnv1a32(
             rom + (size_t)start_menu_call_and_post_return_exit_chain_offset,
             TECMO_ASSET_PACK_START_MENU_CALL_AND_POST_RETURN_EXIT_CHAIN_SIZE) != 0x76C592FCU ||
         tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset, (size_t)chr_size) !=
             TECMO_ASSET_PACK_START_MENU_REV1_CHR_FNV1A32)) {
        tecmo_asset_pack_set_message(message, message_size, "TSGM-1 Rev1 fingerprint mismatch.");
        return -1;
    }
    if (compose_record(decoded, rom + (size_t)main_offset, TECMO_ASSET_PACK_START_MENU_MAIN_RECORD_SIZE,
                       rom + (size_t)char_map_offset, message, message_size) != 0 ||
        compose_record(decoded, rom + (size_t)season_offset, TECMO_ASSET_PACK_START_MENU_SEASON_RECORD_SIZE,
                       rom + (size_t)char_map_offset, message, message_size) != 0) return -1;
    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) != 0x661750F3U) {
        tecmo_asset_pack_set_messagef(message, message_size,
                                      "TSGM-1 composed menu screen fingerprint mismatch (%08X).",
                                      tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)));
        return -1;
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TSGM", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_START_MENU_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 2U);
    tecmo_asset_pack_store_u16(payload + 14U, 6U);
    tecmo_asset_pack_store_u32(payload + 16U, TECMO_ASSET_PACK_START_MENU_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_START_MENU_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_START_MENU_PALETTES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 28U, TECMO_ASSET_PACK_START_MENU_PALETTE_STAGE_COUNT);
    tecmo_asset_pack_store_u16(payload + 30U, 32U);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_START_MENU_EMBLEM_OFFSET);
    tecmo_asset_pack_store_u16(payload + 36U, 49U);
    tecmo_asset_pack_store_u16(payload + 38U, 16U);
    tecmo_asset_pack_store_u32(payload + 40U, TECMO_ASSET_PACK_START_MENU_CURSOR_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U, (uint32_t)payload_size);
    tecmo_asset_pack_store_u16(payload + 48U, 32U);
    tecmo_asset_pack_store_u16(payload + 50U, 7U);
    tecmo_asset_pack_store_u16(payload + 52U, 6U);
    tecmo_asset_pack_store_u16(payload + 54U, 8U);
    tecmo_asset_pack_store_u16(payload + 56U, 32U);
    tecmo_asset_pack_store_u16(payload + 58U, 8U);
    tecmo_asset_pack_store_u16(payload + 60U, 5U);
    tecmo_asset_pack_store_u16(payload + 62U, 31U);
    tecmo_asset_pack_store_u16(payload + 64U, 88U);
    tecmo_asset_pack_store_u16(payload + 66U, 16U);
    tecmo_asset_pack_store_u16(payload + 68U, 184U);
    tecmo_asset_pack_store_u16(payload + 70U, 60U);
    tecmo_asset_pack_store_u32(payload + 72U, TECMO_ASSET_PACK_START_MENU_OVERLAY_DESCS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 76U, 3U);
    tecmo_asset_pack_store_u16(payload + 78U, 16U);
    tecmo_asset_pack_store_u32(payload + 80U, TECMO_ASSET_PACK_START_MENU_OVERLAY_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 84U, TECMO_ASSET_PACK_START_MENU_OVERLAY_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 88U, TECMO_ASSET_PACK_START_MENU_DIGITS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 92U, 10U);
    tecmo_asset_pack_store_u16(payload + 94U, 6U);
    for (size_t i = 0U; i < 9U; ++i) tecmo_asset_pack_store_u16(payload + 96U + i * 2U, stage_frames[i]);
    memcpy(payload + 114U, routes, sizeof(routes));
    memcpy(payload + TECMO_ASSET_PACK_START_MENU_PERIOD_VALUES_HEADER_OFFSET,
           rom + (size_t)period_values_offset,
           TECMO_ASSET_PACK_START_MENU_PERIOD_VALUE_COUNT);
    payload[TECMO_ASSET_PACK_START_MENU_INPUT_GATE_SEED_HEADER_OFFSET] = MENU_INPUT_GATE_SEED;
    payload[TECMO_ASSET_PACK_START_MENU_PERIOD_SETUP_EXTRA_HEADER_OFFSET] =
        MENU_PERIOD_SETUP_EXTRA_FRAMES;
    payload[TECMO_ASSET_PACK_START_MENU_EXIT_PALETTE_STEP_HEADER_OFFSET] =
        MENU_EXIT_PALETTE_STEP_FRAMES;
    payload[TECMO_ASSET_PACK_START_MENU_EXIT_BLACK_FRAME_HEADER_OFFSET] =
        MENU_EXIT_BLACK_FRAME;
    payload[TECMO_ASSET_PACK_START_MENU_EXIT_DISPATCH_FRAME_HEADER_OFFSET] =
        MENU_EXIT_DISPATCH_FRAME;
    payload[TECMO_ASSET_PACK_START_MENU_ROOT_INPUT_MASK_HEADER_OFFSET] =
        MENU_ROOT_INPUT_MASK;
    payload[TECMO_ASSET_PACK_START_MENU_GENERIC_INPUT_MASK_HEADER_OFFSET] =
        MENU_GENERIC_INPUT_MASK;
    payload[TECMO_ASSET_PACK_START_MENU_PERIOD_INPUT_MASK_HEADER_OFFSET] =
        MENU_PERIOD_INPUT_MASK;
    payload[TECMO_ASSET_PACK_START_MENU_DIRECTION_MASK_HEADER_OFFSET] =
        MENU_DIRECTION_MASK;
    payload[TECMO_ASSET_PACK_START_MENU_INITIAL_GATE_HEADER_OFFSET] =
        MENU_INITIAL_INPUT_GATE;
    payload[TECMO_ASSET_PACK_START_MENU_ROW_CADENCE_HEADER_OFFSET] =
        MENU_OVERLAY_ROW_CADENCE;
    payload[TECMO_ASSET_PACK_START_MENU_SETUP_ROW_START_HEADER_OFFSET] =
        MENU_OVERLAY_SETUP_ROW_START;
    payload[TECMO_ASSET_PACK_START_MENU_TEARDOWN_ROW_START_HEADER_OFFSET] =
        MENU_OVERLAY_TEARDOWN_ROW_START;
    payload[TECMO_ASSET_PACK_START_MENU_METADATA_ALIGNMENT_HEADER_OFFSET] = 0U;
    tecmo_asset_pack_store_u32(payload + TECMO_ASSET_PACK_START_MENU_CHR_SIZE_HEADER_OFFSET,
                               TECMO_ASSET_PACK_START_MENU_REV1_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + TECMO_ASSET_PACK_START_MENU_CHR_FNV1A32_HEADER_OFFSET,
                               TECMO_ASSET_PACK_START_MENU_REV1_CHR_FNV1A32);
    payload[TECMO_ASSET_PACK_START_MENU_CURSOR_COMMIT_DELAY_HEADER_OFFSET] =
        MENU_CURSOR_COMMIT_DELAY_FRAMES;
    {
        const uint64_t popup_flow_offsets[3] = {
            music_flow_offset, speed_flow_offset, period_flow_offset
        };
        const size_t popup_flow_sizes[3] = {
            TECMO_ASSET_PACK_START_MENU_MUSIC_FLOW_SIZE,
            TECMO_ASSET_PACK_START_MENU_SPEED_FLOW_SIZE,
            TECMO_ASSET_PACK_START_MENU_PERIOD_FLOW_SIZE
        };
        if (build_popup_cursor_anchors(payload, rom, cursor_offset,
                                       input_params_offset, popup_flow_offsets,
                                       popup_flow_sizes, message, message_size) != 0)
            return -1;
    }

    build_screen_cells(payload, decoded);
    build_palette_stages(payload, rom + (size_t)title_palette_offset,
                         rom + (size_t)title_sprite_palette_offset,
                         rom + (size_t)menu_palette_offset,
                         rom + (size_t)sprite_palette_offset);
    if (build_sprite_pieces(payload, rom + (size_t)emblem_offset,
                            rom + (size_t)cursor_offset, rom + (size_t)chr_offset,
                            chr_size, message, message_size) != 0 ||
        build_overlay(payload, 0U, 0U, rom + (size_t)overlay_offsets[0], overlays[0].size,
                      rom + (size_t)char_map_offset, decoded, message, message_size) != 0 ||
        build_overlay(payload, 1U, 42U, rom + (size_t)overlay_offsets[1], overlays[1].size,
                      rom + (size_t)char_map_offset, decoded, message, message_size) != 0 ||
        build_overlay(payload, 2U, 122U, rom + (size_t)overlay_offsets[2], overlays[2].size,
                      rom + (size_t)char_map_offset, decoded, message, message_size) != 0) return -1;
    for (size_t i = 0U; i < 10U; ++i) {
        uint8_t tile = rom[(size_t)char_map_offset + 0x30U + i];
        store_cell(payload + TECMO_ASSET_PACK_START_MENU_DIGITS_OFFSET + i * 6U, tile, 0U);
    }
    if (enforce_revision_fingerprints != 0 &&
        (tecmo_asset_pack_fnv1a32(payload, payload_size) != MENU_PAYLOAD_FNV1A32 ||
         tecmo_asset_pack_fnv1a32(
             payload + TECMO_ASSET_PACK_START_MENU_PALETTES_OFFSET,
             TECMO_ASSET_PACK_START_MENU_PALETTE_STAGE_COUNT * 32U) !=
             MENU_PALETTE_STAGES_FNV1A32)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TSGM-1 generated Rev1 payload fingerprint mismatch.");
        return -1;
    }

    memset(provenance, 0, sizeof(*provenance));
    provenance->chr_offset = chr_offset;
    provenance->descriptor_offset = descriptor_offset;
    provenance->stream_offset = stream_offset;
    provenance->stream_size = encoded_size;
    provenance->background_palette_offset = menu_palette_offset;
    provenance->title_background_palette_offset = title_palette_offset;
    provenance->title_sprite_palette_offset = title_sprite_palette_offset;
    provenance->sprite_setup_offset = sprite_setup_offset;
    provenance->sprite_palette_offset = sprite_palette_offset;
    provenance->emblem_offset = emblem_offset;
    provenance->cursor_offset = cursor_offset;
    provenance->char_map_offset = char_map_offset;
    provenance->main_record_offset = main_offset;
    provenance->season_record_offset = season_offset;
    provenance->music_record_offset = overlay_offsets[0];
    provenance->speed_record_offset = overlay_offsets[1];
    provenance->period_record_offset = overlay_offsets[2];
    provenance->period_values_offset = period_values_offset;
    provenance->loader_offset = loader_offset;
    provenance->fade_out_offset = fade_out_offset;
    provenance->fade_in_offset = fade_in_offset;
    provenance->season_route_offset = season_route_offset;
    provenance->input_params_offset = input_params_offset;
    provenance->pointer_coord_offset = pointer_coord_offset;
    provenance->music_flow_offset = music_flow_offset;
    provenance->speed_flow_offset = speed_flow_offset;
    provenance->period_flow_offset = period_flow_offset;
    provenance->overlay_transfer_offset = overlay_transfer_offset;
    provenance->record_render_offset = record_render_offset;
    provenance->input_wrapper_offset = input_wrapper_offset;
    provenance->controller_poll_offset = controller_poll_offset;
    provenance->input_helper_offset = input_helper_offset;
    provenance->title_call_and_menu_session_setup_offset =
        title_call_and_menu_session_setup_offset;
    provenance->start_menu_call_and_post_return_exit_chain_offset =
        start_menu_call_and_post_return_exit_chain_offset;
    return 0;
}
