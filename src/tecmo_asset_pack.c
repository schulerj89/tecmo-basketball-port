#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack.h"

#include "asset_pack/tecmo_asset_pack_d9f6.h"
#include "asset_pack/tecmo_asset_pack_finale.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "asset_pack/tecmo_asset_pack_opening.h"
#include "asset_pack/tecmo_asset_pack_source_map.h"
#include "asset_pack/tecmo_asset_pack_util.h"
#include "asset_pack/tecmo_asset_pack_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif


static uint64_t prg_bank_cpu_source_offset(uint64_t prg_offset,
                                           uint32_t prg_banks,
                                           uint32_t bank,
                                           uint32_t cpu_address)
{
    if (bank >= prg_banks ||
        cpu_address < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
        cpu_address >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT) {
        return 0U;
    }

    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu_address - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int build_arena_background_layer(const uint8_t *rom,
                                        uint64_t rom_size,
                                        uint64_t prg_offset,
                                        uint32_t prg_banks,
                                        uint64_t chr_size,
                                        uint8_t *payload,
                                        size_t payload_size,
                                        TecmoArenaBackgroundProvenance *provenance,
                                        char *message,
                                        size_t message_size)
{
    uint8_t decoded[TECMO_ASSET_PACK_SCREEN_DECODED_SIZE];
    uint32_t fixed_bank;
    uint64_t descriptor_offset;
    const uint8_t *descriptor;
    uint32_t palette_cpu;
    uint32_t stream_cpu;
    uint32_t stream_bank;
    uint64_t stream_bank_offset;
    uint64_t palette_offset;
    uint8_t upper_r0;
    uint8_t upper_r1;
    uint8_t lower_r0;
    uint8_t lower_r1;
    size_t encoded_size = 0U;

    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_ARENA_LAYER_SIZE ||
        prg_banks <= TECMO_ASSET_PACK_ARENA_BANK04) {
        tecmo_asset_pack_set_message(message, message_size, "Arena screen import requires a compatible ROM with Bank04.");
        return -1;
    }

    fixed_bank = prg_banks - 1U;
    descriptor_offset = prg_offset +
                        (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
                        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                        TECMO_ASSET_PACK_ARENA_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    if (descriptor_offset > rom_size ||
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE > rom_size - descriptor_offset) {
        tecmo_asset_pack_set_message(message, message_size, "Arena screen descriptor is outside the fixed PRG bank.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    palette_cpu = (uint32_t)descriptor[2] | ((uint32_t)descriptor[3] << 8U);
    stream_cpu = (uint32_t)descriptor[4] | ((uint32_t)descriptor[5] << 8U);
    stream_bank = descriptor[6];

    if (descriptor[0] > 0x7FU || descriptor[1] > 0x7FU) {
        tecmo_asset_pack_set_message(message, message_size, "Arena upper CHR pair overflows MMC3 selectors.");
        return -1;
    }
    upper_r0 = (uint8_t)(descriptor[0] * 2U);
    upper_r1 = (uint8_t)(descriptor[1] * 2U);
    if (stream_bank >= prg_banks ||
        palette_cpu < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
        palette_cpu >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT ||
        stream_cpu < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
        stream_cpu >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT) {
        tecmo_asset_pack_set_message(message, message_size, "Arena screen descriptor has an invalid stream bank or CPU pointer.");
        return -1;
    }

    stream_bank_offset = prg_offset + (uint64_t)stream_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    palette_offset = stream_bank_offset +
                     (uint64_t)(palette_cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    if (palette_offset > rom_size || 16U > rom_size - palette_offset) {
        tecmo_asset_pack_set_message(message, message_size, "Arena background palette is outside its descriptor bank.");
        return -1;
    }

    {
        uint64_t fixed_offset = prg_offset +
                                (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t lower_r0_offset = fixed_offset +
                                   (TECMO_ASSET_PACK_ARENA_LOWER_R0_TABLE_CPU - 0xC000U) +
                                   TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX;
        uint64_t lower_r1_offset = fixed_offset +
                                   (TECMO_ASSET_PACK_ARENA_LOWER_R1_TABLE_CPU - 0xC000U) +
                                   TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX;
        if (lower_r0_offset >= rom_size || lower_r1_offset >= rom_size) {
            tecmo_asset_pack_set_message(message, message_size, "Arena lower CHR selectors are outside fixed PRG.");
            return -1;
        }
        lower_r0 = rom[(size_t)lower_r0_offset];
        lower_r1 = rom[(size_t)lower_r1_offset];
        provenance->lower_r0_table_source_offset = lower_r0_offset -
                                                   TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX;
        provenance->lower_r1_table_source_offset = lower_r1_offset -
                                                   TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX;
    }

    if (tecmo_asset_pack_validate_chr_pair(upper_r0, upper_r1, chr_size, "upper", message, message_size) != 0 ||
        tecmo_asset_pack_validate_chr_pair(lower_r0, lower_r1, chr_size, "lower", message, message_size) != 0) {
        return -1;
    }
    if (tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)stream_bank_offset,
                           (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           (size_t)(stream_cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE),
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0) {
        return -1;
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TATL", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_ARENA_LAYER_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, TECMO_ASSET_PACK_ARENA_LAYER_WIDTH);
    tecmo_asset_pack_store_u16(payload + 10U, TECMO_ASSET_PACK_ARENA_LAYER_HEIGHT);
    tecmo_asset_pack_store_u16(payload + 12U, 32U);
    tecmo_asset_pack_store_u16(payload + 14U, 30U);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_ARENA_LAYER_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 18U, 0U);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_ARENA_LAYER_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_ARENA_LAYER_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 28U, 32U);
    memcpy(payload + 32U, rom + (size_t)palette_offset, 16U);

    for (uint32_t destination_row = 0U;
         destination_row < TECMO_ASSET_PACK_ARENA_LAYER_HEIGHT;
         ++destination_row) {
        uint32_t page;
        uint32_t source_row;

        if (destination_row < 16U) {
            page = 0U;
            source_row = destination_row;
        } else if (destination_row < 38U) {
            page = 1U;
            source_row = destination_row - 15U;
        } else {
            page = 0U;
            source_row = destination_row - 22U;
        }

        for (uint32_t column = 0U; column < TECMO_ASSET_PACK_ARENA_LAYER_WIDTH; ++column) {
            size_t nametable = (size_t)page * TECMO_ASSET_PACK_NAMETABLE_SIZE;
            uint8_t tile_id = decoded[nametable + (size_t)source_row * 32U + column];
            size_t attribute_index = nametable + TECMO_ASSET_PACK_ATTRIBUTE_OFFSET +
                                     (size_t)(source_row / 4U) * 8U + column / 4U;
            unsigned int attribute_shift =
                ((source_row & 2U) != 0U ? 4U : 0U) + ((column & 2U) != 0U ? 2U : 0U);
            uint8_t palette_index = (uint8_t)((decoded[attribute_index] >> attribute_shift) & 3U);
            uint8_t r0 = page == 0U && source_row < 26U ? upper_r0 : lower_r0;
            uint8_t r1 = page == 0U && source_row < 26U ? upper_r1 : lower_r1;
            uint8_t selector = tile_id < 128U ? r0 : r1;
            uint64_t chr_byte_offset = (uint64_t)selector * 1024U +
                                       (uint64_t)(tile_id & 0x7FU) * 16U;
            size_t cell_index = (size_t)destination_row * TECMO_ASSET_PACK_ARENA_LAYER_WIDTH + column;
            uint8_t *cell = payload + TECMO_ASSET_PACK_ARENA_LAYER_HEADER_SIZE +
                            cell_index * TECMO_ASSET_PACK_ARENA_LAYER_CELL_STRIDE;

            if (chr_byte_offset > UINT32_MAX || chr_byte_offset + 16U > chr_size) {
                tecmo_asset_pack_set_messagef(message,
                             message_size,
                             "Arena tile at row %u column %u resolves outside chr/all.",
                             destination_row,
                             column);
                return -1;
            }
            cell[0] = tile_id;
            cell[1] = palette_index;
            tecmo_asset_pack_store_u32(cell + 2U, (uint32_t)chr_byte_offset);
        }
    }

    provenance->route_bank = TECMO_ASSET_PACK_ARENA_BANK04;
    provenance->route_cpu = TECMO_ASSET_PACK_ARENA_BACKGROUND_ROUTE_CPU;
    provenance->route_source_offset = prg_offset +
                                      (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 *
                                          TECMO_ASSET_PACK_PRG_BANK_BYTES +
                                      (TECMO_ASSET_PACK_ARENA_BACKGROUND_ROUTE_CPU -
                                       TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    provenance->descriptor_bank = fixed_bank;
    provenance->descriptor_cpu = TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
                                 TECMO_ASSET_PACK_ARENA_SCREEN_ID *
                                     TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    provenance->descriptor_source_offset = descriptor_offset;
    provenance->stream_bank = stream_bank;
    provenance->stream_cpu = stream_cpu;
    provenance->stream_source_offset = stream_bank_offset +
                                       (uint64_t)(stream_cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    provenance->stream_encoded_size = encoded_size;
    provenance->palette_cpu = palette_cpu;
    provenance->palette_source_offset = palette_offset;
    return 0;
}

static int build_arena_sprite_groups(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint32_t prg_banks,
                                     uint64_t chr_offset,
                                     uint64_t chr_size,
                                     uint8_t *payload,
                                     size_t payload_size,
                                     TecmoArenaSpriteGroupsProvenance *provenance,
                                     char *message,
                                     size_t message_size)
{
    static const uint8_t expected_counts[2] = {
        TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT,
        TECMO_ASSET_PACK_ARENA_GOAL_COUNT
    };
    static const uint8_t expected_seeds[4] = {0x00U, 0x1EU, 0x00U, 0x01U};
    static const uint8_t expected_params[4] = {0x00U, 0xB8U, 0x00U, 0x01U};
    uint64_t bank04_offset;
    uint64_t palette_offset;
    uint64_t pointer_table_offset;
    uint64_t seeds_offset;
    uint64_t params_offset;
    uint32_t stream_cpu[2];
    uint32_t stream_size[2];
    uint64_t stream_offset[2];
    size_t output_piece = 0U;
    size_t connector_overlay_piece_count = 0U;

    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE ||
        prg_banks <= TECMO_ASSET_PACK_ARENA_BANK04) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite import requires Bank00 and Bank04.");
        return -1;
    }
    if (chr_size < ((uint64_t)TECMO_ASSET_PACK_ARENA_SPRITE_CHR_R3 + 1U) * 1024U) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite import requires CHR pages R2=08 and R3=09.");
        return -1;
    }

    bank04_offset = prg_offset +
                    (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 *
                        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    palette_offset = bank04_offset +
                     (TECMO_ASSET_PACK_ARENA_PALETTE_CPU -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    pointer_table_offset = prg_offset +
                           (TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU -
                            TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    seeds_offset = bank04_offset +
                   (TECMO_ASSET_PACK_ARENA_SEEDS_CPU -
                    TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    params_offset = bank04_offset +
                    (TECMO_ASSET_PACK_ARENA_PARAMS_CPU -
                     TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    if (palette_offset > rom_size || 16U > rom_size - palette_offset ||
        pointer_table_offset > rom_size || 4U > rom_size - pointer_table_offset ||
        seeds_offset > rom_size || sizeof(expected_seeds) > rom_size - seeds_offset ||
        params_offset > rom_size || sizeof(expected_params) > rom_size - params_offset) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite source contract is outside the ROM.");
        return -1;
    }

    for (size_t palette = 0U; palette < 4U; ++palette) {
        const uint8_t *source = rom + (size_t)palette_offset + palette * 4U;

        if (source[0] != 2U) {
            tecmo_asset_pack_set_messagef(message,
                         message_size,
                         "Arena sprite subpalette %u has control %u; expected 2.",
                         (unsigned int)palette,
                         (unsigned int)source[0]);
            return -1;
        }
        for (size_t color = 1U; color < 4U; ++color) {
            if (source[color] > 0x3FU) {
                tecmo_asset_pack_set_messagef(message,
                             message_size,
                             "Arena sprite subpalette %u color %u is outside the NES palette.",
                             (unsigned int)palette,
                             (unsigned int)color);
                return -1;
            }
        }
    }
    if (memcmp(rom + (size_t)seeds_offset, expected_seeds, sizeof(expected_seeds)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite seed bytes do not match normalized anchors.");
        return -1;
    }
    if (memcmp(rom + (size_t)params_offset, expected_params, sizeof(expected_params)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite parameter bytes do not match normalized anchors.");
        return -1;
    }

    for (size_t selector = 0U; selector < 2U; ++selector) {
        uint32_t pointer = tecmo_asset_pack_read_u16(rom + (size_t)pointer_table_offset + selector * 2U);
        uint64_t source_offset;

        if (pointer < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
            pointer >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT) {
            tecmo_asset_pack_set_messagef(message,
                         message_size,
                         "Arena sprite selector %u has invalid pointer $%04X.",
                         (unsigned int)selector,
                         pointer);
            return -1;
        }
        source_offset = prg_offset +
                        (uint64_t)(pointer - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        if (source_offset >= rom_size || rom[(size_t)source_offset] != expected_counts[selector]) {
            tecmo_asset_pack_set_messagef(message,
                         message_size,
                         "Arena sprite selector %u does not have the expected %u records.",
                         (unsigned int)selector,
                         (unsigned int)expected_counts[selector]);
            return -1;
        }

        stream_size[selector] = 1U + (uint32_t)expected_counts[selector] * 4U;
        if (stream_size[selector] > TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT - pointer ||
            stream_size[selector] > rom_size - source_offset) {
            tecmo_asset_pack_set_messagef(message,
                         message_size,
                         "Arena sprite selector %u stream crosses its PRG bank.",
                         (unsigned int)selector);
            return -1;
        }
        stream_cpu[selector] = pointer;
        stream_offset[selector] = source_offset;
    }
    if (stream_cpu[0] == stream_cpu[1] ||
        !((uint64_t)stream_cpu[0] + stream_size[0] <= stream_cpu[1] ||
          (uint64_t)stream_cpu[1] + stream_size[1] <= stream_cpu[0])) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite stream pointers overlap.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TASG", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 2U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_ARENA_SPRITE_GROUP_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 2U);
    tecmo_asset_pack_store_u16(payload + 10U, TECMO_ASSET_PACK_ARENA_SPRITE_GROUP_STRIDE);
    tecmo_asset_pack_store_u32(payload + 12U, TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_COUNT);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 18U, 1U);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET);

    for (size_t palette = 0U; palette < 4U; ++palette) {
        const uint8_t *source = rom + (size_t)palette_offset + palette * 4U;
        uint8_t *dest = payload + TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET +
                        palette * 4U;
        dest[0] = 0x0FU;
        memcpy(dest + 1U, source + 1U, 3U);
    }

    {
        uint8_t *jumbotron = payload + TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_OFFSET;
        uint8_t *goal = jumbotron + TECMO_ASSET_PACK_ARENA_SPRITE_GROUP_STRIDE;

        tecmo_asset_pack_store_u16(jumbotron + 0U, 1U);
        tecmo_asset_pack_store_u16(jumbotron + 2U, 1U);
        tecmo_asset_pack_store_u32(jumbotron + 4U, 0U);
        tecmo_asset_pack_store_u32(jumbotron + 8U, TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT);
        tecmo_asset_pack_store_u16(jumbotron + 12U, 0U);
        tecmo_asset_pack_store_u16(jumbotron + 14U, 0U);
        tecmo_asset_pack_store_u16(jumbotron + 16U, 0U);
        tecmo_asset_pack_store_u16(jumbotron + 18U, 2U);

        tecmo_asset_pack_store_u16(goal + 0U, 2U);
        tecmo_asset_pack_store_u16(goal + 2U, 0U);
        tecmo_asset_pack_store_u32(goal + 4U, TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT);
        tecmo_asset_pack_store_u32(goal + 8U, TECMO_ASSET_PACK_ARENA_GOAL_COUNT);
        tecmo_asset_pack_store_u16(goal + 12U, 165U);
        tecmo_asset_pack_store_u16(goal + 14U, 350U);
        tecmo_asset_pack_store_u16(goal + 16U, 0U);
        tecmo_asset_pack_store_u16(goal + 18U, 2U);
    }

    for (size_t selector = 0U; selector < 2U; ++selector) {
        const uint8_t *records = rom + (size_t)stream_offset[selector] + 1U;

        for (size_t index = 0U; index < expected_counts[selector]; ++index) {
            const uint8_t *record = records + index * 4U;
            uint8_t *piece = payload + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET +
                             output_piece * TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE;
            uint8_t top_tile = (uint8_t)(((uint8_t)(record[1] + 1U)) & 0xFEU);
            uint8_t attributes = record[2];
            uint32_t chr_byte_offset = TECMO_ASSET_PACK_ARENA_SPRITE_CHR_BASE +
                                       (uint32_t)top_tile * 16U;
            int16_t dx;
            int16_t dy;
            int16_t connector_overlay_y_adjust = 0;
            uint8_t flags = 0U;
            int is_goal_connector_record =
                selector == 1U &&
                record[0] == TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_Y &&
                record[3] == TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_X;

            if (is_goal_connector_record &&
                (record[1] != TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_TILE ||
                 attributes != TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_ATTRIBUTES ||
                 chr_byte_offset != TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_TOP_CHR_OFFSET)) {
                tecmo_asset_pack_set_messagef(
                    message,
                    message_size,
                    "Arena goal connector source-contract mismatch at record %u: "
                    "tile $%02X, attributes $%02X, top CHR offset %u; "
                    "expected $%02X/$%02X/%u.",
                    (unsigned int)index,
                    (unsigned int)record[1],
                    (unsigned int)attributes,
                    (unsigned int)chr_byte_offset,
                    TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_TILE,
                    TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_ATTRIBUTES,
                    TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_TOP_CHR_OFFSET);
                return -1;
            }
            if ((attributes & 0x1CU) != 0U) {
                tecmo_asset_pack_set_messagef(message,
                             message_size,
                             "Arena sprite selector %u record %u has unsupported attributes $%02X.",
                             (unsigned int)selector,
                             (unsigned int)index,
                             (unsigned int)attributes);
                return -1;
            }
            if (chr_byte_offset < TECMO_ASSET_PACK_ARENA_SPRITE_CHR_BASE ||
                (uint64_t)chr_byte_offset + 32U >
                    TECMO_ASSET_PACK_ARENA_SPRITE_CHR_LIMIT) {
                tecmo_asset_pack_set_messagef(message,
                             message_size,
                             "Arena sprite selector %u record %u references CHR outside pages 8/9.",
                             (unsigned int)selector,
                             (unsigned int)index);
                return -1;
            }

            if (selector == 0U) {
                dx = (int16_t)record[3];
                dy = (int16_t)record[0];
            } else {
                int16_t relative_x = (int16_t)(int8_t)record[3];
                uint8_t final_x = (uint8_t)(0xB8 + relative_x);

                dx = (int16_t)((int16_t)final_x - 165);
                switch (record[0]) {
                    case 0xC0U: dy = 0; break;
                    case 0xD0U: dy = 16; break;
                    case 0xE0U: dy = 32; break;
                    case 0xF0U: dy = 48; break;
                    default:
                        tecmo_asset_pack_set_messagef(message,
                                     message_size,
                                     "Arena goal record %u has invalid relative Y $%02X.",
                                     (unsigned int)index,
                                     (unsigned int)record[0]);
                        return -1;
                }
                if (is_goal_connector_record) {
                    connector_overlay_y_adjust =
                        TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_OVERLAY_Y_ADJUST;
                    ++connector_overlay_piece_count;
                }
            }

            if ((attributes & 0x40U) != 0U) flags |= 0x01U;
            if ((attributes & 0x80U) != 0U) flags |= 0x02U;
            if ((attributes & 0x20U) != 0U) flags |= 0x04U;
            tecmo_asset_pack_store_u16(piece + 0U, (uint16_t)dx);
            tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)dy);
            tecmo_asset_pack_store_u32(piece + 4U, chr_byte_offset);
            piece[8] = (uint8_t)(attributes & 0x03U);
            piece[9] = flags;
            tecmo_asset_pack_store_u16(piece + 10U, (uint16_t)connector_overlay_y_adjust);
            ++output_piece;
        }
    }
    if (output_piece != TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_COUNT) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite piece count mismatch.");
        return -1;
    }
    if (connector_overlay_piece_count != 1U) {
        tecmo_asset_pack_set_message(message, message_size, "Arena sprite connector-overlay record count mismatch.");
        return -1;
    }

    provenance->palette_source_offset = palette_offset;
    provenance->pointer_table_source_offset = pointer_table_offset;
    provenance->seeds_source_offset = seeds_offset;
    provenance->emitter_source_offset = bank04_offset +
                                        (TECMO_ASSET_PACK_ARENA_SPRITE_EMIT_CPU -
                                         TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    provenance->params_source_offset = params_offset;
    provenance->chr_page_source_offset[0] = chr_offset +
                                            (uint64_t)TECMO_ASSET_PACK_ARENA_SPRITE_CHR_R2 * 1024U;
    provenance->chr_page_source_offset[1] = chr_offset +
                                            (uint64_t)TECMO_ASSET_PACK_ARENA_SPRITE_CHR_R3 * 1024U;
    for (size_t selector = 0U; selector < 2U; ++selector) {
        provenance->stream_cpu[selector] = stream_cpu[selector];
        provenance->stream_source_offset[selector] = stream_offset[selector];
        provenance->stream_size[selector] = stream_size[selector];
    }
    return 0;
}

typedef struct TecmoRomByteContract {
    uint16_t cpu;
    uint8_t value;
} TecmoRomByteContract;

static const TecmoRomByteContract POST_ARENA_BANK04_CONTRACT[] = {
    {0x82D1U,0x83U},{0x82D2U,0x84U},{0x82D3U,0xD0U},{0x82D4U,0x86U},
    {0x82D5U,0x5EU},{0x82D6U,0x86U},{0x8483U,0xA9U},{0x8484U,0x1EU},
    {0x849BU,0xC9U},{0x849CU,0x0CU},{0x84D0U,0xA9U},{0x84D1U,0x02U},
    {0x86D0U,0xA9U},{0x86D1U,0x1AU},{0x86F1U,0x29U},{0x86F2U,0x3FU},
    {0x86F3U,0x18U},{0x86F4U,0x69U},{0x86F5U,0x82U},{0x872FU,0xC9U},
    {0x8730U,0x19U},{0x8744U,0xA5U},{0x8745U,0x8AU},{0x8746U,0xC9U},
    {0x8747U,0xF6U},{0x875AU,0xC9U},{0x875BU,0xF9U},{0x8788U,0x20U},
    {0x878BU,0x20U},{0x8792U,0x4CU},{0x8793U,0x1DU},{0x8794U,0x87U},
    {0x83A1U,0xA9U},{0x83A2U,0x1FU},{0x83A6U,0xA9U},{0x83A7U,0x06U},
    {0x83A8U,0x8DU},{0x83A9U,0x00U},{0x83AAU,0x01U},
    {0x865EU,0xA9U},{0x865FU,0x1BU},{0x8660U,0x20U},{0x8661U,0xF2U},
    {0x8662U,0x82U},{0x8663U,0xA9U},{0x8664U,0x00U},{0x8668U,0xA9U},
    {0x8669U,0xD7U},{0x8676U,0xA9U},{0x8677U,0x02U},{0x867BU,0xA5U},
    {0x867CU,0x88U},{0x867DU,0xC9U},{0x867EU,0x14U},{0x86A4U,0xE6U},
    {0x86A5U,0x88U},{0x86A6U,0xE6U},{0x86A7U,0x8AU},{0x86A8U,0xD0U},
    {0x86A9U,0xCCU}
};

static const TecmoRomByteContract POST_ARENA_FIXED_CONTRACT[] = {
    {0xFD84U,0x48U},{0xFD85U,0x8AU},{0xFD86U,0x48U},
    {0xFD99U,0xAEU},{0xFD9AU,0x04U},{0xFD9BU,0x03U},
    {0xFD9CU,0xBDU},{0xFD9DU,0xD9U},{0xFD9EU,0xFDU},
    {0xFDB7U,0xA9U},{0xFDB8U,0xFAU},{0xFDB9U,0x8DU},{0xFDBAU,0x01U},
    {0xFDBBU,0x80U},{0xFDD7U,0x01U},{0xFDD8U,0x00U},{0xFDD9U,0xA8U},
    {0xFDDAU,0x00U},{0xFDDBU,0x01U},{0xFDDCU,0x00U},
    {0xFD7CU,0x14U},{0xFD7DU,0x5EU},{0xFD7EU,0x14U},
    {0xFD80U,0x16U},{0xFD81U,0x60U},{0xFD82U,0x16U}
};

static int validate_post_arena_route_contract(const uint8_t *rom,
                                              uint64_t rom_size,
                                              uint64_t prg_offset,
                                              uint32_t prg_banks,
                                              char *message,
                                              size_t message_size)
{
    uint64_t bank04_offset;
    uint64_t fixed_offset;

    if (prg_banks <= TECMO_ASSET_PACK_ARENA_BANK04) return -1;
    bank04_offset = prg_offset +
                    (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    fixed_offset = prg_offset +
                   (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    for (size_t i = 0U; i < sizeof(POST_ARENA_BANK04_CONTRACT) / sizeof(POST_ARENA_BANK04_CONTRACT[0]); ++i) {
        uint64_t offset = bank04_offset + (POST_ARENA_BANK04_CONTRACT[i].cpu - 0x8000U);
        if (offset >= rom_size || rom[(size_t)offset] != POST_ARENA_BANK04_CONTRACT[i].value) {
            tecmo_asset_pack_set_message(message, message_size, "Post-arena Bank04 route contract is not Rev1.");
            return -1;
        }
    }
    for (size_t i = 0U; i < sizeof(POST_ARENA_FIXED_CONTRACT) / sizeof(POST_ARENA_FIXED_CONTRACT[0]); ++i) {
        uint64_t offset = fixed_offset + (POST_ARENA_FIXED_CONTRACT[i].cpu - 0xC000U);
        if (offset >= rom_size || rom[(size_t)offset] != POST_ARENA_FIXED_CONTRACT[i].value) {
            tecmo_asset_pack_set_message(message, message_size, "Post-arena IRQ route contract is not Rev1.");
            return -1;
        }
    }
    return 0;
}

static int build_ready_screen(const uint8_t *rom,
                              uint64_t rom_size,
                              uint64_t prg_offset,
                              uint32_t prg_banks,
                              uint64_t chr_size,
                              uint8_t payload[TECMO_ASSET_PACK_READY_SIZE],
                              TecmoPostArenaProvenance *provenance,
                              char *message,
                              size_t message_size)
{
    static const uint8_t expected_script[TECMO_ASSET_PACK_READY_SCRIPT_SIZE] = {
        0x00,0x00,0x00,0x00, 0x08,0x00,0x00,0x00,
        0x0C,0x02,0x00,0x00, 0x08,0x0B,0x00,0x00,
        0x04,0x0E,0x02,0x00, 0x00,0x09,0x0B,0x00,
        0x00,0x04,0x0E,0x02, 0x00,0x00,0x09,0x0B,
        0x00,0x00,0x04,0x0E, 0x00,0x00,0x00,0x09,
        0x00,0x00,0x00,0x04, 0x00,0x00,0x00,0x00
    };
    uint8_t decoded[1024];
    uint32_t fixed_bank = prg_banks - 1U;
    uint64_t fixed_offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank04_offset;
    uint64_t descriptor_offset;
    const uint8_t *descriptor;
    uint32_t palette_cpu;
    uint32_t stream_cpu;
    uint32_t stream_bank;
    uint64_t stream_bank_offset;
    uint64_t palette_offset;
    uint64_t script_offset;
    uint8_t r0;
    uint8_t r1;
    size_t encoded_size = 0U;

    if (prg_banks <= TECMO_ASSET_PACK_ARENA_BANK04 || chr_size == 0U) {
        tecmo_asset_pack_set_message(message, message_size, "READY import requires Bank04 and CHR ROM.");
        return -1;
    }
    if (validate_post_arena_route_contract(rom,
                                           rom_size,
                                           prg_offset,
                                           prg_banks,
                                           message,
                                           message_size) != 0) return -1;
    bank04_offset = prg_offset +
                    (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset +
                        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                        TECMO_ASSET_PACK_READY_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    script_offset = bank04_offset +
                    (TECMO_ASSET_PACK_READY_SCRIPT_CPU - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    if (descriptor_offset > rom_size || 7U > rom_size - descriptor_offset ||
        script_offset > rom_size || sizeof(expected_script) > rom_size - script_offset ||
        memcmp(rom + (size_t)script_offset, expected_script, sizeof(expected_script)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "READY descriptor or reveal script does not match Rev1.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    palette_cpu = tecmo_asset_pack_read_u16(descriptor + 2U);
    stream_cpu = tecmo_asset_pack_read_u16(descriptor + 4U);
    stream_bank = descriptor[6U];
    if (stream_bank >= prg_banks || palette_cpu < 0x8000U || palette_cpu >= 0xC000U ||
        stream_cpu < 0x8000U || stream_cpu >= 0xC000U ||
        descriptor[0] > 0x7FU || descriptor[1] > 0x7FU) {
        tecmo_asset_pack_set_message(message, message_size, "READY screen descriptor is invalid.");
        return -1;
    }
    r0 = (uint8_t)(descriptor[0] * 2U);
    r1 = (uint8_t)(descriptor[1] * 2U);
    if (tecmo_asset_pack_validate_chr_pair(r0, r1, chr_size, "READY", message, message_size) != 0) {
        return -1;
    }
    stream_bank_offset = prg_offset + (uint64_t)stream_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    palette_offset = stream_bank_offset + (palette_cpu - 0x8000U);
    if (palette_offset > rom_size || 16U > rom_size - palette_offset ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)stream_bank_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           stream_cpu - 0x8000U,
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0 || encoded_size != 53U) {
        tecmo_asset_pack_set_message(message, message_size, "READY compressed screen is not the Rev1 53-byte stream.");
        return -1;
    }

    memset(payload, 0, TECMO_ASSET_PACK_READY_SIZE);
    memcpy(payload, "TRDY", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_READY_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_READY_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 14U, 5U);
    tecmo_asset_pack_store_u16(payload + 16U, 12U);
    tecmo_asset_pack_store_u16(payload + 18U, 8U);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_READY_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_READY_PALETTE_FRAMES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_READY_MASKS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_READY_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_READY_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 40U, 58U);
    tecmo_asset_pack_store_u16(payload + 44U, 8U);
    tecmo_asset_pack_store_u16(payload + 46U, 16U);
    tecmo_asset_pack_store_u16(payload + 48U, 16U);
    tecmo_asset_pack_store_u16(payload + 50U, 4U);

    memset(payload + TECMO_ASSET_PACK_READY_PALETTES_OFFSET, 0x0F, 16U);
    for (size_t i = 0U; i < 16U; ++i) {
        uint8_t color = rom[(size_t)palette_offset + i];
        payload[TECMO_ASSET_PACK_READY_PALETTES_OFFSET + 16U + i] =
            tecmo_asset_pack_imported_fade_color(color, 0x20U);
        payload[TECMO_ASSET_PACK_READY_PALETTES_OFFSET + 32U + i] =
            tecmo_asset_pack_imported_fade_color(color, 0x10U);
        payload[TECMO_ASSET_PACK_READY_PALETTES_OFFSET + 48U + i] = color;
        payload[TECMO_ASSET_PACK_READY_PALETTES_OFFSET + 64U + i] = color;
    }
    {
        static const uint8_t frames[5] = {0U, 10U, 14U, 18U, 22U};
        memcpy(payload + TECMO_ASSET_PACK_READY_PALETTE_FRAMES_OFFSET, frames, sizeof(frames));
    }
    for (size_t record = 0U; record < 12U; ++record) {
        for (size_t byte = 0U; byte < 4U; ++byte) {
            uint8_t packed = expected_script[record * 4U + byte];
            payload[TECMO_ASSET_PACK_READY_MASKS_OFFSET + record * 8U + byte * 2U] =
                packed & 3U;
            payload[TECMO_ASSET_PACK_READY_MASKS_OFFSET + record * 8U + byte * 2U + 1U] =
                (packed >> 2U) & 3U;
        }
    }
    for (size_t i = 0U; i < TECMO_ASSET_PACK_READY_CELL_COUNT; ++i) {
        unsigned row = (unsigned)(i / 32U);
        unsigned col = (unsigned)(i % 32U);
        uint8_t tile = decoded[i];
        uint8_t selector = tile < 128U ? r0 : r1;
        uint32_t chr_offset = (uint32_t)selector * 1024U + (uint32_t)(tile & 0x7FU) * 16U;
        uint8_t *cell = payload + TECMO_ASSET_PACK_READY_CELLS_OFFSET +
                        i * TECMO_ASSET_PACK_READY_CELL_STRIDE;
        if ((uint64_t)chr_offset + 16U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "READY tile resolves outside chr/all.");
            return -1;
        }
        cell[0] = tile;
        cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
        tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
    }

    provenance->ready_route_offset = bank04_offset +
                                     (TECMO_ASSET_PACK_READY_ROUTE_CPU - 0x8000U);
    provenance->ready_descriptor_offset = descriptor_offset;
    provenance->ready_stream_offset = stream_bank_offset + (stream_cpu - 0x8000U);
    provenance->ready_stream_size = encoded_size;
    provenance->ready_palette_offset = palette_offset;
    provenance->ready_script_offset = script_offset;
    return 0;
}

static int build_warriors_transition(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint32_t prg_banks,
                                     uint64_t chr_size,
                                     uint8_t payload[TECMO_ASSET_PACK_WARRIORS_SIZE],
                                     TecmoPostArenaProvenance *provenance,
                                     char *message,
                                     size_t message_size)
{
    static const uint8_t sprite_selectors[4] = {0x91U, 0x93U, 0x95U, 0x97U};
    static const uint16_t wordmark_glyph_cpu[TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT] = {
        0xAF55U, 0xAF05U, 0xAF41U, 0xAF41U,
        0xAF21U, 0xAF39U, 0xAF41U, 0xAF45U
    };
    static const uint32_t patch_cpu[2] = {
        TECMO_ASSET_PACK_WARRIORS_PATCH0_CPU,
        TECMO_ASSET_PACK_WARRIORS_PATCH1_CPU
    };
    uint8_t decoded[2048];
    uint32_t fixed_bank = prg_banks - 1U;
    uint64_t fixed_offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank04_offset;
    uint64_t descriptor_offset;
    const uint8_t *descriptor;
    uint32_t stream_bank;
    uint32_t stream_cpu;
    uint32_t palette_cpu;
    uint64_t stream_bank_offset;
    uint64_t palette_offset;
    uint64_t sprite_palette_offset;
    uint64_t pointer_offset;
    uint64_t piece_stream_offset;
    uint64_t wordmark_bank_offset;
    uint8_t r0;
    uint8_t r1;
    size_t encoded_size = 0U;

    if (prg_banks <= TECMO_ASSET_PACK_WARRIORS_WORDMARK_BANK || chr_size == 0U) {
        tecmo_asset_pack_set_message(message, message_size, "WARRIORS import requires Bank04 and CHR ROM.");
        return -1;
    }
    if (validate_post_arena_route_contract(rom,
                                           rom_size,
                                           prg_offset,
                                           prg_banks,
                                           message,
                                           message_size) != 0) return -1;
    bank04_offset = prg_offset +
                    (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset +
                        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                        TECMO_ASSET_PACK_WARRIORS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    if (descriptor_offset > rom_size || 7U > rom_size - descriptor_offset) {
        tecmo_asset_pack_set_message(message, message_size, "WARRIORS descriptor is outside fixed PRG.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    palette_cpu = tecmo_asset_pack_read_u16(descriptor + 2U);
    stream_cpu = tecmo_asset_pack_read_u16(descriptor + 4U);
    stream_bank = descriptor[6U];
    if (stream_bank >= prg_banks || descriptor[0] > 0x7FU || descriptor[1] > 0x7FU ||
        palette_cpu < 0x8000U || palette_cpu >= 0xC000U ||
        stream_cpu < 0x8000U || stream_cpu >= 0xC000U) {
        tecmo_asset_pack_set_message(message, message_size, "WARRIORS descriptor is invalid.");
        return -1;
    }
    r0 = (uint8_t)(descriptor[0] * 2U);
    r1 = (uint8_t)(descriptor[1] * 2U);
    if (tecmo_asset_pack_validate_chr_pair(r0, r1, chr_size, "WARRIORS moving", message, message_size) != 0 ||
        tecmo_asset_pack_validate_chr_pair(r0,
                          TECMO_ASSET_PACK_WARRIORS_BG_R1_LOWER,
                          chr_size,
                          "WARRIORS lower",
                          message,
                          message_size) != 0) {
        return -1;
    }
    for (size_t i = 0U; i < 4U; ++i) {
        if ((uint64_t)(sprite_selectors[i] + 1U) * 1024U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "WARRIORS sprite selector resolves outside chr/all.");
            return -1;
        }
    }
    stream_bank_offset = prg_offset + (uint64_t)stream_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    palette_offset = stream_bank_offset + (palette_cpu - 0x8000U);
    memset(decoded, 0xFF, sizeof(decoded));
    if (palette_offset > rom_size || 16U > rom_size - palette_offset ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)stream_bank_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           stream_cpu - 0x8000U,
                           decoded,
                           TECMO_ASSET_PACK_WARRIORS_DECODED_SIZE,
                           &encoded_size,
                           message,
                           message_size) != 0 || encoded_size != 279U) {
        tecmo_asset_pack_set_message(message, message_size, "WARRIORS compressed screen is not the Rev1 279-byte stream.");
        return -1;
    }
    sprite_palette_offset = bank04_offset +
                            (TECMO_ASSET_PACK_WARRIORS_SPRITE_PALETTE_CPU - 0x8000U);
    pointer_offset = prg_offset + (TECMO_ASSET_PACK_WARRIORS_POINTER_CPU - 0x8000U);
    piece_stream_offset = prg_offset + (TECMO_ASSET_PACK_WARRIORS_STREAM_CPU - 0x8000U);
    wordmark_bank_offset = prg_offset +
                           (uint64_t)TECMO_ASSET_PACK_WARRIORS_WORDMARK_BANK *
                               TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (sprite_palette_offset > rom_size || 16U > rom_size - sprite_palette_offset ||
        pointer_offset > rom_size || 2U > rom_size - pointer_offset ||
        tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset) != TECMO_ASSET_PACK_WARRIORS_STREAM_CPU ||
        piece_stream_offset > rom_size || 1U + 46U * 4U > rom_size - piece_stream_offset ||
        rom[(size_t)piece_stream_offset] != 46U) {
        tecmo_asset_pack_set_message(message, message_size, "WARRIORS player sprite pointer/stream contract is invalid.");
        return -1;
    }
    for (size_t patch = 0U; patch < 2U; ++patch) {
        uint64_t offset = stream_bank_offset + (patch_cpu[patch] - 0x8000U);
        if (offset > rom_size || 64U > rom_size - offset) {
            tecmo_asset_pack_set_message(message, message_size, "WARRIORS patch crosses Bank01.");
            return -1;
        }
        provenance->warriors_patch_offset[patch] = offset;
    }

    memset(payload, 0, TECMO_ASSET_PACK_WARRIORS_SIZE);
    memcpy(payload, "TWAR", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_WARRIORS_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 2U);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_WARRIORS_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 16U, 46U);
    tecmo_asset_pack_store_u16(payload + 18U, TECMO_ASSET_PACK_WARRIORS_PIECE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 20U, 2U);
    tecmo_asset_pack_store_u16(payload + 22U, 64U);
    tecmo_asset_pack_store_u16(payload + 24U, 0xA8U);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_WARRIORS_BG_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_WARRIORS_SPRITE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_WARRIORS_PAGES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U, TECMO_ASSET_PACK_WARRIORS_PIECES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_WARRIORS_PATCHES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 48U, 214U);
    payload[52U] = 0x1BU;
    payload[53U] = 4U;
    payload[54U] = 7U;
    payload[55U] = 11U;
    payload[56U] = 15U;
    payload[57U] = 17U;
    payload[58U] = 26U;
    payload[59U] = 74U;
    payload[60U] = 193U;
    payload[61U] = 200U;
    payload[62U] = 213U;
    payload[63U] = 0xA3U;
    tecmo_asset_pack_store_u32(payload + 64U, TECMO_ASSET_PACK_WARRIORS_WORDMARK_OFFSET);
    tecmo_asset_pack_store_u16(payload + 68U, TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT);
    tecmo_asset_pack_store_u16(payload + 70U, 4U);
    tecmo_asset_pack_store_u16(payload + 72U, 8U);
    tecmo_asset_pack_store_u16(payload + 74U, 26U);
    memcpy(payload + TECMO_ASSET_PACK_WARRIORS_BG_PALETTE_OFFSET,
           rom + (size_t)palette_offset,
           16U);
    for (size_t palette = 0U; palette < 4U; ++palette) {
        const uint8_t *source = rom + (size_t)sprite_palette_offset + palette * 4U;
        uint8_t *dest = payload + TECMO_ASSET_PACK_WARRIORS_SPRITE_PALETTE_OFFSET +
                        palette * 4U;
        if (source[0] != 2U) {
            tecmo_asset_pack_set_message(message, message_size, "WARRIORS sprite palette control is not Rev1.");
            return -1;
        }
        dest[0] = 0x0FU;
        memcpy(dest + 1U, source + 1U, 3U);
    }

    for (size_t page = 0U; page < 2U; ++page) {
        const uint8_t *decoded_page = decoded + page * 1024U;
        for (size_t i = 0U; i < 960U; ++i) {
            unsigned row = (unsigned)(i / 32U);
            unsigned col = (unsigned)(i % 32U);
            uint8_t tile = decoded_page[i];
            uint8_t palette_index = page == 0U
                                        ? tecmo_asset_pack_decoded_palette_index(decoded_page, row, col)
                                        : 3U;
            uint32_t moving = tecmo_asset_pack_bg_chr_offset(tile, r0, r1);
            uint32_t lower = tecmo_asset_pack_bg_chr_offset(tile,
                                                     r0,
                                                     TECMO_ASSET_PACK_WARRIORS_BG_R1_LOWER);
            uint8_t *cell = payload + TECMO_ASSET_PACK_WARRIORS_PAGES_OFFSET +
                            (page * 960U + i) * TECMO_ASSET_PACK_WARRIORS_CELL_STRIDE;
            if ((uint64_t)moving + 16U > chr_size || (uint64_t)lower + 16U > chr_size) {
                tecmo_asset_pack_set_message(message, message_size, "WARRIORS background tile resolves outside chr/all.");
                return -1;
            }
            cell[0] = tile;
            cell[1] = palette_index;
            tecmo_asset_pack_store_u32(cell + 2U, moving);
            tecmo_asset_pack_store_u32(cell + 6U, lower);
        }
    }
    for (size_t i = 0U; i < 46U; ++i) {
        const uint8_t *record = rom + (size_t)piece_stream_offset + 1U + i * 4U;
        uint8_t top_tile = (uint8_t)((record[1] + 1U) & 0xFEU);
        unsigned slot = top_tile / 64U;
        uint32_t top_offset = (uint32_t)sprite_selectors[slot] * 1024U +
                              (uint32_t)(top_tile & 0x3FU) * 16U;
        uint8_t *piece = payload + TECMO_ASSET_PACK_WARRIORS_PIECES_OFFSET +
                         i * TECMO_ASSET_PACK_WARRIORS_PIECE_STRIDE;
        uint8_t attributes = record[2];
        uint8_t flags = 0U;
        if ((attributes & 0x3CU) != 0U || (uint64_t)top_offset + 32U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "WARRIORS player sprite piece has invalid attributes or CHR.");
            return -1;
        }
        if ((attributes & 0x40U) != 0U) flags |= 1U;
        if ((attributes & 0x80U) != 0U) flags |= 2U;
        tecmo_asset_pack_store_u16(piece + 0U, (uint16_t)(int16_t)(int8_t)record[3]);
        tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)(int16_t)(int8_t)record[0]);
        tecmo_asset_pack_store_u32(piece + 4U, top_offset);
        tecmo_asset_pack_store_u32(piece + 8U, top_offset + 16U);
        piece[12U] = attributes & 3U;
        piece[13U] = flags;
    }
    for (size_t patch = 0U; patch < 2U; ++patch) {
        const uint8_t *source = rom + (size_t)provenance->warriors_patch_offset[patch];
        for (size_t i = 0U; i < 64U; ++i) {
            unsigned row = 10U + (unsigned)(i / 8U);
            unsigned col = 12U + (unsigned)(i % 8U);
            uint8_t tile = source[i];
            uint32_t chr_offset = tecmo_asset_pack_bg_chr_offset(tile, r0, r1);
            uint8_t *cell = payload + TECMO_ASSET_PACK_WARRIORS_PATCHES_OFFSET +
                            (patch * 64U + i) * TECMO_ASSET_PACK_WARRIORS_PATCH_STRIDE;
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
        }
    }
    for (size_t glyph = 0U; glyph < TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT; ++glyph) {
        uint64_t glyph_offset = wordmark_bank_offset +
                                (uint64_t)(wordmark_glyph_cpu[glyph] - 0x8000U);
        if (glyph_offset > rom_size || 4U > rom_size - glyph_offset) {
            tecmo_asset_pack_set_message(message, message_size, "WARRIORS wordmark glyph crosses Bank06.");
            return -1;
        }
        provenance->warriors_wordmark_glyph_offset[glyph] = glyph_offset;
        for (size_t tile_index = 0U; tile_index < 4U; ++tile_index) {
            unsigned local_row = (unsigned)(tile_index / 2U);
            unsigned col = 8U + (unsigned)glyph * 2U + (unsigned)(tile_index % 2U);
            unsigned row = 26U + local_row;
            uint8_t tile = rom[(size_t)glyph_offset + tile_index];
            uint32_t chr_offset = tecmo_asset_pack_bg_chr_offset(
                tile,
                r0,
                TECMO_ASSET_PACK_WARRIORS_BG_R1_LOWER);
            uint8_t *cell = payload + TECMO_ASSET_PACK_WARRIORS_WORDMARK_OFFSET +
                            (glyph * 4U + tile_index) *
                                TECMO_ASSET_PACK_WARRIORS_WORDMARK_STRIDE;
            if (tile < 0x80U || (uint64_t)chr_offset + 16U > chr_size) {
                tecmo_asset_pack_set_message(message, message_size, "WARRIORS wordmark glyph has invalid CHR data.");
                return -1;
            }
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
        }
    }

    provenance->warriors_route_offset = bank04_offset +
                                        (TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU - 0x8000U);
    provenance->warriors_descriptor_offset = descriptor_offset;
    provenance->warriors_stream_offset = stream_bank_offset + (stream_cpu - 0x8000U);
    provenance->warriors_stream_size = encoded_size;
    provenance->warriors_palette_offset = palette_offset;
    provenance->warriors_sprite_palette_offset = sprite_palette_offset;
    provenance->warriors_pointer_offset = pointer_offset;
    provenance->warriors_piece_stream_offset = piece_stream_offset;
    return 0;
}

static int build_clippers_transition(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint32_t prg_banks,
                                     uint64_t chr_size,
                                     uint8_t payload[TECMO_ASSET_PACK_CLIPPERS_SIZE],
                                     TecmoPostArenaProvenance *provenance,
                                     char *message,
                                     size_t message_size)
{
    static const uint8_t clippers_descriptor[7] = {
        0x16U,0x17U,0xC4U,0xB7U,0xEBU,0xB5U,0x01U
    };
    static const uint8_t clippers_text[8] = {'C','L','I','P','P','E','R','S'};
    uint8_t decoded[2048];
    uint64_t fixed_offset;
    uint64_t bank04_offset;
    uint64_t bank06_offset;
    uint64_t descriptor_offset;
    uint64_t stream_bank_offset;
    uint64_t palette_offset;
    const uint8_t *descriptor;
    uint32_t stream_bank;
    uint32_t stream_cpu;
    uint32_t palette_cpu;
    uint8_t base_r0;
    uint8_t base_r1;
    size_t encoded_size = 0U;

    if (prg_banks <= 6U || chr_size == 0U ||
        validate_post_arena_route_contract(rom,
                                           rom_size,
                                           prg_offset,
                                           prg_banks,
                                           message,
                                           message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "CLIPPERS import requires the Rev1 post-arena route.");
        return -1;
    }
    fixed_offset = prg_offset +
                   (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank04_offset = prg_offset +
                    (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset +
                        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                        TECMO_ASSET_PACK_CLIPPERS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    if (descriptor_offset > rom_size || 7U > rom_size - descriptor_offset) {
        tecmo_asset_pack_set_message(message, message_size, "CLIPPERS descriptor is outside fixed PRG.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    if (memcmp(descriptor, clippers_descriptor, sizeof(clippers_descriptor)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "CLIPPERS screen descriptor is not Rev1.");
        return -1;
    }
    palette_cpu = tecmo_asset_pack_read_u16(descriptor + 2U);
    stream_cpu = tecmo_asset_pack_read_u16(descriptor + 4U);
    stream_bank = descriptor[6U];
    base_r0 = (uint8_t)(descriptor[0] * 2U);
    base_r1 = (uint8_t)(descriptor[1] * 2U);
    if (stream_bank != 1U || palette_cpu != 0xB7C4U || stream_cpu != 0xB5EBU ||
        stream_cpu < 0x8000U || stream_cpu >= 0xC000U ||
        tecmo_asset_pack_validate_chr_pair(base_r0, base_r1, chr_size, "CLIPPERS base", message, message_size) != 0 ||
        tecmo_asset_pack_validate_chr_pair(TECMO_ASSET_PACK_CLIPPERS_LOWER_R0,
                          TECMO_ASSET_PACK_CLIPPERS_LOWER_R1,
                          chr_size,
                          "CLIPPERS lower",
                          message,
                          message_size) != 0) {
        return -1;
    }
    stream_bank_offset = prg_offset +
                         (uint64_t)stream_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    palette_offset = stream_bank_offset + (palette_cpu - 0x8000U);
    if (palette_offset > stream_bank_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES ||
        16U > stream_bank_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES - palette_offset ||
        palette_offset > rom_size || 16U > rom_size - palette_offset ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)stream_bank_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           stream_cpu - 0x8000U,
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0 || encoded_size != 474U) {
        tecmo_asset_pack_set_message(message, message_size, "CLIPPERS compressed screen is not the Rev1 474-byte stream.");
        return -1;
    }

    memset(payload, 0, TECMO_ASSET_PACK_CLIPPERS_SIZE);
    memcpy(payload, "TCLP", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_CLIPPERS_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 2U);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_CLIPPERS_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 16U, 4U);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_CLIPPERS_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_CLIPPERS_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_CLIPPERS_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 32U, 151U);
    tecmo_asset_pack_store_u16(payload + 36U, 0x883DU);
    tecmo_asset_pack_store_u16(payload + 38U, 200U);
    tecmo_asset_pack_store_u16(payload + 40U, 40U);
    tecmo_asset_pack_store_u16(payload + 42U, 2U);
    tecmo_asset_pack_store_u16(payload + 44U, 41U);
    tecmo_asset_pack_store_u16(payload + 46U, 20U);
    payload[48U] = base_r0;
    payload[49U] = base_r1;
    payload[50U] = TECMO_ASSET_PACK_CLIPPERS_LOWER_R0;
    payload[51U] = TECMO_ASSET_PACK_CLIPPERS_LOWER_R1;
    tecmo_asset_pack_store_u32(payload + 56U, TECMO_ASSET_PACK_CLIPPERS_WORDMARK_OFFSET);
    tecmo_asset_pack_store_u16(payload + 60U, TECMO_ASSET_PACK_CLIPPERS_WORDMARK_TILE_COUNT);
    tecmo_asset_pack_store_u16(payload + 62U, 32U);
    memset(payload + TECMO_ASSET_PACK_CLIPPERS_PALETTES_OFFSET, 0x0F, 16U);
    for (size_t i = 0U; i < 16U; ++i) {
        uint8_t color = rom[(size_t)palette_offset + i];
        payload[TECMO_ASSET_PACK_CLIPPERS_PALETTES_OFFSET + 16U + i] =
            tecmo_asset_pack_imported_fade_color(color, 0x20U);
        payload[TECMO_ASSET_PACK_CLIPPERS_PALETTES_OFFSET + 32U + i] =
            tecmo_asset_pack_imported_fade_color(color, 0x10U);
        payload[TECMO_ASSET_PACK_CLIPPERS_PALETTES_OFFSET + 48U + i] = color;
    }
    for (size_t page = 0U; page < 2U; ++page) {
        const uint8_t *decoded_page = decoded + page * 1024U;
        for (size_t i = 0U; i < 960U; ++i) {
            unsigned row = (unsigned)(i / 32U);
            unsigned col = (unsigned)(i % 32U);
            uint8_t tile = decoded_page[i];
            uint8_t *cell = payload + TECMO_ASSET_PACK_CLIPPERS_CELLS_OFFSET +
                            (page * 960U + i) * TECMO_ASSET_PACK_CLIPPERS_CELL_STRIDE;
            uint32_t base = tecmo_asset_pack_bg_chr_offset(tile, base_r0, base_r1);
            uint32_t lower = tecmo_asset_pack_bg_chr_offset(tile,
                                                    TECMO_ASSET_PACK_CLIPPERS_LOWER_R0,
                                                    TECMO_ASSET_PACK_CLIPPERS_LOWER_R1);
            if ((uint64_t)base + 16U > chr_size || (uint64_t)lower + 16U > chr_size) {
                tecmo_asset_pack_set_message(message, message_size, "CLIPPERS tile resolves outside chr/all.");
                return -1;
            }
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded_page, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, base);
            tecmo_asset_pack_store_u32(cell + 6U, lower);
        }
    }
    {
        uint64_t dispatch_bank = fixed_offset + (0xCAF5U - 0xC000U) + 0x37U;
        uint64_t dispatch_low = fixed_offset + (0xCB33U - 0xC000U) + 0x37U;
        uint64_t dispatch_high = fixed_offset + (0xCB71U - 0xC000U) + 0x37U;
        uint64_t string_pointer = bank06_offset + (0xAD60U - 0x8000U) + 0x16U;
        uint16_t string_cpu;
        uint64_t string_offset;

        if (dispatch_high >= rom_size || string_pointer + 1U >= rom_size ||
            rom[(size_t)dispatch_bank] != 6U ||
            rom[(size_t)dispatch_low] != 0xAEU ||
            rom[(size_t)dispatch_high] != 0x9EU ||
            memcmp(rom + (size_t)(bank06_offset + (0x9EAEU - 0x8000U)),
                   "\xBD\x60\xAD\x85\x16\xBD\x61\xAD\x85\x17",
                   10U) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "CLIPPERS text dispatch is not the Rev1 Bank06 path.");
            return -1;
        }
        string_cpu = tecmo_asset_pack_read_u16(rom + (size_t)string_pointer);
        string_offset = bank06_offset + (uint64_t)(string_cpu - 0x8000U);
        if (string_cpu != 0xACA3U ||
            string_offset + 9U > bank06_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES ||
            string_offset + 9U > rom_size || rom[(size_t)string_offset] != 8U ||
            memcmp(rom + (size_t)string_offset + 1U, clippers_text, sizeof(clippers_text)) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "CLIPPERS Bank06 text record is not Rev1.");
            return -1;
        }
        for (size_t glyph = 0U; glyph < sizeof(clippers_text); ++glyph) {
            uint64_t map_offset = bank06_offset + (0xA273U - 0x8000U) + clippers_text[glyph];
            uint8_t mapped;
            uint64_t glyph_offset;
            if (map_offset >= bank06_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES ||
                map_offset >= rom_size) return -1;
            mapped = rom[(size_t)map_offset];
            glyph_offset = bank06_offset + (0xAF05U - 0x8000U) + (uint64_t)mapped * 4U;
            if (glyph_offset + 4U > bank06_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES ||
                glyph_offset + 4U > rom_size) return -1;
            for (size_t tile_index = 0U; tile_index < 4U; ++tile_index) {
                unsigned row = 26U + (unsigned)(tile_index / 2U);
                unsigned col = 8U + (unsigned)glyph * 2U + (unsigned)(tile_index % 2U);
                uint8_t tile = rom[(size_t)glyph_offset + tile_index];
                uint8_t *cell = payload + TECMO_ASSET_PACK_CLIPPERS_WORDMARK_OFFSET +
                                (glyph * 4U + tile_index) * TECMO_ASSET_PACK_CLIPPERS_CELL_STRIDE;
                uint32_t base = tecmo_asset_pack_bg_chr_offset(tile, base_r0, base_r1);
                uint32_t lower = tecmo_asset_pack_bg_chr_offset(tile,
                                                        TECMO_ASSET_PACK_CLIPPERS_LOWER_R0,
                                                        TECMO_ASSET_PACK_CLIPPERS_LOWER_R1);
                if ((uint64_t)base + 16U > chr_size || (uint64_t)lower + 16U > chr_size) return -1;
                cell[0] = tile;
                cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
                tecmo_asset_pack_store_u32(cell + 2U, base);
                tecmo_asset_pack_store_u32(cell + 6U, lower);
            }
        }
        provenance->clippers_string_offset = string_offset;
        provenance->clippers_glyph_table_offset = bank06_offset + (0xAF05U - 0x8000U);
    }

    provenance->clippers_route_offset = bank04_offset +
                                         (TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU - 0x8000U);
    provenance->clippers_descriptor_offset = descriptor_offset;
    provenance->clippers_stream_offset = stream_bank_offset + (stream_cpu - 0x8000U);
    provenance->clippers_stream_size = encoded_size;
    provenance->clippers_palette_offset = palette_offset;
    provenance->clippers_irq_table_offset[0] = fixed_offset + (0xFD84U - 0xC000U);
    provenance->clippers_irq_table_offset[1] = fixed_offset + (0xFDD7U - 0xC000U);
    return 0;
}

static int build_bucks_transition(const uint8_t *rom,
                                  uint64_t rom_size,
                                  uint64_t prg_offset,
                                  uint32_t prg_banks,
                                  uint64_t chr_size,
                                  uint8_t payload[TECMO_ASSET_PACK_BUCKS_SIZE],
                                  TecmoPostArenaProvenance *provenance,
                                  char *message,
                                  size_t message_size)
{
    static const uint8_t expected_descriptor[7] = {
        0x2FU,0x30U,0xA5U,0xB4U,0x01U,0xB4U,0x01U
    };
    static const uint8_t expected_thresholds[6] = {0xEFU,0xC0U,0x90U,0x60U,0x30U,0x00U};
    static const uint8_t expected_route[] = {
        0xA9U,0x19U,0x20U,0xF2U,0x82U,0xA9U,0x0BU,0x20U,0x6EU,0x8AU
    };
    static const uint8_t bucks_text[5] = {'B','U','C','K','S'};
    uint8_t decoded[2048];
    uint64_t fixed_offset;
    uint64_t bank01_offset;
    uint64_t bank04_offset;
    uint64_t bank06_offset;
    uint64_t descriptor_offset;
    uint64_t stream_offset;
    uint64_t palette_offset;
    uint64_t threshold_offset;
    uint64_t color_offset;
    uint64_t string_pointer_offset;
    uint16_t string_cpu;
    uint64_t string_offset;
    size_t encoded_size = 0U;

    if (prg_banks <= 6U || chr_size == 0U ||
        validate_post_arena_route_contract(rom, rom_size, prg_offset, prg_banks,
                                           message, message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "BUCKS import requires the Rev1 post-arena route.");
        return -1;
    }
    fixed_offset = prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank01_offset = prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset + (0xDD34U - 0xC000U);
    stream_offset = bank01_offset + (0xB401U - 0x8000U);
    palette_offset = bank01_offset + (0xB4A5U - 0x8000U);
    threshold_offset = bank04_offset + (0x88A3U - 0x8000U);
    color_offset = fixed_offset + (0xDC19U - 0xC000U) + 0x0EU;
    string_pointer_offset = bank06_offset + (0xAD60U - 0x8000U) + 0x0EU * 2U;
    if (descriptor_offset + 7U > rom_size || stream_offset >= rom_size ||
        palette_offset + 16U > rom_size || threshold_offset + 6U > rom_size ||
        color_offset >= rom_size || string_pointer_offset + 2U > rom_size ||
        memcmp(rom + (size_t)descriptor_offset, expected_descriptor, 7U) != 0 ||
        memcmp(rom + (size_t)threshold_offset, expected_thresholds, 6U) != 0 ||
        memcmp(rom + (size_t)(bank04_offset + (TECMO_ASSET_PACK_BUCKS_ROUTE_CPU - 0x8000U)),
               expected_route, sizeof(expected_route)) != 0 ||
        rom[(size_t)color_offset] != 0x2AU) {
        tecmo_asset_pack_set_message(message, message_size, "BUCKS Rev1 route, descriptor, thresholds, or team color is invalid.");
        return -1;
    }
    if (tecmo_asset_pack_validate_chr_pair(0x5EU, 0x60U, chr_size, "BUCKS base", message, message_size) != 0 ||
        tecmo_asset_pack_validate_chr_pair(0x5EU, 0xFAU, chr_size, "BUCKS lower", message, message_size) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)bank01_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           0xB401U - 0x8000U,
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0) return -1;

    string_cpu = tecmo_asset_pack_read_u16(rom + (size_t)string_pointer_offset);
    string_offset = bank06_offset + (uint64_t)(string_cpu - 0x8000U);
    if (string_cpu != 0xACB8U || string_offset + 6U > rom_size ||
        rom[(size_t)string_offset] != 5U ||
        memcmp(rom + (size_t)string_offset + 1U, bucks_text, sizeof(bucks_text)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "BUCKS Bank06 team-name record is invalid.");
        return -1;
    }

    memset(payload, 0, TECMO_ASSET_PACK_BUCKS_SIZE);
    memcpy(payload, "TBUC", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_BUCKS_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 2U);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_BUCKS_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 16U, 4U);
    tecmo_asset_pack_store_u16(payload + 18U, 5U);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_BUCKS_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_BUCKS_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_BUCKS_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_BUCKS_WORDMARK_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, 83U);
    tecmo_asset_pack_store_u16(payload + 40U, 0x854FU);
    tecmo_asset_pack_store_u16(payload + 42U, 31U);
    tecmo_asset_pack_store_u16(payload + 44U, 168U);
    tecmo_asset_pack_store_u16(payload + 46U, 10U);
    tecmo_asset_pack_store_u16(payload + 48U, 14U);
    tecmo_asset_pack_store_u16(payload + 50U, 20U);
    payload[52U] = 0x5EU;
    payload[53U] = 0x60U;
    payload[54U] = 0x5EU;
    payload[55U] = 0xFAU;
    tecmo_asset_pack_store_u16(payload + 56U, 6U);
    tecmo_asset_pack_store_u16(payload + 58U, 20U);
    memcpy(payload + 64U, expected_thresholds, sizeof(expected_thresholds));
    for (size_t stage = 0U; stage < 4U; ++stage) {
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)palette_offset + i];
            if (i == 11U) color = rom[(size_t)color_offset];
            if (stage != 0U && i >= 4U) color = tecmo_asset_pack_imported_fade_color(color, (uint8_t)(stage * 0x10U));
            payload[TECMO_ASSET_PACK_BUCKS_PALETTES_OFFSET + stage * 16U + i] = color;
        }
    }
    for (size_t page = 0U; page < 2U; ++page) {
        const uint8_t *decoded_page = decoded + page * 1024U;
        for (size_t i = 0U; i < 960U; ++i) {
            unsigned row = (unsigned)(i / 32U);
            unsigned col = (unsigned)(i % 32U);
            uint8_t tile = decoded_page[i];
            uint8_t *cell = payload + TECMO_ASSET_PACK_BUCKS_CELLS_OFFSET +
                            (page * 960U + i) * TECMO_ASSET_PACK_BUCKS_CELL_STRIDE;
            uint32_t base = tecmo_asset_pack_bg_chr_offset(tile, 0x5EU, 0x60U);
            uint32_t lower = tecmo_asset_pack_bg_chr_offset(tile, 0x5EU, 0xFAU);
            if ((uint64_t)base + 16U > chr_size || (uint64_t)lower + 16U > chr_size) return -1;
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded_page, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, base);
            tecmo_asset_pack_store_u32(cell + 6U, lower);
        }
    }
    for (size_t glyph = 0U; glyph < sizeof(bucks_text); ++glyph) {
        uint64_t map_offset = bank06_offset + (0xA273U - 0x8000U) + bucks_text[glyph];
        uint8_t mapped;
        uint64_t glyph_offset;
        if (map_offset >= rom_size) return -1;
        mapped = rom[(size_t)map_offset];
        glyph_offset = bank06_offset + (0xAF05U - 0x8000U) + (uint64_t)mapped * 4U;
        if (glyph_offset + 4U > rom_size) return -1;
        for (size_t tile_index = 0U; tile_index < 4U; ++tile_index) {
            unsigned row = 26U + (unsigned)(tile_index / 2U);
            unsigned col = 12U + (unsigned)glyph * 2U + (unsigned)(tile_index % 2U);
            uint8_t tile = rom[(size_t)glyph_offset + tile_index];
            uint8_t *cell = payload + TECMO_ASSET_PACK_BUCKS_WORDMARK_OFFSET +
                            (glyph * 4U + tile_index) * TECMO_ASSET_PACK_BUCKS_CELL_STRIDE;
            uint32_t base = tecmo_asset_pack_bg_chr_offset(tile, 0x5EU, 0x60U);
            uint32_t lower = tecmo_asset_pack_bg_chr_offset(tile, 0x5EU, 0xFAU);
            if (tile < 0x80U || (uint64_t)lower + 16U > chr_size) return -1;
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, base);
            tecmo_asset_pack_store_u32(cell + 6U, lower);
        }
    }
    provenance->bucks_route_offset = bank04_offset + (TECMO_ASSET_PACK_BUCKS_ROUTE_CPU - 0x8000U);
    provenance->bucks_descriptor_offset = descriptor_offset;
    provenance->bucks_stream_offset = stream_offset;
    provenance->bucks_stream_size = encoded_size;
    provenance->bucks_palette_offset = palette_offset;
    provenance->bucks_threshold_offset = threshold_offset;
    provenance->bucks_string_offset = string_offset;
    provenance->bucks_glyph_table_offset = bank06_offset + (0xAF05U - 0x8000U);
    return 0;
}

