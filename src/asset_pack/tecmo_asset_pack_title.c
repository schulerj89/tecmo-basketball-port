#include "tecmo_asset_pack_title.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

static int title_range(uint64_t offset, uint64_t size, uint64_t rom_size)
{
    return offset <= rom_size && size <= rom_size - offset;
}

static void build_cells(uint8_t *payload,
                        const uint8_t decoded[1024],
                        uint64_t chr_size)
{
    for (size_t i = 0U; i < TECMO_ASSET_PACK_TITLE_CELL_COUNT; ++i) {
        unsigned row = (unsigned)(i / TECMO_ASSET_PACK_TITLE_WIDTH);
        unsigned col = (unsigned)(i % TECMO_ASSET_PACK_TITLE_WIDTH);
        uint8_t tile = decoded[i];
        uint8_t selector = tile < 0x80U ? TECMO_ASSET_PACK_TITLE_BG_R0
                                       : TECMO_ASSET_PACK_TITLE_BG_R1;
        uint32_t chr_offset = (uint32_t)selector * 1024U +
                              (uint32_t)(tile & 0x7FU) * 16U;
        uint8_t *cell = payload + TECMO_ASSET_PACK_TITLE_CELLS_OFFSET +
                        i * TECMO_ASSET_PACK_TITLE_CELL_STRIDE;
        (void)chr_size;
        cell[0] = tile;
        cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
        tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
    }
}

