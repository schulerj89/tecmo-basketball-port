#include "tecmo_asset_pack_arena.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

int tecmo_asset_pack_build_arena_background_layer(const uint8_t *rom,
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
int tecmo_asset_pack_build_arena_sprite_groups(const uint8_t *rom,
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