static int build_pass_transition(const uint8_t *rom,
                                 uint64_t rom_size,
                                 uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 uint64_t chr_size,
                                 uint8_t payload[TECMO_ASSET_PACK_PASS_SIZE],
                                 TecmoPostArenaProvenance *provenance,
                                 char *message,
                                 size_t message_size)
{
    static const uint8_t expected_descriptor[7] = {
        0x78U,0x79U,0xA9U,0xBAU,0x33U,0xB9U,0x01U
    };
    static const uint8_t expected_route[] = {
        0xAEU,0xE9U,0x07U,0xBDU,0x3CU,0x86U,0x20U,0xF2U,0x82U,
        0xA9U,0x0DU,0x20U,0x6EU,0x8AU,0xA2U,0xFDU,0xA0U,0x89U
    };
    static const uint8_t expected_emit_helper[] = {
        0x85U,0x09U,0x84U,0x0BU,0xA9U,0x00U,0x85U,0x2DU,0x85U,0x2CU,
        0xA9U,0x01U,0x85U,0x0DU,0x85U,0x2EU,0xA9U,0x00U,0xA2U,0x0FU,
        0xA0U,0xA9U,0x4CU,0x51U,0xC0U
    };
    static const uint8_t sprite_selectors[3] = {0x91U,0x93U,0x95U};
    uint8_t decoded[2048];
    uint64_t fixed_offset;
    uint64_t bank01_offset;
    uint64_t bank04_offset;
    uint64_t descriptor_offset;
    uint64_t stream_offset;
    uint64_t palette_offset;
    uint64_t helper_offset;
    uint64_t special_offset;
    uint64_t pointer_offset;
    uint64_t piece_stream_offset;
    size_t encoded_size = 0U;

    if (prg_banks <= 4U || chr_size == 0U) return -1;
    fixed_offset = prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank01_offset = prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset + (0xDD50U - 0xC000U);
    stream_offset = bank01_offset + (0xB933U - 0x8000U);
    palette_offset = bank01_offset + (0xBAA9U - 0x8000U);
    helper_offset = bank04_offset + (TECMO_ASSET_PACK_PASS_HELPER_PALETTE_CPU - 0x8000U);
    special_offset = bank04_offset + (TECMO_ASSET_PACK_PASS_SPECIAL_PALETTE_CPU - 0x8000U);
    pointer_offset = prg_offset + (TECMO_ASSET_PACK_PASS_POINTER_CPU - 0x8000U);
    piece_stream_offset = prg_offset + (TECMO_ASSET_PACK_PASS_STREAM_CPU - 0x8000U);
    if (descriptor_offset + 7U > rom_size || palette_offset + 16U > rom_size ||
        helper_offset + 16U > rom_size || special_offset + 16U > rom_size ||
        pointer_offset + 4U > rom_size || piece_stream_offset + 41U > rom_size ||
        memcmp(rom + (size_t)descriptor_offset, expected_descriptor, 7U) != 0 ||
        memcmp(rom + (size_t)(bank04_offset + (TECMO_ASSET_PACK_PASS_ROUTE_CPU - 0x8000U)),
               expected_route, sizeof(expected_route)) != 0 ||
        memcmp(rom + (size_t)(bank04_offset + (0x8645U - 0x8000U)),
               expected_emit_helper, sizeof(expected_emit_helper)) != 0 ||
        tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset + 2U) != TECMO_ASSET_PACK_PASS_STREAM_CPU ||
        rom[(size_t)piece_stream_offset] != TECMO_ASSET_PACK_PASS_PIECE_COUNT) {
        tecmo_asset_pack_set_message(message, message_size, "PASS Rev1 route, descriptor, or player stream is invalid.");
        return -1;
    }
    for (size_t i = 0U; i < 4U; ++i) {
        if (rom[(size_t)helper_offset + i * 4U] != 0x02U) return -1;
    }
    if (tecmo_asset_pack_validate_chr_pair(0xF0U, 0xF2U, chr_size, "PASS background", message, message_size) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)bank01_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           0xB933U - 0x8000U,
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0) return -1;

    memset(payload, 0, TECMO_ASSET_PACK_PASS_SIZE);
    memcpy(payload, "TPAS", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_PASS_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 2U);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_PASS_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_PASS_PIECE_COUNT);
    tecmo_asset_pack_store_u16(payload + 18U, TECMO_ASSET_PACK_PASS_PIECE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 20U, 5U);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_PASS_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_PASS_SPRITE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_PASS_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_PASS_PIECES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U, 52U);
    tecmo_asset_pack_store_u16(payload + 44U, 0x851CU);
    tecmo_asset_pack_store_u16(payload + 46U, 18U);
    tecmo_asset_pack_store_u16(payload + 48U, 30U);
    payload[50U] = 0x68U;
    payload[51U] = 8U;
    payload[52U] = 0xF0U;
    payload[53U] = 0xF2U;
    payload[54U] = 0x91U;
    payload[55U] = 0x93U;
    payload[56U] = 0x95U;
    payload[57U] = 0x54U;
    tecmo_asset_pack_store_u16(payload + 58U, TECMO_ASSET_PACK_PASS_ROUTE_CPU);
    payload[60U] = TECMO_ASSET_PACK_PASS_SCREEN_ID;
    payload[64U] = 10U;
    payload[65U] = 14U;
    payload[66U] = 18U;
    payload[67U] = 22U;
    payload[68U] = 27U;
    payload[69U] = 28U;

    for (size_t stage = 0U; stage < 4U; ++stage) {
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)palette_offset + i];
            if (i == 4U || i == 8U || i == 12U) color = rom[(size_t)helper_offset + i];
            if (i == 13U) color = rom[(size_t)special_offset + 3U];
            if (i != 4U && i != 8U && i != 12U) color = tecmo_asset_pack_palette_brightness_cap(color, (uint8_t)stage);
            payload[TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + stage * 16U + i] = color;
        }
    }
    memcpy(payload + TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + 64U,
           rom + (size_t)palette_offset,
           16U);
    memcpy(payload + TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + 64U,
           rom + (size_t)special_offset,
           4U);
    payload[TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + 68U] = rom[(size_t)helper_offset + 4U];
    payload[TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + 72U] = rom[(size_t)helper_offset + 8U];
    payload[TECMO_ASSET_PACK_PASS_PALETTES_OFFSET + 76U] = rom[(size_t)helper_offset + 12U];
    memcpy(payload + TECMO_ASSET_PACK_PASS_SPRITE_PALETTE_OFFSET,
           rom + (size_t)helper_offset,
           16U);

    for (size_t page = 0U; page < 2U; ++page) {
        const uint8_t *decoded_page = decoded + page * 1024U;
        for (size_t i = 0U; i < 960U; ++i) {
            unsigned row = (unsigned)(i / 32U);
            unsigned col = (unsigned)(i % 32U);
            uint8_t tile = decoded_page[i];
            uint32_t chr_offset = tecmo_asset_pack_bg_chr_offset(tile, 0xF0U, 0xF2U);
            uint8_t *cell = payload + TECMO_ASSET_PACK_PASS_CELLS_OFFSET +
                            (page * 960U + i) * TECMO_ASSET_PACK_PASS_CELL_STRIDE;
            if ((uint64_t)chr_offset + 16U > chr_size) return -1;
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded_page, row, col);
            tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
        }
    }
    for (size_t i = 0U; i < TECMO_ASSET_PACK_PASS_PIECE_COUNT; ++i) {
        const uint8_t *record = rom + (size_t)piece_stream_offset + 1U + i * 4U;
        uint8_t top_tile = (uint8_t)((record[1] + 1U) & 0xFEU);
        unsigned slot = top_tile / 64U;
        uint32_t top_offset;
        uint8_t attributes = record[2];
        uint8_t flags = 0U;
        uint8_t *piece = payload + TECMO_ASSET_PACK_PASS_PIECES_OFFSET +
                         i * TECMO_ASSET_PACK_PASS_PIECE_STRIDE;
        if (slot >= 3U || (attributes & 0x3CU) != 0U) return -1;
        top_offset = (uint32_t)sprite_selectors[slot] * 1024U +
                     (uint32_t)(top_tile & 0x3FU) * 16U;
        if ((uint64_t)top_offset + 32U > chr_size) return -1;
        if ((attributes & 0x40U) != 0U) flags |= 1U;
        if ((attributes & 0x80U) != 0U) flags |= 2U;
        tecmo_asset_pack_store_u16(piece + 0U, (uint16_t)(int16_t)(int8_t)record[3]);
        tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)(int16_t)(int8_t)record[0]);
        tecmo_asset_pack_store_u32(piece + 4U, top_offset);
        tecmo_asset_pack_store_u32(piece + 8U, top_offset + 16U);
        piece[12U] = attributes & 3U;
        piece[13U] = flags;
    }
    provenance->pass_route_offset = bank04_offset + (TECMO_ASSET_PACK_PASS_ROUTE_CPU - 0x8000U);
    provenance->pass_descriptor_offset = descriptor_offset;
    provenance->pass_stream_offset = stream_offset;
    provenance->pass_stream_size = encoded_size;
    provenance->pass_palette_offset = palette_offset;
    provenance->pass_helper_palette_offset = helper_offset;
    provenance->pass_special_palette_offset = special_offset;
    provenance->pass_pointer_offset = pointer_offset + 2U;
    provenance->pass_piece_stream_offset = piece_stream_offset;
    return 0;
}