int tecmo_asset_pack_build_title_asset(const uint8_t *rom,
                                       uint64_t rom_size,
                                       uint64_t prg_offset,
                                       uint32_t prg_banks,
                                       uint64_t chr_size,
                                       unsigned kind,
                                       int enforce_revision_fingerprints,
                                       uint8_t *payload,
                                       size_t payload_size,
                                       TecmoTitleProvenance *provenance,
                                       char *message,
                                       size_t message_size)
{
    uint8_t decoded[1024];
    uint32_t fixed_bank;
    uint64_t fixed_offset;
    uint64_t bank00_offset = prg_offset;
    uint64_t bank01_offset = prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank03_offset = prg_offset + 3U * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint8_t screen_id = kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_SCREEN_ID
                                   : TECMO_ASSET_PACK_TITLE_START_SCREEN_ID;
    uint32_t stream_cpu = kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_STREAM_CPU
                                     : TECMO_ASSET_PACK_TITLE_START_STREAM_CPU;
    size_t expected_stream = kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_STREAM_SIZE
                                        : TECMO_ASSET_PACK_TITLE_START_STREAM_SIZE;
    uint64_t descriptor_offset;
    uint64_t stream_offset;
    uint64_t palette_offset;
    uint64_t sprite_palette_offset;
    uint64_t sprite_table_offset;
    uint64_t attr_a_offset;
    uint64_t attr_b_offset;
    uint64_t prompt_blank_offset;
    uint64_t prompt_visible_offset;
    size_t encoded_size = 0U;
    const uint8_t *descriptor;

    if (rom == NULL || payload == NULL || provenance == NULL || kind > 1U ||
        prg_banks < 4U || chr_size == 0U ||
        payload_size != (kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_SIZE
                                    : TECMO_ASSET_PACK_TITLE_SCREEN_SIZE)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "Title import requires Rev1 PRG/CHR data and its exact payload layout.");
        return -1;
    }
    fixed_bank = prg_banks - 1U;
    fixed_offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    descriptor_offset = fixed_offset + (0xDC85U - 0xC000U) + (uint64_t)screen_id * 7U;
    stream_offset = bank00_offset + (stream_cpu - 0x8000U);
    palette_offset = bank00_offset + (TECMO_ASSET_PACK_TITLE_PALETTE_CPU - 0x8000U);
    sprite_palette_offset = bank00_offset + (TECMO_ASSET_PACK_TITLE_SPRITE_PALETTE_CPU - 0x8000U);
    sprite_table_offset = bank01_offset + (TECMO_ASSET_PACK_TITLE_SPRITE_TABLE_CPU - 0x8000U);
    attr_a_offset = bank00_offset + (TECMO_ASSET_PACK_TITLE_ATTR_A_CPU - 0x8000U);
    attr_b_offset = bank00_offset + (TECMO_ASSET_PACK_TITLE_ATTR_B_CPU - 0x8000U);
    prompt_blank_offset = bank03_offset + (TECMO_ASSET_PACK_TITLE_PROMPT_BLANK_CPU - 0x8000U);
    prompt_visible_offset = bank03_offset + (TECMO_ASSET_PACK_TITLE_PROMPT_VISIBLE_CPU - 0x8000U);
    if (!title_range(descriptor_offset, 7U, rom_size) ||
        !title_range(stream_offset, expected_stream, rom_size) ||
        !title_range(palette_offset, kind == 0U ? 32U : 16U, rom_size) ||
        (kind == 0U && (!title_range(attr_a_offset, 16U, rom_size) ||
                       !title_range(attr_b_offset, 16U, rom_size))) ||
        (kind == 1U && !title_range(prompt_blank_offset, 20U, rom_size))) {
        tecmo_asset_pack_set_message(message, message_size, "Title source range is outside PRG ROM.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    if (descriptor[0] != TECMO_ASSET_PACK_TITLE_BG_R0 / 2U ||
        descriptor[1] != TECMO_ASSET_PACK_TITLE_BG_R1 / 2U ||
        tecmo_asset_pack_read_u16(descriptor + 2U) != TECMO_ASSET_PACK_TITLE_PALETTE_CPU ||
        tecmo_asset_pack_read_u16(descriptor + 4U) != stream_cpu || descriptor[6] != 0U ||
        tecmo_asset_pack_validate_chr_pair(TECMO_ASSET_PACK_TITLE_BG_R0,
                                           TECMO_ASSET_PACK_TITLE_BG_R1,
                                           chr_size, "title background", message,
                                           message_size) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)bank00_offset,
                                            TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                            stream_cpu - 0x8000U,
                                            decoded, sizeof(decoded), &encoded_size,
                                            message, message_size) != 0 ||
        encoded_size != expected_stream) {
        tecmo_asset_pack_set_message(message, message_size, "Title descriptor or compressed screen contract rejected.");
        return -1;
    }
    if (enforce_revision_fingerprints != 0) {
        int mismatch = kind == 0U
            ? tecmo_asset_pack_fnv1a32(descriptor, 7U) != 0xCA34236AU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset, encoded_size) != 0x361CBF03U ||
              tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) != 0xD3C29A45U ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)palette_offset, 32U) != 0x8AD8A04FU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)attr_a_offset, 32U) != 0xE325475FU
            : tecmo_asset_pack_fnv1a32(descriptor, 7U) != 0x4545B29AU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset, encoded_size) != 0x2A81D89AU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)palette_offset, 16U) != 0xBBF7850BU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)prompt_blank_offset, 20U) != 0xA7829917U;
        if (mismatch) {
            tecmo_asset_pack_set_message(message, message_size, "Title Rev1 fingerprint mismatch.");
            return -1;
        }
    }

    memset(payload, 0, payload_size);
    memcpy(payload, kind == 0U ? "TATR" : "TTLE", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, kind == 0U ? 2U : 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_TITLE_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, TECMO_ASSET_PACK_TITLE_WIDTH);
    tecmo_asset_pack_store_u16(payload + 10U, TECMO_ASSET_PACK_TITLE_HEIGHT);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_TITLE_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 14U, (uint16_t)kind);
    tecmo_asset_pack_store_u32(payload + 16U, TECMO_ASSET_PACK_TITLE_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_TITLE_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_TITLE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, (uint32_t)payload_size);
    tecmo_asset_pack_store_u32(payload + 32U, kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_SPRITES_OFFSET
                                                         : TECMO_ASSET_PACK_TITLE_PROMPT_ROWS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 36U, kind == 0U ? TECMO_ASSET_PACK_TITLE_SPRITE_COUNT
                                                         : TECMO_ASSET_PACK_TITLE_PROMPT_COUNT);
    tecmo_asset_pack_store_u16(payload + 38U, kind == 0U ? TECMO_ASSET_PACK_TITLE_SPRITE_STRIDE : 1U);
    tecmo_asset_pack_store_u32(payload + 40U, kind == 0U ? TECMO_ASSET_PACK_TITLE_ATTRACT_ATTRS_OFFSET
                                                         : 0U);
    tecmo_asset_pack_store_u16(payload + 44U, kind == 0U ? 642U : 127U);
    tecmo_asset_pack_store_u16(payload + 46U, kind == 0U ? 621U : 126U);
    build_cells(payload, decoded, chr_size);
    memcpy(payload + TECMO_ASSET_PACK_TITLE_PALETTE_OFFSET,
           rom + (size_t)palette_offset, 16U);

    if (kind == 0U) {
        if (!title_range(sprite_palette_offset, 16U, rom_size) ||
            !title_range(sprite_table_offset, TECMO_ASSET_PACK_TITLE_SPRITE_TABLE_SIZE, rom_size) ||
            rom[(size_t)sprite_table_offset] != TECMO_ASSET_PACK_TITLE_SPRITE_COUNT ||
            (enforce_revision_fingerprints != 0 &&
             (tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_palette_offset, 16U) != 0xF85BA74AU ||
              tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_table_offset,
                                       TECMO_ASSET_PACK_TITLE_SPRITE_TABLE_SIZE) != 0x669E53D3U))) {
            tecmo_asset_pack_set_message(message, message_size, "Title attract sprite contract rejected.");
            return -1;
        }
        memcpy(payload + TECMO_ASSET_PACK_TITLE_PALETTE_OFFSET + 16U,
               rom + (size_t)palette_offset + 16U, 16U);
        memcpy(payload + TECMO_ASSET_PACK_TITLE_PALETTE_OFFSET + 32U,
               rom + (size_t)sprite_palette_offset, 16U);
        for (size_t i = 0U; i < TECMO_ASSET_PACK_TITLE_SPRITE_COUNT; ++i) {
            const uint8_t *record = rom + (size_t)sprite_table_offset + 1U + i * 4U;
            uint8_t runtime_tile = (uint8_t)(record[1] + 1U);
            uint8_t selector = (runtime_tile & 0x40U) != 0U ? 0xF5U : 0xF4U;
            uint32_t top = (uint32_t)selector * 1024U +
                           (uint32_t)(runtime_tile & 0x3EU) * 16U;
            uint8_t *piece = payload + TECMO_ASSET_PACK_TITLE_ATTRACT_SPRITES_OFFSET + i * 16U;
            if ((uint64_t)top + 32U > chr_size || (record[2] & 0x1CU) != 0U) {
                tecmo_asset_pack_set_message(message, message_size, "Title attract sprite resolves outside chr/all.");
                return -1;
            }
            tecmo_asset_pack_store_u16(piece + 0U, (uint16_t)(int16_t)(int8_t)record[3]);
            tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)(int16_t)(int8_t)record[0]);
            tecmo_asset_pack_store_u32(piece + 4U, top);
            tecmo_asset_pack_store_u32(piece + 8U, top + 16U);
            piece[12U] = record[2] & 3U;
            piece[13U] = (uint8_t)((record[2] >> 5U) & 7U);
        }
        memcpy(payload + TECMO_ASSET_PACK_TITLE_ATTRACT_ATTRS_OFFSET,
               rom + (size_t)attr_a_offset, 16U);
        memcpy(payload + TECMO_ASSET_PACK_TITLE_ATTRACT_ATTRS_OFFSET + 16U,
               rom + (size_t)attr_b_offset, 16U);
    } else {
        if (!title_range(prompt_blank_offset, 10U, rom_size) ||
            !title_range(prompt_visible_offset, 10U, rom_size)) {
            tecmo_asset_pack_set_message(message, message_size, "Title prompt rows are outside Bank03.");
            return -1;
        }
        memcpy(payload + TECMO_ASSET_PACK_TITLE_PROMPT_ROWS_OFFSET,
               rom + (size_t)prompt_blank_offset, 10U);
        memcpy(payload + TECMO_ASSET_PACK_TITLE_PROMPT_ROWS_OFFSET + 10U,
               rom + (size_t)prompt_visible_offset, 10U);
    }

    memset(provenance, 0, sizeof(*provenance));
    provenance->descriptor_offset = descriptor_offset;
    provenance->stream_offset = stream_offset;
    provenance->stream_size = encoded_size;
    provenance->palette_offset = palette_offset;
    provenance->sprite_palette_offset = kind == 0U ? sprite_palette_offset : 0U;
    provenance->sprite_table_offset = kind == 0U ? sprite_table_offset : 0U;
    provenance->attr_a_offset = kind == 0U ? attr_a_offset : 0U;
    provenance->attr_b_offset = kind == 0U ? attr_b_offset : 0U;
    provenance->prompt_blank_offset = kind == 1U ? prompt_blank_offset : 0U;
    provenance->prompt_visible_offset = kind == 1U ? prompt_visible_offset : 0U;
    provenance->stream_cpu = stream_cpu;
    provenance->screen_id = screen_id;
    return 0;
}