static int add_native_arena_intro_entries(TecmoAssetPackBuilder *builder,
                                          const uint8_t *rom,
                                          uint64_t rom_size,
                                          uint64_t prg_offset,
                                          uint32_t prg_banks,
                                          uint64_t chr_offset,
                                          uint64_t chr_size,
                                          int enforce_finale_revision_fingerprints,
                                          TecmoOpeningScreenProvenance opening_provenance[2],
                                          TecmoArenaBackgroundProvenance *background_provenance,
                                          TecmoArenaSpriteGroupsProvenance *sprite_groups_provenance,
                                          TecmoPostArenaProvenance *post_arena_provenance,
                                          TecmoFinaleProvenance *finale_provenance,
                                          char *message,
                                          size_t message_size)
{
    uint8_t opening_payload[TECMO_ASSET_PACK_PRESENTS_SIZE];
    char script_payload[2048];
    uint8_t background_payload[TECMO_ASSET_PACK_ARENA_LAYER_SIZE];
    char palette_payload[2048];
    uint8_t sprite_groups_payload[TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE];
    uint8_t ready_payload[TECMO_ASSET_PACK_READY_SIZE];
    uint8_t warriors_payload[TECMO_ASSET_PACK_WARRIORS_SIZE];
    uint8_t clippers_payload[TECMO_ASSET_PACK_CLIPPERS_SIZE];
    uint8_t bucks_payload[TECMO_ASSET_PACK_BUCKS_SIZE];
    uint8_t pass_payload[TECMO_ASSET_PACK_PASS_SIZE];
    uint8_t finale_payload[TECMO_ASSET_PACK_FINALE_SIZE];
    uint64_t script_source_offset =
        prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_ROUTE_CPU);
    uint64_t palette_source_offset =
        prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_PALETTE_CPU);
    uint32_t source_bank =
        prg_banks > TECMO_ASSET_PACK_ARENA_BANK04
            ? TECMO_ASSET_PACK_ARENA_BANK04
            : prg_banks - 1U;
    int bank04_available = prg_banks > TECMO_ASSET_PACK_ARENA_BANK04;
    int payload_length;
    TecmoAssetPackEntryInfo entry_info;

    if (tecmo_asset_pack_build_opening_screen(rom,
                             rom_size,
                             prg_offset,
                             prg_banks,
                             chr_size,
                             0U,
                             enforce_finale_revision_fingerprints,
                             opening_payload,
                             TECMO_ASSET_PACK_PRESENTS_SIZE,
                             &opening_provenance[0],
                             message,
                             message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_PRESENTS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_PRESENTS_STREAM_CPU,
                                 opening_provenance[0].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            opening_payload,
                                            TECMO_ASSET_PACK_PRESENTS_SIZE,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native TECMO presents entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_opening_screen(rom,
                             rom_size,
                             prg_offset,
                             prg_banks,
                             chr_size,
                             1U,
                             enforce_finale_revision_fingerprints,
                             opening_payload,
                             TECMO_ASSET_PACK_LICENSE_SIZE,
                             &opening_provenance[1],
                             message,
                             message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_LICENSE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_LICENSE_STREAM_CPU,
                                 opening_provenance[1].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            opening_payload,
                                            TECMO_ASSET_PACK_LICENSE_SIZE,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native NBA license entry.");
        return -1;
    }

    payload_length = snprintf(script_payload,
                              sizeof(script_payload),
                              "{\n"
                              "  \"format\":\"tecmo.arena-intro.script/1\",\n"
                              "  \"input_contract\":\"ines-only\",\n"
                              "  \"source_route\":\"bank04:C-0127\",\n"
                              "  \"source_bank_available\":%s,\n"
                              "  \"runtime_shape\":\"native-scene-script\",\n"
                              "  \"phases\":[\"enter\",\"pan_to_goal\",\"hold_goal\",\"handoff\"],\n"
                              "  \"camera\":{\"viewport\":[256,240],\"start\":[0,0],\"end\":[40,72],\"pan_frames\":96},\n"
                              "  \"timeline\":[\n"
                              "    {\"op\":\"set_phase\",\"phase\":\"enter\",\"frame\":0},\n"
                              "    {\"op\":\"move_camera\",\"phase\":\"pan_to_goal\",\"duration_frames\":96},\n"
                              "    {\"op\":\"set_phase\",\"phase\":\"hold_goal\",\"frame\":96},\n"
                              "    {\"op\":\"handoff\",\"phase\":\"handoff\",\"frame\":192,\"target\":\"arena/intro/ready-screen\"}\n"
                              "  ]\n"
                              "}\n",
                              bank04_available ? "true" : "false");
    if (payload_length < 0 || (size_t)payload_length >= sizeof(script_payload)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build arena intro script entry.");
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 source_bank,
                                 TECMO_ASSET_PACK_ARENA_ROUTE_CPU,
                                 script_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            script_payload,
                                            (uint64_t)payload_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena intro script entry.");
        return -1;
    }

    if (build_arena_background_layer(rom,
                                     rom_size,
                                     prg_offset,
                                     prg_banks,
                                     chr_size,
                                     background_payload,
                                     sizeof(background_payload),
                                     background_provenance,
                                     message,
                                     message_size) != 0) {
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 background_provenance->stream_bank,
                                 background_provenance->stream_cpu,
                                 background_provenance->stream_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            background_payload,
                                            sizeof(background_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena background layer entry.");
        return -1;
    }

    payload_length = snprintf(palette_payload,
                              sizeof(palette_payload),
                              "{\n"
                              "  \"format\":\"tecmo.arena-intro.palette-cycle/1\",\n"
                              "  \"input_contract\":\"ines-only\",\n"
                              "  \"source_route\":\"bank04:C-0132\",\n"
                              "  \"source_bank_available\":%s,\n"
                              "  \"runtime_shape\":\"native-palette-cycle\",\n"
                              "  \"source_snapshot_cpu\":%u,\n"
                              "  \"fixed_helper\":\"C05A/D700 setup snapshot copy\",\n"
                              "  \"work_ranges\":{\"full\":\"033E-034D\",\"low_nibbles\":\"031E-032D\"},\n"
                              "  \"stages\":[\n"
                              "    {\"name\":\"setup\",\"frame\":0,\"mode\":\"copy_rom_snapshot\"},\n"
                              "    {\"name\":\"fade_step\",\"source\":\"bank04:L88A9\",\"mode\":\"subtract_clamped\"}\n"
                              "  ],\n"
                              "  \"palette_state\":\"extractor-populated-runtime-state-pending\"\n"
                              "}\n",
                              bank04_available ? "true" : "false",
                              TECMO_ASSET_PACK_ARENA_PALETTE_CPU);
    if (payload_length < 0 || (size_t)payload_length >= sizeof(palette_payload)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build arena palette cycle entry.");
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 source_bank,
                                 TECMO_ASSET_PACK_ARENA_PALETTE_CPU,
                                 palette_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            palette_payload,
                                            (uint64_t)payload_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena palette cycle entry.");
        return -1;
    }

    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_offset,
                                  chr_size,
                                  sprite_groups_payload,
                                  sizeof(sprite_groups_payload),
                                  sprite_groups_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU,
                                 sprite_groups_provenance->pointer_table_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            sprite_groups_payload,
                                            sizeof(sprite_groups_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena sprite groups entry.");
        return -1;
    }

    if (build_ready_screen(rom,
                           rom_size,
                           prg_offset,
                           prg_banks,
                           chr_size,
                           ready_payload,
                           post_arena_provenance,
                           message,
                           message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_READY_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_READY_ROUTE_CPU,
                                 post_arena_provenance->ready_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            ready_payload,
                                            sizeof(ready_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native READY entry.");
        return -1;
    }

    if (build_warriors_transition(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_size,
                                  warriors_payload,
                                  post_arena_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_WARRIORS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU,
                                 post_arena_provenance->warriors_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            warriors_payload,
                                            sizeof(warriors_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native WARRIORS entry.");
        return -1;
    }

    if (build_clippers_transition(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_size,
                                  clippers_payload,
                                  post_arena_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_CLIPPERS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU,
                                 post_arena_provenance->clippers_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            clippers_payload,
                                            sizeof(clippers_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native CLIPPERS entry.");
        return -1;
    }

    if (build_bucks_transition(rom,
                               rom_size,
                               prg_offset,
                               prg_banks,
                               chr_size,
                               bucks_payload,
                               post_arena_provenance,
                               message,
                               message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_BUCKS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_BUCKS_ROUTE_CPU,
                                 post_arena_provenance->bucks_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            bucks_payload,
                                            sizeof(bucks_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native BUCKS entry.");
        return -1;
    }

    if (build_pass_transition(rom,
                              rom_size,
                              prg_offset,
                              prg_banks,
                              chr_size,
                              pass_payload,
                              post_arena_provenance,
                              message,
                              message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_PASS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_PASS_ROUTE_CPU,
                                 post_arena_provenance->pass_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            pass_payload,
                                            sizeof(pass_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native PASS entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_finale_sequence(rom,
                              rom_size,
                              prg_offset,
                              prg_banks,
                              chr_size,
                              enforce_finale_revision_fingerprints,
                              finale_payload,
                              finale_provenance,
                              message,
                              message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_FINALE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 0x82CFU,
                                 finale_provenance->dispatch_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            finale_payload,
                                            sizeof(finale_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native intro finale entry.");
        return -1;
    }

    return 0;
}

static int tecmo_asset_pack_build_from_ines_internal(
    const char *rom_path,
    const char *out_path,
    int enforce_finale_revision_fingerprints,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0;
    uint64_t prg_offset;
    uint64_t prg_size;
    uint64_t chr_offset;
    uint64_t chr_size;
    uint32_t prg_banks;
    uint32_t chr_banks;
    uint32_t mapper;
    uint32_t trainer_bytes;
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    char manifest[512];
    char *source_map = NULL;
    size_t source_map_size = 0U;
    TecmoOpeningScreenProvenance opening_provenance[2];
    TecmoArenaBackgroundProvenance background_provenance;
    TecmoArenaSpriteGroupsProvenance sprite_groups_provenance;
    TecmoPostArenaProvenance post_arena_provenance;
    TecmoFinaleProvenance finale_provenance;
    int manifest_length;
    int result = -1;

    if (rom_path == NULL || out_path == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "ROM path and output path are required.");
        return -1;
    }
    if (tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not read iNES ROM.");
        return -1;
    }
    if (rom_size < 16U ||
        rom[0] != 'N' ||
        rom[1] != 'E' ||
        rom[2] != 'S' ||
        rom[3] != 0x1AU) {
        tecmo_asset_pack_set_message(message, message_size, "Input is not an iNES ROM.");
        goto cleanup;
    }

    prg_banks = rom[4];
    chr_banks = rom[5];
    mapper = (uint32_t)(rom[6] >> 4U) | ((uint32_t)rom[7] & 0xF0U);
    trainer_bytes = (rom[6] & 0x04U) != 0U ? 512U : 0U;
    prg_offset = 16ULL + (uint64_t)trainer_bytes;
    prg_size = (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    chr_offset = prg_offset + prg_size;
    chr_size = (uint64_t)chr_banks * TECMO_ASSET_PACK_CHR_BANK_BYTES;

    if (prg_banks == 0U || rom_size < chr_offset + chr_size) {
        tecmo_asset_pack_set_message(message, message_size, "ROM is shorter than its iNES PRG/CHR sizes.");
        goto cleanup;
    }

    if (tecmo_asset_pack_builder_begin(&builder, out_path, message, message_size) != 0) {
        goto cleanup;
    }

    manifest_length = snprintf(manifest,
                               sizeof(manifest),
                               "format=tecmo.assetpack/1\n"
                               "source=ines\n"
                               "source_map=system/source-map\n"
                               "mapper=%u\n"
                               "trainer_bytes=%u\n"
                               "prg_banks_16k=%u\n"
                               "chr_banks_8k=%u\n"
                               "raw_entry_prefixes=prg/,chr/\n"
                               "logical_entry_prefixes=arena/intro/,intro/\n"
                               "input_contract=ines-only\n",
                               mapper,
                               trainer_bytes,
                               prg_banks,
                               chr_banks);
    if (manifest_length < 0 || (size_t)manifest_length >= sizeof(manifest)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build asset pack manifest.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("system/manifest",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            manifest,
                                            (uint64_t)manifest_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write asset pack manifest.");
        goto cleanup;
    }

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        entry_info = tecmo_asset_pack_make_entry_info(id,
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     bank,
                                     0x8000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write PRG bank asset.");
            goto cleanup;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        entry_info = tecmo_asset_pack_make_entry_info("prg/fixed",
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     fixed_bank,
                                     0xC000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write fixed PRG bank asset.");
            goto cleanup;
        }
    }

    if (chr_size > 0U) {
        entry_info = tecmo_asset_pack_make_entry_info("chr/all",
                                     TECMO_ASSET_PACK_TYPE_CHR,
                                     0U,
                                     0U,
                                     chr_offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)chr_offset,
                                                chr_size,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write CHR asset.");
            goto cleanup;
        }

        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;

            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            entry_info = tecmo_asset_pack_make_entry_info(id,
                                         TECMO_ASSET_PACK_TYPE_CHR,
                                         bank,
                                         0U,
                                         offset,
                                         TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
            if (tecmo_asset_pack_builder_add_memory(builder,
                                                    &entry_info,
                                                    rom + (size_t)offset,
                                                    TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                                    message,
                                                    message_size) != 0) {
                tecmo_asset_pack_set_message(message, message_size, "Could not write CHR bank asset.");
                goto cleanup;
            }
        }
    }

    memset(opening_provenance, 0, sizeof(opening_provenance));
    memset(&background_provenance, 0, sizeof(background_provenance));
    memset(&sprite_groups_provenance, 0, sizeof(sprite_groups_provenance));
    memset(&post_arena_provenance, 0, sizeof(post_arena_provenance));
    if (add_native_arena_intro_entries(builder,
                                       rom,
                                       rom_size,
                                       prg_offset,
                                       prg_banks,
                                       chr_offset,
                                       chr_size,
                                       enforce_finale_revision_fingerprints,
                                       opening_provenance,
                                       &background_provenance,
                                       &sprite_groups_provenance,
                                       &post_arena_provenance,
                                       &finale_provenance,
                                       message,
                                       message_size) != 0) {
        goto cleanup;
    }

    source_map = tecmo_asset_pack_build_ines_source_map(mapper,
                                       trainer_bytes,
                                       prg_banks,
                                       chr_banks,
                                       prg_offset,
                                       chr_offset,
                                       chr_size,
                                       opening_provenance,
                                       &background_provenance,
                                       &sprite_groups_provenance,
                                       &post_arena_provenance,
                                       &finale_provenance,
                                       &source_map_size);
    if (source_map == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build asset pack source map.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("system/source-map",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            source_map,
                                            (uint64_t)source_map_size,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write asset pack source map.");
        goto cleanup;
    }
    free(source_map);
    source_map = NULL;

    {
        size_t entry_count = tecmo_asset_pack_builder_entry_count(builder);
        int finish_result = tecmo_asset_pack_builder_finish(builder, message, message_size);

        builder = NULL;
        if (finish_result != 0) {
            goto cleanup;
        }
        if (message != NULL && message_size > 0U) {
            (void)snprintf(message,
                           message_size,
                           "Wrote %u PRG banks, %u CHR banks, %llu entries to %s from iNES ROM",
                           prg_banks,
                           chr_banks,
                           (unsigned long long)entry_count,
                           out_path);
        }
    }
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(source_map);
    free(rom);
    return result;
}

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size)
{
    return tecmo_asset_pack_build_from_ines_internal(rom_path,
                                                      out_path,
                                                      1,
                                                      message,
                                                      message_size);
}

static int make_self_test_temp_dir(char *path, size_t path_size)
{
    if (path == NULL || path_size == 0U) {
        return -1;
    }
    path[0] = '\0';

#ifdef _WIN32
    {
        char temp_root[TECMO_ASSET_PACK_PATH_SIZE];
        char temp_name[TECMO_ASSET_PACK_PATH_SIZE];
        DWORD root_length = GetTempPathA((DWORD)sizeof(temp_root), temp_root);

        if (root_length == 0U || root_length >= sizeof(temp_root)) {
            return -1;
        }
        if (GetTempFileNameA(temp_root, "tap", 0U, temp_name) == 0U) {
            return -1;
        }
        if (DeleteFileA(temp_name) == 0) {
            return -1;
        }
        if (CreateDirectoryA(temp_name, NULL) == 0) {
            return -1;
        }
        return tecmo_asset_pack_copy_path(path, path_size, temp_name);
    }
#else
    {
        const char *directory = getenv("TMPDIR");
        char template_path[TECMO_ASSET_PACK_PATH_SIZE];
        char *created;
        int written;

        if (directory == NULL || directory[0] == '\0') {
            directory = "/tmp";
        }
        written = snprintf(template_path,
                           sizeof(template_path),
                           "%s%stecmo_asset_pack_self_test_XXXXXX",
                           directory,
                           directory[strlen(directory) - 1U] == '/' ? "" : "/");
        if (written < 0 || (size_t)written >= sizeof(template_path)) {
            return -1;
        }
        created = mkdtemp(template_path);
        if (created == NULL) {
            return -1;
        }
        return tecmo_asset_pack_copy_path(path, path_size, created);
    }
#endif
}

static int make_self_test_path(char *path,
                               size_t path_size,
                               const char *directory,
                               const char *file_name)
{
    const char *separator = "";
    size_t directory_length;
    int written;

    if (path == NULL || path_size == 0U ||
        directory == NULL || directory[0] == '\0' ||
        file_name == NULL || file_name[0] == '\0') {
        return -1;
    }

    directory_length = strlen(directory);
    if (directory_length > 0U &&
        directory[directory_length - 1U] != '/' &&
        directory[directory_length - 1U] != '\\') {
        separator = "/";
    }

    written = snprintf(path, path_size, "%s%s%s", directory, separator, file_name);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }
    return 0;
}

static void remove_self_test_file(const char *path)
{
    if (path != NULL && path[0] != '\0') {
        (void)remove(path);
    }
}

static void remove_self_test_dir(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

static int write_self_test_file(const char *path, const uint8_t *bytes, size_t byte_count)
{
    FILE *file;
    int result = -1;

    if (path == NULL || (bytes == NULL && byte_count > 0U)) {
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (byte_count == 0U || fwrite(bytes, 1U, byte_count, file) == byte_count) {
        result = 0;
    }
    if (fclose(file) != 0) {
        result = -1;
    }
    return result;
}

static int bytes_contain_text(const uint8_t *bytes, uint64_t byte_count, const char *needle)
{
    size_t haystack_size;
    size_t needle_size;

    if (bytes == NULL || needle == NULL || byte_count > (uint64_t)SIZE_MAX) {
        return 0;
    }

    haystack_size = (size_t)byte_count;
    needle_size = strlen(needle);
    if (needle_size == 0U) {
        return 1;
    }
    if (needle_size > haystack_size) {
        return 0;
    }

    for (size_t i = 0; i <= haystack_size - needle_size; ++i) {
        if (memcmp(bytes + i, needle, needle_size) == 0) {
            return 1;
        }
    }
    return 0;
}

static int self_test_read_and_compare(const char *pack_path,
                                      const char *entry_id,
                                      const uint8_t *expected,
                                      uint64_t expected_size,
                                      char *message,
                                      size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (byte_count != expected_size ||
        (expected_size > 0U && memcmp(bytes, expected, (size_t)expected_size) != 0)) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test readback mismatch for entry '%s'.", entry_id);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_background_layer(const char *pack_path,
                                            const uint8_t expected_palette[16],
                                            char *message,
                                            size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    const uint8_t *cell;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path,
                                    TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not read arena background layer.");
        return -1;
    }
    if (byte_count != TECMO_ASSET_PACK_ARENA_LAYER_SIZE ||
        memcmp(bytes, "TATL", 4U) != 0 ||
        tecmo_asset_pack_read_u16(bytes + 4U) != 1U ||
        tecmo_asset_pack_read_u16(bytes + 6U) != 48U ||
        tecmo_asset_pack_read_u16(bytes + 8U) != 32U ||
        tecmo_asset_pack_read_u16(bytes + 10U) != 51U ||
        tecmo_asset_pack_read_u16(bytes + 12U) != 32U ||
        tecmo_asset_pack_read_u16(bytes + 14U) != 30U ||
        tecmo_asset_pack_read_u16(bytes + 16U) != 6U ||
        tecmo_asset_pack_read_u16(bytes + 18U) != 0U ||
        tecmo_asset_pack_read_u32(bytes + 20U) != 1632U ||
        tecmo_asset_pack_read_u32(bytes + 24U) != 48U ||
        tecmo_asset_pack_read_u32(bytes + 28U) != 32U ||
        memcmp(bytes + 32U, expected_palette, 16U) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test arena background header mismatch.");
        goto cleanup;
    }

#define SELF_TEST_CELL(row, column) \
    (bytes + TECMO_ASSET_PACK_ARENA_LAYER_HEADER_SIZE + \
     (((size_t)(row) * TECMO_ASSET_PACK_ARENA_LAYER_WIDTH + (column)) * \
      TECMO_ASSET_PACK_ARENA_LAYER_CELL_STRIDE))
    cell = SELF_TEST_CELL(0U, 0U);
    if (cell[0] != 1U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 16U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test literal tile cell mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(0U, 2U);
    if (cell[0] != 3U || cell[1] != 2U || tecmo_asset_pack_read_u32(cell + 2U) != 48U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test attribute quadrant mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(16U, 0U);
    if (cell[0] != 9U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96400U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test page-1 lower CHR cell mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(38U, 0U);
    if (cell[0] != 6U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96U) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Self-test page-0 upper CHR cell mismatch: %u/%u/%u.",
                     (unsigned int)cell[0],
                     (unsigned int)cell[1],
                     tecmo_asset_pack_read_u32(cell + 2U));
        goto cleanup;
    }
    cell = SELF_TEST_CELL(48U, 0U);
    if (cell[0] != 8U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96384U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test page-0 lower CHR split mismatch.");
        goto cleanup;
    }
#undef SELF_TEST_CELL

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_sprite_groups(const char *pack_path,
                                         const uint8_t expected_palette[16],
                                         char *message,
                                         size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    const uint8_t *jumbotron;
    const uint8_t *goal;
    const uint8_t *piece;
    size_t connector_overlay_piece_count = 0U;
    size_t zero_connector_overlay_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path,
                                    TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not read arena sprite groups.");
        return -1;
    }
    if (byte_count != TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE ||
        memcmp(bytes, "TASG", 4U) != 0 ||
        tecmo_asset_pack_read_u16(bytes + 4U) != 2U ||
        tecmo_asset_pack_read_u16(bytes + 6U) != 48U ||
        tecmo_asset_pack_read_u16(bytes + 8U) != 2U ||
        tecmo_asset_pack_read_u16(bytes + 10U) != 20U ||
        tecmo_asset_pack_read_u32(bytes + 12U) != 71U ||
        tecmo_asset_pack_read_u16(bytes + 16U) != 12U ||
        tecmo_asset_pack_read_u16(bytes + 18U) != 1U ||
        tecmo_asset_pack_read_u32(bytes + 20U) != 48U ||
        tecmo_asset_pack_read_u32(bytes + 24U) != 64U ||
        tecmo_asset_pack_read_u32(bytes + 28U) != 104U ||
        memcmp(bytes + 48U, expected_palette, 16U) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG header or palette mismatch.");
        goto cleanup;
    }
    for (size_t index = 32U; index < 48U; ++index) {
        if (bytes[index] != 0U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG reserved header bytes are nonzero.");
            goto cleanup;
        }
    }
    for (size_t index = TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET;
         index < TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET + 16U;
         ++index) {
        if (bytes[index] > 0x3FU) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG palette color is outside the NES palette.");
            goto cleanup;
        }
    }

    jumbotron = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_OFFSET;
    goal = jumbotron + TECMO_ASSET_PACK_ARENA_SPRITE_GROUP_STRIDE;
    if (tecmo_asset_pack_read_u16(jumbotron + 0U) != 1U ||
        tecmo_asset_pack_read_u16(jumbotron + 2U) != 1U ||
        tecmo_asset_pack_read_u32(jumbotron + 4U) != 0U ||
        tecmo_asset_pack_read_u32(jumbotron + 8U) != 55U ||
        tecmo_asset_pack_read_u16(jumbotron + 12U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 14U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 16U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 18U) != 2U ||
        tecmo_asset_pack_read_u16(goal + 0U) != 2U ||
        tecmo_asset_pack_read_u16(goal + 2U) != 0U ||
        tecmo_asset_pack_read_u32(goal + 4U) != 55U ||
        tecmo_asset_pack_read_u32(goal + 8U) != 16U ||
        tecmo_asset_pack_read_u16(goal + 12U) != 165U ||
        tecmo_asset_pack_read_u16(goal + 14U) != 350U ||
        tecmo_asset_pack_read_u16(goal + 16U) != 0U ||
        tecmo_asset_pack_read_u16(goal + 18U) != 2U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG group descriptors mismatch.");
        goto cleanup;
    }

    piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET;
    if ((int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 32 ||
        (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 16 ||
        tecmo_asset_pack_read_u32(piece + 4U) != 8192U + 0x22U * 16U ||
        piece[8] != 3U || piece[9] != 7U || tecmo_asset_pack_read_u16(piece + 10U) != 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG first jumbotron piece mismatch.");
        goto cleanup;
    }
    piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET +
            TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT *
                TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE;
    if ((int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 3 ||
        (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 0 ||
        tecmo_asset_pack_read_u32(piece + 4U) != 8192U + 0x40U * 16U ||
        piece[8] != 2U || piece[9] != 1U || tecmo_asset_pack_read_u16(piece + 10U) != 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG first goal piece mismatch.");
        goto cleanup;
    }

    for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_COUNT; ++index) {
        uint32_t chr_byte_offset;
        int16_t connector_overlay_y_adjust;
        piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET +
                index * TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE;
        chr_byte_offset = tecmo_asset_pack_read_u32(piece + 4U);
        connector_overlay_y_adjust = (int16_t)tecmo_asset_pack_read_u16(piece + 10U);
        if (connector_overlay_y_adjust ==
            TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_OVERLAY_Y_ADJUST) {
            ++connector_overlay_piece_count;
            if (index < TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT ||
                (int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 16 ||
                (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 32 ||
                chr_byte_offset != 9056U) {
                tecmo_asset_pack_set_message(message, message_size, "Self-test TASG connector-overlay goal piece mismatch.");
                goto cleanup;
            }
        } else if (connector_overlay_y_adjust == 0) {
            ++zero_connector_overlay_count;
        } else {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG piece connector overlay mismatch.");
            goto cleanup;
        }
        if (piece[8] > 3U || piece[9] > 7U ||
            (chr_byte_offset & 0x0FU) != 0U ||
            chr_byte_offset < TECMO_ASSET_PACK_ARENA_SPRITE_CHR_BASE ||
            (uint64_t)chr_byte_offset + 32U >
                TECMO_ASSET_PACK_ARENA_SPRITE_CHR_LIMIT) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG piece contract mismatch.");
            goto cleanup;
        }
    }
    if (connector_overlay_piece_count != 1U || zero_connector_overlay_count != 70U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG connector-overlay counts mismatch.");
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_sprite_group_validation(uint8_t *rom,
                                                   uint64_t rom_size,
                                                   uint64_t prg_offset,
                                                   uint64_t chr_offset,
                                                   uint64_t chr_size,
                                                   char *message,
                                                   size_t message_size)
{
    uint8_t payload[TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE];
    TecmoArenaSpriteGroupsProvenance provenance;
    uint64_t bank04_offset = prg_offset +
                             (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 *
                                 TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t palette_offset = bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PALETTE_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t seeds_offset = bank04_offset +
                            (TECMO_ASSET_PACK_ARENA_SEEDS_CPU -
                             TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t params_offset = bank04_offset +
                             (TECMO_ASSET_PACK_ARENA_PARAMS_CPU -
                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t first_record_tile_offset = prg_offset + (0xA7E3U - 0x8000U) + 2U;
    uint64_t goal_connector_record_offset =
        prg_offset + (0xA8C0U - 0x8000U) + 1U + 7U * 4U;
    uint8_t saved;
    char validation_message[192];

    saved = rom[(size_t)palette_offset + 1U];
    rom[(size_t)palette_offset + 1U] = 0x40U;
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)palette_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted an invalid sprite palette color.");
        return -1;
    }
    rom[(size_t)palette_offset + 1U] = saved;

    saved = rom[(size_t)first_record_tile_offset];
    rom[(size_t)first_record_tile_offset] = 0x7DU;
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) != 0 ||
        tecmo_asset_pack_read_u32(payload + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET + 4U) !=
            TECMO_ASSET_PACK_ARENA_SPRITE_CHR_LIMIT - 32U) {
        rom[(size_t)first_record_tile_offset] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test rejected the final valid sprite CHR pair.");
        return -1;
    }
    rom[(size_t)first_record_tile_offset] = 0x7FU;
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)first_record_tile_offset] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted a sprite CHR pair outside pages 8/9.");
        return -1;
    }
    rom[(size_t)first_record_tile_offset] = saved;

    saved = rom[(size_t)goal_connector_record_offset + 1U];
    /* $35 still normalizes to top tile $36, so this checks raw identity too. */
    rom[(size_t)goal_connector_record_offset + 1U] = 0x35U;
    validation_message[0] = '\0';
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0 ||
        strstr(validation_message, "connector source-contract mismatch") == NULL) {
        rom[(size_t)goal_connector_record_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message,
                    message_size,
                    "Self-test accepted a connector record with the wrong raw tile identity.");
        return -1;
    }
    rom[(size_t)goal_connector_record_offset + 1U] = saved;

    saved = rom[(size_t)goal_connector_record_offset + 2U];
    rom[(size_t)goal_connector_record_offset + 2U] = 0x03U;
    validation_message[0] = '\0';
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0 ||
        strstr(validation_message, "connector source-contract mismatch") == NULL) {
        rom[(size_t)goal_connector_record_offset + 2U] = saved;
        tecmo_asset_pack_set_message(message,
                    message_size,
                    "Self-test accepted a connector record with the wrong source attributes.");
        return -1;
    }
    rom[(size_t)goal_connector_record_offset + 2U] = saved;

    saved = rom[(size_t)seeds_offset + 1U];
    rom[(size_t)seeds_offset + 1U] ^= 1U;
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)seeds_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted invalid sprite anchor seeds.");
        return -1;
    }
    rom[(size_t)seeds_offset + 1U] = saved;

    saved = rom[(size_t)params_offset + 1U];
    rom[(size_t)params_offset + 1U] ^= 1U;
    if (build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)params_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted invalid sprite anchor parameters.");
        return -1;
    }
    rom[(size_t)params_offset + 1U] = saved;
    return 0;
}

static int self_test_entry_contains_text(const char *pack_path,
                                         const char *entry_id,
                                         const char *needle,
                                         char *message,
                                         size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (!bytes_contain_text(bytes, byte_count, needle)) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Self-test entry '%s' did not contain '%s'.",
                     entry_id,
                     needle);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

typedef struct TecmoAssetPackSelfTestBuilderListState {
    unsigned int count;
    int saw_memory;
    int saw_file;
    uint64_t memory_size;
    uint64_t file_size;
    char *message;
    size_t message_size;
} TecmoAssetPackSelfTestBuilderListState;

static int self_test_builder_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                        void *user_data)
{
    TecmoAssetPackSelfTestBuilderListState *state =
        (TecmoAssetPackSelfTestBuilderListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "test/memory") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 7U ||
            entry_info->cpu_address != 0x8123U ||
            entry_info->source_offset != 0x1234ULL ||
            entry_info->byte_count != state->memory_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_DERIVED) {
            tecmo_asset_pack_set_message(state->message,
                        state->message_size,
                        "Self-test memory directory metadata mismatch.");
            return -1;
        }
        state->saw_memory = 1;
    } else if (strcmp(entry_info->id, "test/file") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 3U ||
            entry_info->cpu_address != 0x9000U ||
            entry_info->source_offset != 0x5678ULL ||
            entry_info->byte_count != state->file_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_LOCAL) {
            tecmo_asset_pack_set_message(state->message,
                        state->message_size,
                        "Self-test file directory metadata mismatch.");
            return -1;
        }
        state->saw_file = 1;
    }

    return 0;
}

typedef struct TecmoAssetPackSelfTestInesListState {
    unsigned int count;
    int saw_manifest;
    int saw_source_map;
    int saw_presents;
    int presents_metadata_valid;
    int saw_license;
    int license_metadata_valid;
    int saw_arena_intro_script;
    int saw_arena_intro_background;
    int arena_intro_background_metadata_valid;
    int saw_arena_intro_palette;
    int saw_arena_intro_sprite_groups;
    int arena_intro_sprite_groups_metadata_valid;
    int saw_ready;
    int ready_metadata_valid;
    int saw_warriors;
    int warriors_metadata_valid;
    int saw_clippers;
    int clippers_metadata_valid;
    int saw_bucks;
    int bucks_metadata_valid;
    int saw_pass;
    int pass_metadata_valid;
    int saw_finale;
    int finale_metadata_valid;
    int saw_prg_bank0;
    int saw_prg_bank1;
    int saw_prg_fixed;
    int saw_chr_all;
    int saw_chr_bank0;
    int saw_chr_bank1;
} TecmoAssetPackSelfTestInesListState;

static int self_test_ines_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                     void *user_data)
{
    TecmoAssetPackSelfTestInesListState *state =
        (TecmoAssetPackSelfTestInesListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "system/manifest") == 0) {
        state->saw_manifest = 1;
    } else if (strcmp(entry_info->id, "system/source-map") == 0) {
        state->saw_source_map = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_PRESENTS_ID) == 0) {
        state->saw_presents = 1;
        state->presents_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_PRESENTS_STREAM_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_PRESENTS_STREAM_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_PRESENTS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_LICENSE_ID) == 0) {
        state->saw_license = 1;
        state->license_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_LICENSE_STREAM_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_LICENSE_STREAM_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_LICENSE_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID) == 0) {
        state->saw_arena_intro_script = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID) == 0) {
        state->saw_arena_intro_background = 1;
        state->arena_intro_background_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == 0xA000U &&
            entry_info->source_offset == 16ULL + 0x2000ULL &&
            entry_info->byte_count == TECMO_ASSET_PACK_ARENA_LAYER_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID) == 0) {
        state->saw_arena_intro_palette = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID) == 0) {
        state->saw_arena_intro_sprite_groups = 1;
        state->arena_intro_sprite_groups_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_READY_ID) == 0) {
        state->saw_ready = 1;
        state->ready_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_READY_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_READY_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_WARRIORS_ID) == 0) {
        state->saw_warriors = 1;
        state->warriors_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_WARRIORS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_CLIPPERS_ID) == 0) {
        state->saw_clippers = 1;
        state->clippers_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_CLIPPERS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_BUCKS_ID) == 0) {
        state->saw_bucks = 1;
        state->bucks_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_BUCKS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_BUCKS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_PASS_ID) == 0) {
        state->saw_pass = 1;
        state->pass_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_PASS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_PASS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_FINALE_ID) == 0) {
        state->saw_finale = 1;
        state->finale_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 4U &&
            entry_info->cpu_address == 0x82CFU &&
            entry_info->byte_count == TECMO_ASSET_PACK_FINALE_SIZE;
    } else if (strcmp(entry_info->id, "prg/bank00") == 0) {
        state->saw_prg_bank0 = 1;
    } else if (strcmp(entry_info->id, "prg/bank01") == 0) {
        state->saw_prg_bank1 = 1;
    } else if (strcmp(entry_info->id, "prg/fixed") == 0) {
        state->saw_prg_fixed = 1;
    } else if (strcmp(entry_info->id, "chr/all") == 0) {
        state->saw_chr_all = 1;
    } else if (strcmp(entry_info->id, "chr/bank00") == 0) {
        state->saw_chr_bank0 = 1;
    } else if (strcmp(entry_info->id, "chr/bank01") == 0) {
        state->saw_chr_bank1 = 1;
    }

    return 0;
}

static size_t write_self_test_opening_stream(uint8_t *bank00,
                                             size_t offset,
                                             unsigned int group_count,
                                             uint8_t literal_count,
                                             uint8_t fourth_repeat_count,
                                             uint8_t terminator_count)
{
    size_t source = offset;

    for (unsigned int group = 0U; group < group_count; ++group) {
        bank00[source++] = group == 0U ? 0xA9U : 0xAAU;
        for (unsigned int slot = 0U; slot < 4U; ++slot) {
            unsigned int operation_index = group * 4U + slot;
            if (operation_index == 0U) {
                bank00[source++] = literal_count;
                memset(bank00 + source, 0, literal_count);
                source += literal_count;
            } else {
                unsigned int repeat_index = operation_index - 1U;
                bank00[source++] = repeat_index < 3U
                                       ? 0U
                                       : (repeat_index == 3U ? fourth_repeat_count : 1U);
                bank00[source++] = 0U;
            }
        }
    }
    bank00[source++] = 0U;
    bank00[source++] = terminator_count;
    return source - offset;
}

static int populate_self_test_opening_fixture(uint8_t *bank00, uint8_t *fixed)
{
    static const uint8_t selector_contract[7] = {
        0xA2U,0xF4U,0x86U,0x57U,0xE8U,0x86U,0x58U
    };
    uint8_t *presents_descriptor = fixed +
        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
        TECMO_ASSET_PACK_PRESENTS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    uint8_t *license_descriptor = fixed +
        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
        TECMO_ASSET_PACK_LICENSE_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    size_t presents_size;
    size_t license_size;

    if (bank00 == NULL || fixed == NULL) return -1;
    presents_descriptor[0] = TECMO_ASSET_PACK_PRESENTS_BG_R0 / 2U;
    presents_descriptor[1] = TECMO_ASSET_PACK_PRESENTS_BG_R1 / 2U;
    tecmo_asset_pack_store_u16(presents_descriptor + 2U, TECMO_ASSET_PACK_OPENING_PALETTE_CPU);
    tecmo_asset_pack_store_u16(presents_descriptor + 4U, TECMO_ASSET_PACK_PRESENTS_STREAM_CPU);
    presents_descriptor[6] = 0U;
    memcpy(license_descriptor, presents_descriptor, TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
    tecmo_asset_pack_store_u16(license_descriptor + 4U, TECMO_ASSET_PACK_LICENSE_STREAM_CPU);

    presents_size = write_self_test_opening_stream(
        bank00,
        TECMO_ASSET_PACK_PRESENTS_STREAM_CPU - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE,
        12U,
        7U,
        206U,
        0xA9U);
    license_size = write_self_test_opening_stream(
        bank00,
        TECMO_ASSET_PACK_LICENSE_STREAM_CPU - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE,
        18U,
        14U,
        175U,
        0U);
    if (presents_size != TECMO_ASSET_PACK_PRESENTS_STREAM_SIZE ||
        license_size != TECMO_ASSET_PACK_LICENSE_STREAM_SIZE) return -1;

    for (size_t color = 0U; color < 16U; ++color) {
        bank00[TECMO_ASSET_PACK_OPENING_PALETTE_CPU -
               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + color] =
            color % 4U == 0U ? 0x0FU : (uint8_t)(0x10U + color);
    }
    memcpy(bank00 +
               (TECMO_ASSET_PACK_PRESENTS_SELECTOR_CODE_CPU -
                TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE),
           selector_contract,
           sizeof(selector_contract));
    bank00[TECMO_ASSET_PACK_PRESENTS_Y_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x40U;
    bank00[TECMO_ASSET_PACK_PRESENTS_TILE_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x55U;
    bank00[TECMO_ASSET_PACK_PRESENTS_X_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x50U;
    bank00[TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] =
        TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT;
    for (size_t record = 0U;
         record < TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT;
         ++record) {
        size_t offset = TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU -
                        TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + 1U + record * 4U;
        bank00[offset + 0U] = (uint8_t)((record / 4U) * 8U);
        bank00[offset + 1U] = (uint8_t)record;
        bank00[offset + 2U] = (uint8_t)(record >= 10U ? 1U : 0U);
        bank00[offset + 3U] = (uint8_t)((record % 4U) * 8U);
    }
    for (size_t color = 0U; color < 16U; ++color) {
        bank00[TECMO_ASSET_PACK_PRESENTS_SPRITE_PALETTE_CPU -
               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + color] =
            color % 4U == 0U ? 0x0FU : (uint8_t)(0x20U + color);
    }
    return 0;
}

static void write_self_test_chr_selector(uint8_t *bank04,
                                         uint16_t instruction_cpu,
                                         uint8_t selector,
                                         uint8_t destination)
{
    size_t offset = instruction_cpu - 0x8000U;
    bank04[offset + 0U] = 0xA9U;
    bank04[offset + 1U] = selector;
    bank04[offset + 2U] = 0x85U;
    bank04[offset + 3U] = destination;
}

int tecmo_asset_pack_self_test(char *message, size_t message_size)
{
    static const uint8_t memory_entry_bytes[] = {0x00U, 0x01U, 0x7FU, 0x80U, 0xFFU};
    static const uint8_t file_entry_bytes[] = {
        'l', 'o', 'c', 'a', 'l', '-', 'f', 'i', 'l', 'e', '-', 'e', 'n', 't', 'r', 'y'
    };
    static const uint8_t arena_palette[16] = {
        0x0FU, 0x01U, 0x02U, 0x03U, 0x0FU, 0x04U, 0x05U, 0x06U,
        0x0FU, 0x07U, 0x08U, 0x09U, 0x0FU, 0x0AU, 0x0BU, 0x0CU
    };
    static const uint8_t arena_sprite_palette[16] = {
        0x02U, 0x11U, 0x12U, 0x13U, 0x02U, 0x14U, 0x15U, 0x16U,
        0x02U, 0x17U, 0x18U, 0x19U, 0x02U, 0x1AU, 0x1BU, 0x1CU
    };
    static const uint8_t expected_sprite_palette[16] = {
        0x0FU, 0x11U, 0x12U, 0x13U, 0x0FU, 0x14U, 0x15U, 0x16U,
        0x0FU, 0x17U, 0x18U, 0x19U, 0x0FU, 0x1AU, 0x1BU, 0x1CU
    };
    static const uint8_t arena_sprite_seeds[4] = {0x00U, 0x1EU, 0x00U, 0x01U};
    static const uint8_t arena_sprite_params[4] = {0x00U, 0xB8U, 0x00U, 0x01U};
    static const uint8_t arena_stream[] = {
        0xB9U,
        0x04U, 0x01U, 0x02U, 0x03U, 0x04U,
        0x00U, 0x05U,
        0x04U, 0x07U, 0x00U,
        0x00U, 0x06U,
        0xAAU,
        0x00U, 0x07U,
        0x00U, 0x08U,
        0x00U, 0x09U,
        0x00U, 0x0AU,
        0x0AU,
        0x00U, 0x0BU,
        0xF8U, 0x0CU,
        0x00U
    };
    char builder_pack_path[1024] = {0};
    char local_file_path[1024] = {0};
    char rom_path[1024] = {0};
    char ines_pack_path[1024] = {0};
    char temp_dir[1024] = {0};
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    TecmoAssetPackSelfTestBuilderListState builder_list_state;
    TecmoAssetPackSelfTestInesListState ines_list_state;
    uint8_t *rom = NULL;
    char *dump = NULL;
    uint64_t prg_offset = 16ULL;
    uint64_t chr_offset = 16ULL + 8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size = 32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint64_t rom_size = 16ULL + 8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES +
                        32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    size_t dump_size = 0U;
    int result = -1;

    if (tecmo_asset_pack_finale_self_test(message, message_size) != 0) {
        return -1;
    }

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }

    if (tecmo_asset_pack_d9f6_self_test(message, message_size) != 0) {
        goto cleanup;
    }

    if (make_self_test_temp_dir(temp_dir, sizeof(temp_dir)) != 0 ||
        make_self_test_path(builder_pack_path, sizeof(builder_pack_path), temp_dir, "builder.assetpack") != 0 ||
        make_self_test_path(local_file_path, sizeof(local_file_path), temp_dir, "local.bin") != 0 ||
        make_self_test_path(rom_path, sizeof(rom_path), temp_dir, "input.nes") != 0 ||
        make_self_test_path(ines_pack_path, sizeof(ines_pack_path), temp_dir, "ines.assetpack") != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not create temporary paths.");
        goto cleanup;
    }

    if (tecmo_asset_pack_builder_begin(&builder, builder_pack_path, message, message_size) != 0) {
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("test/memory",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 7U,
                                 0x8123U,
                                 0x1234ULL,
                                 TECMO_ASSET_PACK_FLAG_DERIVED);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            memory_entry_bytes,
                                            sizeof(memory_entry_bytes),
                                            message,
                                            message_size) != 0) {
        goto cleanup;
    }

    if (write_self_test_file(local_file_path, file_entry_bytes, sizeof(file_entry_bytes)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not write temporary file entry source.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("test/file",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 3U,
                                 0x9000U,
                                 0x5678ULL,
                                 TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_file(builder,
                                          &entry_info,
                                          local_file_path,
                                          message,
                                          message_size) != 0) {
        goto cleanup;
    }
    if (tecmo_asset_pack_builder_finish(builder, message, message_size) != 0) {
        builder = NULL;
        goto cleanup;
    }
    builder = NULL;

    if (self_test_read_and_compare(builder_pack_path,
                                   "test/memory",
                                   memory_entry_bytes,
                                   sizeof(memory_entry_bytes),
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(builder_pack_path,
                                   "test/file",
                                   file_entry_bytes,
                                   sizeof(file_entry_bytes),
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&builder_list_state, 0, sizeof(builder_list_state));
    builder_list_state.memory_size = sizeof(memory_entry_bytes);
    builder_list_state.file_size = sizeof(file_entry_bytes);
    builder_list_state.message = message;
    builder_list_state.message_size = message_size;
    if (tecmo_asset_pack_list_entries(builder_pack_path,
                                      self_test_builder_list_entry,
                                      &builder_list_state) != 0 ||
        builder_list_state.count != 2U ||
        !builder_list_state.saw_memory ||
        !builder_list_state.saw_file) {
        if (message != NULL && message_size > 0U && message[0] == '\0') {
            tecmo_asset_pack_set_message(message, message_size, "Self-test directory enumeration mismatch.");
        }
        goto cleanup;
    }

    if (tecmo_asset_pack_dump_directory(builder_pack_path, NULL, 0U, &dump_size) != 0 ||
        dump_size == 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not size directory dump.");
        goto cleanup;
    }
    dump = (char *)malloc(dump_size);
    if (dump == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not allocate directory dump.");
        goto cleanup;
    }
    if (tecmo_asset_pack_dump_directory(builder_pack_path, dump, dump_size, &dump_size) != 0 ||
        strstr(dump, "test/memory") == NULL ||
        strstr(dump, "test/file") == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test directory dump did not include expected entries.");
        goto cleanup;
    }
    free(dump);
    dump = NULL;

    if (rom_size > (uint64_t)SIZE_MAX) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test ROM fixture is too large.");
        goto cleanup;
    }
    rom = (uint8_t *)calloc(1U, (size_t)rom_size);
    if (rom == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not allocate ROM fixture.");
        goto cleanup;
    }
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1AU;
    rom[4] = 8U;
    rom[5] = 32U;

    for (uint32_t bank = 0U; bank < 8U; ++bank) {
        for (size_t i = 0; i < (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES; ++i) {
            rom[(size_t)(prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES) + i] =
                (uint8_t)((bank * 29U) ^ (i & 0xFFU));
        }
    }
    for (size_t i = 0; i < (size_t)chr_size; ++i) {
        rom[(size_t)chr_offset + i] = (uint8_t)(0x40U ^ (i & 0xFFU));
    }

    if (populate_self_test_opening_fixture(
            rom + (size_t)prg_offset,
            rom + (size_t)(prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not build opening screen fixture.");
        goto cleanup;
    }

    {
        uint64_t fixed_offset = prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t descriptor_offset = fixed_offset +
                                     (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                                     TECMO_ASSET_PACK_ARENA_SCREEN_ID *
                                         TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
        uint64_t stream_offset = prg_offset + (0xA000U - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        uint64_t palette_offset = prg_offset + (0xA100U - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);

        rom[(size_t)descriptor_offset + 0U] = 0U;
        rom[(size_t)descriptor_offset + 1U] = 1U;
        rom[(size_t)descriptor_offset + 2U] = 0x00U;
        rom[(size_t)descriptor_offset + 3U] = 0xA1U;
        rom[(size_t)descriptor_offset + 4U] = 0x00U;
        rom[(size_t)descriptor_offset + 5U] = 0xA0U;
        rom[(size_t)descriptor_offset + 6U] = 0U;
        memcpy(rom + (size_t)stream_offset, arena_stream, sizeof(arena_stream));
        memcpy(rom + (size_t)palette_offset, arena_palette, sizeof(arena_palette));
        rom[(size_t)(fixed_offset +
                     (TECMO_ASSET_PACK_ARENA_LOWER_R0_TABLE_CPU - 0xC000U) +
                     TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX)] = 2U;
        rom[(size_t)(fixed_offset +
                     (TECMO_ASSET_PACK_ARENA_LOWER_R1_TABLE_CPU - 0xC000U) +
                     TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX)] = 4U;
    }
    {
        uint64_t bank00_offset = prg_offset;
        uint64_t bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint8_t *pointer_table = rom + (size_t)(bank00_offset +
                                                (TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU -
                                                 TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE));
        uint8_t *jumbotron = rom + (size_t)(bank00_offset + (0xA7E3U - 0x8000U));
        uint8_t *goal = rom + (size_t)(bank00_offset + (0xA8C0U - 0x8000U));

        tecmo_asset_pack_store_u16(pointer_table + 0U, 0xA7E3U);
        tecmo_asset_pack_store_u16(pointer_table + 2U, 0xA8C0U);
        jumbotron[0] = TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT;
        for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT; ++index) {
            uint8_t *record = jumbotron + 1U + index * 4U;
            record[0] = (uint8_t)(0x10U + index);
            record[1] = (uint8_t)(0x20U + index % 0x40U);
            record[2] = (uint8_t)(index & 0x03U);
            if ((index & 1U) != 0U) record[2] |= 0x20U;
            if ((index & 2U) != 0U) record[2] |= 0x40U;
            if ((index & 4U) != 0U) record[2] |= 0x80U;
            record[3] = (uint8_t)(0x20U + index * 3U);
        }
        jumbotron[1U + 1U] = 0x21U;
        jumbotron[1U + 2U] = 0xE3U;

        goal[0] = TECMO_ASSET_PACK_ARENA_GOAL_COUNT;
        for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_GOAL_COUNT; ++index) {
            uint8_t *record = goal + 1U + index * 4U;
            record[0] = (uint8_t)(0xC0U + (index % 4U) * 0x10U);
            record[1] = (uint8_t)(0x40U + index * 2U);
            record[2] = (uint8_t)(index & 0x03U);
            if ((index & 1U) != 0U) record[2] |= 0x20U;
            if ((index & 2U) != 0U) record[2] |= 0x80U;
            record[3] = (uint8_t)(0xF0U + index);
        }
        goal[1U + 2U] = 0x42U;
        goal[1U + 7U * 4U + 0U] = TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_Y;
        goal[1U + 7U * 4U + 1U] = 0x36U;
        goal[1U + 7U * 4U + 2U] = 0x02U;
        goal[1U + 7U * 4U + 3U] = TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_X;
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PALETTE_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_palette,
               sizeof(arena_sprite_palette));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_SEEDS_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_seeds,
               sizeof(arena_sprite_seeds));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PARAMS_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_params,
               sizeof(arena_sprite_params));
    }
    {
        static const uint8_t ready_script[TECMO_ASSET_PACK_READY_SCRIPT_SIZE] = {
            0x00U,0x00U,0x00U,0x00U, 0x08U,0x00U,0x00U,0x00U,
            0x0CU,0x02U,0x00U,0x00U, 0x08U,0x0BU,0x00U,0x00U,
            0x04U,0x0EU,0x02U,0x00U, 0x00U,0x09U,0x0BU,0x00U,
            0x00U,0x04U,0x0EU,0x02U, 0x00U,0x00U,0x09U,0x0BU,
            0x00U,0x00U,0x04U,0x0EU, 0x00U,0x00U,0x00U,0x09U,
            0x00U,0x00U,0x00U,0x04U, 0x00U,0x00U,0x00U,0x00U
        };
        static const uint16_t wordmark_glyph_cpu[TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT] = {
            0xAF55U, 0xAF05U, 0xAF41U, 0xAF41U,
            0xAF21U, 0xAF39U, 0xAF41U, 0xAF45U
        };
        uint64_t fixed_offset = prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank00_offset = prg_offset;
        uint64_t bank01_offset = prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank02_offset = prg_offset + 2ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint8_t *ready_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_READY_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *warriors_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_WARRIORS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *clippers_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_CLIPPERS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *bucks_descriptor = rom + (size_t)(fixed_offset + (0xDD34U - 0xC000U));
        uint8_t *pass_descriptor = rom + (size_t)(fixed_offset + (0xDD50U - 0xC000U));
        uint8_t *ready_stream = rom + (size_t)(bank04_offset + (0xBAF2U - 0x8000U));
        uint8_t *warriors_stream = rom + (size_t)(bank01_offset + (0xB4C5U - 0x8000U));
        uint8_t *clippers_stream = rom + (size_t)(bank01_offset + (0xB5EBU - 0x8000U));
        uint8_t *bucks_stream = rom + (size_t)(bank01_offset + (0xB401U - 0x8000U));
        uint8_t *pass_stream = rom + (size_t)(bank01_offset + (0xB933U - 0x8000U));
        size_t out = 0U;

        for (size_t i = 0U;
             i < sizeof(POST_ARENA_BANK04_CONTRACT) / sizeof(POST_ARENA_BANK04_CONTRACT[0]);
             ++i) {
            rom[(size_t)(bank04_offset +
                         (POST_ARENA_BANK04_CONTRACT[i].cpu - 0x8000U))] =
                POST_ARENA_BANK04_CONTRACT[i].value;
        }
        for (size_t i = 0U;
             i < sizeof(POST_ARENA_FIXED_CONTRACT) / sizeof(POST_ARENA_FIXED_CONTRACT[0]);
             ++i) {
            rom[(size_t)(fixed_offset +
                         (POST_ARENA_FIXED_CONTRACT[i].cpu - 0xC000U))] =
                POST_ARENA_FIXED_CONTRACT[i].value;
        }
        rom[(size_t)(fixed_offset + (0xCAF5U - 0xC000U) + 0x37U)] = 6U;
        rom[(size_t)(fixed_offset + (0xCB33U - 0xC000U) + 0x37U)] = 0xAEU;
        rom[(size_t)(fixed_offset + (0xCB71U - 0xC000U) + 0x37U)] = 0x9EU;
        {
            static const uint8_t text_routine[10] = {
                0xBDU,0x60U,0xADU,0x85U,0x16U,0xBDU,0x61U,0xADU,0x85U,0x17U
            };
            static const uint8_t text_record[9] = {
                8U,'C','L','I','P','P','E','R','S'
            };
            static const uint8_t chars[8] = {'C','L','I','P','P','E','R','S'};
            static const uint8_t maps[8] = {2U,10U,7U,14U,14U,4U,15U,16U};
            static const uint8_t glyphs[8][4] = {
                {0xBAU,0xBBU,0xBCU,0xBDU},{0xD0U,0xFFU,0xC0U,0xD1U},
                {0xC7U,0xC8U,0xC9U,0xCAU},{0xB6U,0xB7U,0xB4U,0xD8U},
                {0xB6U,0xB7U,0xB4U,0xD8U},{0xB6U,0xC2U,0xB8U,0xC3U},
                {0xB6U,0xB7U,0xB4U,0xCFU},{0xD9U,0xDAU,0xDBU,0xB9U}
            };
            memcpy(rom + (size_t)(bank06_offset + (0x9EAEU - 0x8000U)),
                   text_routine,
                   sizeof(text_routine));
            tecmo_asset_pack_store_u16(rom + (size_t)(bank06_offset + (0xAD60U - 0x8000U) + 0x16U),
                      0xACA3U);
            memcpy(rom + (size_t)(bank06_offset + (0xACA3U - 0x8000U)),
                   text_record,
                   sizeof(text_record));
            for (size_t i = 0U; i < 8U; ++i) {
                rom[(size_t)(bank06_offset + (0xA273U - 0x8000U) + chars[i])] = maps[i];
                memcpy(rom + (size_t)(bank06_offset + (0xAF05U - 0x8000U) + maps[i] * 4U),
                       glyphs[i],
                       4U);
            }
        }

        ready_descriptor[0] = 0U;
        ready_descriptor[1] = 1U;
        tecmo_asset_pack_store_u16(ready_descriptor + 2U, 0xBD61U);
        tecmo_asset_pack_store_u16(ready_descriptor + 4U, 0xBAF2U);
        ready_descriptor[6] = 4U;
        warriors_descriptor[0] = 0U;
        warriors_descriptor[1] = 1U;
        tecmo_asset_pack_store_u16(warriors_descriptor + 2U, 0xB5DBU);
        tecmo_asset_pack_store_u16(warriors_descriptor + 4U, 0xB4C5U);
        warriors_descriptor[6] = 1U;
        clippers_descriptor[0] = 0x16U;
        clippers_descriptor[1] = 0x17U;
        tecmo_asset_pack_store_u16(clippers_descriptor + 2U, 0xB7C4U);
        tecmo_asset_pack_store_u16(clippers_descriptor + 4U, 0xB5EBU);
        clippers_descriptor[6] = 1U;
        memcpy(bucks_descriptor, "\x2F\x30\xA5\xB4\x01\xB4\x01", 7U);
        memcpy(pass_descriptor, "\x78\x79\xA9\xBA\x33\xB9\x01", 7U);
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_READY_SCRIPT_CPU - 0x8000U)),
               ready_script,
               sizeof(ready_script));
        memcpy(rom + (size_t)(bank04_offset + (0xBD61U - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));
        memcpy(rom + (size_t)(bank01_offset + (0xB5DBU - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_WARRIORS_SPRITE_PALETTE_CPU - 0x8000U)),
               arena_sprite_palette,
               sizeof(arena_sprite_palette));

        ready_stream[out++] = 0xA9U;
        ready_stream[out++] = 34U;
        for (size_t i = 0U; i < 34U; ++i) ready_stream[out++] = 0x7FU;
        for (size_t i = 0U; i < 3U; ++i) {
            ready_stream[out++] = 128U;
            ready_stream[out++] = 0x7FU;
        }
        ready_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 3U; ++i) {
            ready_stream[out++] = 128U;
            ready_stream[out++] = 0x7FU;
        }
        ready_stream[out++] = 222U;
        ready_stream[out++] = 0x7FU;
        ready_stream[out++] = 0U;
        ready_stream[out++] = 0U;
        if (out != 53U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test READY stream size mismatch.");
            goto cleanup;
        }

        out = 0U;
        warriors_stream[out++] = 0xA5U;
        warriors_stream[out++] = 129U;
        for (size_t i = 0U; i < 129U; ++i) warriors_stream[out++] = 0x7FU;
        warriors_stream[out++] = 130U;
        for (size_t i = 0U; i < 130U; ++i) warriors_stream[out++] = 0x7FU;
        for (size_t i = 0U; i < 2U; ++i) {
            warriors_stream[out++] = 246U;
            warriors_stream[out++] = 0x7FU;
        }
        warriors_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 4U; ++i) {
            warriors_stream[out++] = 246U;
            warriors_stream[out++] = 0x7FU;
        }
        warriors_stream[out++] = 0x02U;
        warriors_stream[out++] = 249U;
        warriors_stream[out++] = 0x7FU;
        warriors_stream[out++] = 0U;
        if (out != 279U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test WARRIORS stream size mismatch.");
            goto cleanup;
        }

        out = 0U;
        clippers_stream[out++] = 0xA5U;
        clippers_stream[out++] = 226U;
        for (size_t i = 0U; i < 226U; ++i) clippers_stream[out++] = (uint8_t)i;
        clippers_stream[out++] = 228U;
        for (size_t i = 0U; i < 228U; ++i) clippers_stream[out++] = (uint8_t)(i + 1U);
        for (size_t i = 0U; i < 2U; ++i) {
            clippers_stream[out++] = 227U;
            clippers_stream[out++] = 0x7FU;
        }
        clippers_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 4U; ++i) {
            clippers_stream[out++] = 227U;
            clippers_stream[out++] = 0x7FU;
        }
        clippers_stream[out++] = 0x02U;
        clippers_stream[out++] = 232U;
        clippers_stream[out++] = 0x7FU;
        clippers_stream[out++] = 0U;
        if (out != 474U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test CLIPPERS stream size mismatch.");
            goto cleanup;
        }
        memcpy(rom + (size_t)(bank01_offset + (0xB7C4U - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));

        {
            static const uint8_t compact_two_page_stream[20] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t bucks_palette[16] = {
                0x0FU,0x17U,0x27U,0x01U,0x0FU,0x17U,0x16U,0x26U,
                0x0FU,0x17U,0x27U,0x01U,0x0FU,0x17U,0x27U,0x11U
            };
            static const uint8_t pass_palette[16] = {
                0x0FU,0x17U,0x27U,0x37U,0x0FU,0x0FU,0x16U,0x27U,
                0x0FU,0x0CU,0x01U,0x30U,0x0FU,0x0CU,0x17U,0x27U
            };
            static const uint8_t helper_palette[16] = {
                0x02U,0x01U,0x11U,0x21U,0x02U,0x17U,0x16U,0x26U,
                0x02U,0x0FU,0x09U,0x30U,0x02U,0x0FU,0x00U,0x30U
            };
            static const uint8_t special_palette[16] = {
                0x0FU,0x01U,0x11U,0x21U,0x0FU,0x0FU,0x16U,0x27U,
                0x0FU,0x0CU,0x01U,0x30U,0x0FU,0x0CU,0x17U,0x27U
            };
            static const uint8_t bucks_route[] = {
                0xA9U,0x19U,0x20U,0xF2U,0x82U,0xA9U,0x0BU,0x20U,0x6EU,0x8AU
            };
            static const uint8_t pass_route[] = {
                0xAEU,0xE9U,0x07U,0xBDU,0x3CU,0x86U,0x20U,0xF2U,0x82U,
                0xA9U,0x0DU,0x20U,0x6EU,0x8AU,0xA2U,0xFDU,0xA0U,0x89U
            };
            static const uint8_t pass_emit_helper[] = {
                0x85U,0x09U,0x84U,0x0BU,0xA9U,0x00U,0x85U,0x2DU,0x85U,0x2CU,
                0xA9U,0x01U,0x85U,0x0DU,0x85U,0x2EU,0xA9U,0x00U,0xA2U,0x0FU,
                0xA0U,0xA9U,0x4CU,0x51U,0xC0U
            };
            static const uint8_t thresholds[6] = {0xEFU,0xC0U,0x90U,0x60U,0x30U,0x00U};
            static const uint8_t bucks_record[6] = {5U,'B','U','C','K','S'};
            static const uint8_t bucks_chars[5] = {'B','U','C','K','S'};
            memcpy(bucks_stream, compact_two_page_stream, sizeof(compact_two_page_stream));
            memcpy(pass_stream, compact_two_page_stream, sizeof(compact_two_page_stream));
            memcpy(rom + (size_t)(bank01_offset + (0xB4A5U - 0x8000U)), bucks_palette, 16U);
            memcpy(rom + (size_t)(bank01_offset + (0xBAA9U - 0x8000U)), pass_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x89FDU - 0x8000U)), helper_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x8A1DU - 0x8000U)), special_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x883DU - 0x8000U)), bucks_route, sizeof(bucks_route));
            memcpy(rom + (size_t)(bank04_offset + (0x854FU - 0x8000U)), pass_route, sizeof(pass_route));
            memcpy(rom + (size_t)(bank04_offset + (0x8645U - 0x8000U)),
                   pass_emit_helper,
                   sizeof(pass_emit_helper));
            memcpy(rom + (size_t)(bank04_offset + (0x88A3U - 0x8000U)), thresholds, sizeof(thresholds));
            rom[(size_t)(fixed_offset + (0xDC19U - 0xC000U) + 0x0EU)] = 0x2AU;
            tecmo_asset_pack_store_u16(rom + (size_t)(bank06_offset + (0xAD60U - 0x8000U) + 0x0EU * 2U), 0xACB8U);
            memcpy(rom + (size_t)(bank06_offset + (0xACB8U - 0x8000U)), bucks_record, sizeof(bucks_record));
            for (size_t i = 0U; i < sizeof(bucks_chars); ++i) {
                uint8_t map = (uint8_t)(20U + i);
                rom[(size_t)(bank06_offset + (0xA273U - 0x8000U) + bucks_chars[i])] = map;
                memset(rom + (size_t)(bank06_offset + (0xAF05U - 0x8000U) + map * 4U),
                       (int)(0x80U + i * 4U),
                       4U);
            }
        }

        tecmo_asset_pack_store_u16(rom + (size_t)(bank00_offset +
                                 (TECMO_ASSET_PACK_WARRIORS_POINTER_CPU - 0x8000U)),
                   TECMO_ASSET_PACK_WARRIORS_STREAM_CPU);
        tecmo_asset_pack_store_u16(rom + (size_t)(bank00_offset +
                                 (TECMO_ASSET_PACK_PASS_POINTER_CPU - 0x8000U) + 2U),
                   TECMO_ASSET_PACK_PASS_STREAM_CPU);
        {
            uint8_t *pieces = rom + (size_t)(bank00_offset +
                (TECMO_ASSET_PACK_WARRIORS_STREAM_CPU - 0x8000U));
            pieces[0] = TECMO_ASSET_PACK_WARRIORS_PIECE_COUNT;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_WARRIORS_PIECE_COUNT; ++i) {
                pieces[1U + i * 4U + 0U] = (uint8_t)(i * 3U);
                pieces[1U + i * 4U + 1U] = (uint8_t)((i * 2U) & 0xFCU);
                pieces[1U + i * 4U + 2U] = (uint8_t)(i & 3U);
                pieces[1U + i * 4U + 3U] = (uint8_t)(i * 5U);
            }
        }
        {
            uint8_t *pieces = rom + (size_t)(bank00_offset +
                (TECMO_ASSET_PACK_PASS_STREAM_CPU - 0x8000U));
            pieces[0] = TECMO_ASSET_PACK_PASS_PIECE_COUNT;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_PASS_PIECE_COUNT; ++i) {
                pieces[1U + i * 4U + 0U] = (uint8_t)((i / 3U) * 8U);
                pieces[1U + i * 4U + 1U] = (uint8_t)((i * 2U) & 0x3EU);
                pieces[1U + i * 4U + 2U] = 1U;
                pieces[1U + i * 4U + 3U] = (uint8_t)((i % 3U) * 8U);
            }
        }
        {
            static const uint8_t finale_screen_ids[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x1CU,0x20U,0x1FU,0x22U,0x2DU
            };
            static const uint16_t finale_stream_cpu[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x9000U,0x9020U,0x9040U,0x9060U,0x9080U
            };
            static const uint16_t finale_palette_cpu[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x9100U,0x9120U,0x9140U,0x9160U,0x9180U
            };
            static const uint8_t one_page_stream[11] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t two_page_stream[20] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t finale_sprite_palette[16] = {
                0x02U,0x01U,0x11U,0x21U,0x02U,0x02U,0x12U,0x22U,
                0x02U,0x03U,0x13U,0x23U,0x02U,0x04U,0x14U,0x24U
            };
            uint8_t *finale_bank04 = rom + (size_t)bank04_offset;
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DBU - 0x8000U), 0x851CU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DDU - 0x8000U), 0x83EAU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DFU - 0x8000U), 0x852EU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E1U - 0x8000U), 0x83AEU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E3U - 0x8000U), 0x8310U);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E5U - 0x8000U), 0xFFFFU);
            finale_bank04[0x82EDU - 0x8000U] = 50U;
            finale_bank04[0x82EEU - 0x8000U] = 30U;
            finale_bank04[0x82EFU - 0x8000U] = 0U;
            finale_bank04[0x82F0U - 0x8000U] = 75U;
            finale_bank04[0x82F1U - 0x8000U] = 1U;
            finale_bank04[0x852FU - 0x8000U] = 2U;
            for (size_t selector = 0U; selector < 3U; ++selector) {
                uint8_t value = (uint8_t)(0x91U + selector * 2U);
                uint8_t destination = (uint8_t)(0x57U + selector);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x83C2U + selector * 4U),
                                             value,
                                             destination);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x8402U + selector * 4U),
                                             value,
                                             destination);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x8569U + selector * 4U),
                                             value,
                                             destination);
            }
            for (size_t anchor = 0U; anchor < 4U; ++anchor) {
                finale_bank04[0x8463U - 0x8000U + anchor] =
                    (uint8_t)(0x80U + anchor * 8U);
                finale_bank04[0x8473U - 0x8000U + anchor] =
                    (uint8_t)(0x40U + anchor * 16U);
            }
            finale_bank04[0x83D8U - 0x8000U] = 0x2CU;
            finale_bank04[0x83DAU - 0x8000U] = 0x30U;
            for (size_t screen = 0U; screen < TECMO_ASSET_PACK_FINALE_SCREEN_COUNT; ++screen) {
                uint8_t *descriptor = rom + (size_t)(fixed_offset +
                    (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                    (uint64_t)finale_screen_ids[screen] * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
                uint8_t selector = (uint8_t)(screen * 2U);
                descriptor[0] = selector;
                descriptor[1] = selector;
                tecmo_asset_pack_store_u16(descriptor + 2U, finale_palette_cpu[screen]);
                tecmo_asset_pack_store_u16(descriptor + 4U, finale_stream_cpu[screen]);
                descriptor[6U] = 2U;
                memcpy(rom + (size_t)(bank02_offset + finale_stream_cpu[screen] - 0x8000U),
                       screen == 0U || screen == 3U ? one_page_stream : two_page_stream,
                       screen == 0U || screen == 3U ? sizeof(one_page_stream) : sizeof(two_page_stream));
                for (size_t color = 0U; color < 16U; ++color) {
                    rom[(size_t)(bank02_offset + finale_palette_cpu[screen] - 0x8000U + color)] =
                        (uint8_t)((color + screen) & 0x3FU);
                }
            }
            memcpy(rom + (size_t)(bank04_offset +
                                  (TECMO_ASSET_PACK_FINALE_SHORT_PALETTE_CPU - 0x8000U)),
                   finale_sprite_palette,
                   sizeof(finale_sprite_palette));
            rom[(size_t)(bank04_offset + (0x863EU - 0x8000U))] = 0x1FU;
            rom[(size_t)(bank04_offset + (0x8640U - 0x8000U))] = 0x78U;
            rom[(size_t)(bank04_offset + (0x8642U - 0x8000U))] = 0xD8U;
            rom[(size_t)(bank04_offset + (0x8644U - 0x8000U))] = 0xF8U;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_SIZE; ++i) {
                uint8_t code = (uint8_t)(0x41U + i);
                uint8_t map = (uint8_t)(32U + i);
                rom[(size_t)(bank04_offset +
                             (TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_CPU - 0x8000U) + i)] = code;
                rom[(size_t)(bank06_offset +
                             (TECMO_ASSET_PACK_FINALE_GLYPH_MAP_CPU - 0x8000U) + code)] = map;
                for (size_t tile = 0U; tile < 4U; ++tile) {
                    rom[(size_t)(bank06_offset +
                                 (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U) +
                                 (uint64_t)map * 4U + tile)] =
                        (uint8_t)(0x80U + ((i * 4U + tile) & 0x3FU));
                }
            }
            for (size_t tile = 0U; tile < 4U; ++tile) {
                rom[(size_t)(bank06_offset +
                             (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U) +
                             0x18U * 4U + tile)] = (uint8_t)(0x80U + tile);
            }
        }
        memset(rom + (size_t)(bank01_offset +
                              (TECMO_ASSET_PACK_WARRIORS_PATCH0_CPU - 0x8000U)),
               0x80,
               64U);
        memset(rom + (size_t)(bank01_offset +
                              (TECMO_ASSET_PACK_WARRIORS_PATCH1_CPU - 0x8000U)),
               0x81,
               64U);
        for (size_t glyph = 0U;
             glyph < TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT;
             ++glyph) {
            uint8_t *source = rom + (size_t)(bank06_offset +
                                             (wordmark_glyph_cpu[glyph] - 0x8000U));
            source[0] = (uint8_t)(0x80U + glyph * 4U);
            source[1] = (uint8_t)(0x81U + glyph * 4U);
            source[2] = (uint8_t)(0x82U + glyph * 4U);
            source[3] = (uint8_t)(0x83U + glyph * 4U);
        }
    }

    if (self_test_arena_sprite_group_validation(rom,
                                                rom_size,
                                                prg_offset,
                                                chr_offset,
                                                chr_size,
                                                message,
                                                message_size) != 0) {
        goto cleanup;
    }

    if (write_self_test_file(rom_path, rom, (size_t)rom_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not write temporary iNES ROM.");
        goto cleanup;
    }
    if (tecmo_asset_pack_build_from_ines_internal(rom_path,
                                                  ines_pack_path,
                                                  0,
                                                  message,
                                                  message_size) != 0) {
        goto cleanup;
    }

    if (self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "format=tecmo.assetpack/1",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "prg_banks_16k=8",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "chr_banks_8k=32",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "logical_entry_prefixes=arena/intro/",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "input_contract=ines-only",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"format\":\"tecmo.assetpack.source-map/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"input_contract\":\"ines-only\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_PRESENTS_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_LICENSE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"palette_frames\":[0,4,8,12,16,123,125,127,129]",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"encoded_size\":116,\"decoded_size\":1024",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"screen_id\":24,\"decoder_cpu_address\":55798",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":56621",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"encoded_size\":28,\"decoded_size\":2048",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":64892,\"selector_cpu_address\":64893",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":64896,\"selector_cpu_address\":64897",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"schema\":\"tecmo.arena-intro.sprite-groups/TASG-2\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"pointer_table\":{\"source_entry\":\"prg/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"record_count\":55",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"record_count\":16",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"emitter\":{\"source_entry\":\"prg/bank04\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"chr_pages\":[{\"source_entry\":\"chr/bank01\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"prg/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"prg/bank01\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"chr/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_FINALE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"schema\":\"tecmo.intro.finale/TFIN-1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"role\":\"selector-two-indexed-operands\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"title_slots\":44,\"title_source_slots\":26,\"blank_slots\":18",
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }

    if (self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                      "\"format\":\"tecmo.arena-intro.script/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID,
                                      "\"format\":\"tecmo.arena-intro.palette-cycle/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                      "\"source_route\":\"bank04:C-0127\"",
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }
    if (self_test_arena_background_layer(ines_pack_path,
                                         arena_palette,
                                         message,
                                         message_size) != 0) {
        goto cleanup;
    }
    if (self_test_arena_sprite_groups(ines_pack_path,
                                      expected_sprite_palette,
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }

    if (self_test_read_and_compare(ines_pack_path,
                                   "prg/bank00",
                                   rom + (size_t)prg_offset,
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "prg/bank01",
                                   rom + (size_t)(prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES),
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "prg/fixed",
                                   rom + (size_t)(prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES),
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/all",
                                   rom + (size_t)chr_offset,
                                   chr_size,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/bank00",
                                   rom + (size_t)chr_offset,
                                   TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&ines_list_state, 0, sizeof(ines_list_state));
    if (tecmo_asset_pack_list_entries(ines_pack_path,
                                      self_test_ines_list_entry,
                                      &ines_list_state) != 0 ||
        ines_list_state.count != 56U ||
        !ines_list_state.saw_manifest ||
        !ines_list_state.saw_source_map ||
        !ines_list_state.saw_presents ||
        !ines_list_state.presents_metadata_valid ||
        !ines_list_state.saw_license ||
        !ines_list_state.license_metadata_valid ||
        !ines_list_state.saw_arena_intro_script ||
        !ines_list_state.saw_arena_intro_background ||
        !ines_list_state.arena_intro_background_metadata_valid ||
        !ines_list_state.saw_arena_intro_palette ||
        !ines_list_state.saw_arena_intro_sprite_groups ||
        !ines_list_state.arena_intro_sprite_groups_metadata_valid ||
        !ines_list_state.saw_ready ||
        !ines_list_state.ready_metadata_valid ||
        !ines_list_state.saw_warriors ||
        !ines_list_state.warriors_metadata_valid ||
        !ines_list_state.saw_clippers ||
        !ines_list_state.clippers_metadata_valid ||
        !ines_list_state.saw_bucks ||
        !ines_list_state.bucks_metadata_valid ||
        !ines_list_state.saw_pass ||
        !ines_list_state.pass_metadata_valid ||
        !ines_list_state.saw_finale ||
        !ines_list_state.finale_metadata_valid ||
        !ines_list_state.saw_prg_bank0 ||
        !ines_list_state.saw_prg_bank1 ||
        !ines_list_state.saw_prg_fixed ||
        !ines_list_state.saw_chr_all ||
        !ines_list_state.saw_chr_bank0 ||
        !ines_list_state.saw_chr_bank1) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test iNES pack directory enumeration mismatch.");
        goto cleanup;
    }

    tecmo_asset_pack_set_message(message, message_size, "Asset pack self-test passed.");
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(dump);
    free(rom);
    remove_self_test_file(builder_pack_path);
    remove_self_test_file(local_file_path);
    remove_self_test_file(rom_path);
    remove_self_test_file(ines_pack_path);
    remove_self_test_dir(temp_dir);
    return result;
}
