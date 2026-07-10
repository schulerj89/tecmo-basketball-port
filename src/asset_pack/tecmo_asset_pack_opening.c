#include "tecmo_asset_pack_opening.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

static uint8_t opening_fade_color(uint8_t color, int level)
{
    uint8_t maximum;
    uint8_t high;

    if (level < 0 || color == 0x0FU) return 0x0FU;
    maximum = (uint8_t)((unsigned int)level << 4U);
    high = (uint8_t)(color & 0x30U);
    if (high > maximum) high = maximum;
    return (uint8_t)(high | (color & 0x0FU));
}

static void build_opening_palette_stage(uint8_t *destination,
                                        const uint8_t *target,
                                        size_t palette_stride,
                                        int level)
{
    for (size_t i = 0U; i < palette_stride; ++i) {
        destination[i] = opening_fade_color(target[i], level);
    }
}

static void build_opening_fade_out_stage(uint8_t *destination,
                                         const uint8_t *target,
                                         size_t palette_stride,
                                         unsigned int steps)
{
    for (size_t i = 0U; i < palette_stride; ++i) {
        uint8_t color = target[i];
        for (unsigned int step = 0U; step < steps; ++step) {
            color = tecmo_asset_pack_imported_fade_color(color, 0x10U);
        }
        destination[i] = color;
    }
}

int tecmo_asset_pack_build_opening_screen(const uint8_t *rom,
                                uint64_t rom_size,
                                uint64_t prg_offset,
                                uint32_t prg_banks,
                                uint64_t chr_size,
                                unsigned int kind,
                                int enforce_revision_fingerprints,
                                uint8_t *payload,
                                size_t payload_size,
                                TecmoOpeningScreenProvenance *provenance,
                                char *message,
                                size_t message_size)
{
    static const uint16_t presents_frames[TECMO_ASSET_PACK_PRESENTS_STAGE_COUNT] = {
        0U, 4U, 8U, 12U, 16U, 123U, 125U, 127U, 129U
    };
    static const int8_t presents_levels[TECMO_ASSET_PACK_PRESENTS_STAGE_COUNT] = {
        -1, 0, 1, 2, 3, 0, 0, 0, 0
    };
    static const uint16_t license_frames[TECMO_ASSET_PACK_LICENSE_STAGE_COUNT] = {
        0U, 36U, 40U, 44U, 48U, 275U
    };
    static const int8_t license_levels[TECMO_ASSET_PACK_LICENSE_STAGE_COUNT] = {
        -1, 0, 1, 2, 3, -1
    };
    static const uint8_t selector_contract[7] = {
        0xA2U, 0xF4U, 0x86U, 0x57U, 0xE8U, 0x86U, 0x58U
    };
    uint8_t decoded[TECMO_ASSET_PACK_NAMETABLE_SIZE];
    uint8_t target_palette[TECMO_ASSET_PACK_PRESENTS_PALETTE_STRIDE];
    uint32_t fixed_bank;
    uint64_t fixed_offset;
    uint64_t bank00_offset;
    uint64_t descriptor_offset;
    uint64_t stream_offset;
    uint64_t palette_offset;
    uint64_t selector_code_offset;
    uint64_t sprite_table_offset = 0U;
    uint64_t sprite_palette_offset = 0U;
    uint64_t y_base_operand_offset = 0U;
    uint64_t tile_base_operand_offset = 0U;
    uint64_t x_base_operand_offset = 0U;
    const uint8_t *descriptor;
    const uint16_t *palette_frames;
    const int8_t *palette_levels;
    uint32_t screen_id;
    uint32_t stream_cpu;
    uint32_t expected_stream_size;
    uint16_t stage_count;
    uint16_t palette_stride;
    uint16_t duration;
    uint16_t sprite_count;
    uint16_t sprite_hide_frame;
    uint32_t palettes_offset = TECMO_ASSET_PACK_OPENING_SCREEN_PALETTES_OFFSET;
    uint32_t frames_offset;
    uint32_t sprites_offset;
    uint32_t total_size;
    size_t encoded_size = 0U;

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks == 0U ||
        kind > 1U || chr_size == 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Opening screen import requires Rev1 PRG and CHR ROM data.");
        return -1;
    }

    if (kind == 0U) {
        screen_id = TECMO_ASSET_PACK_PRESENTS_SCREEN_ID;
        stream_cpu = TECMO_ASSET_PACK_PRESENTS_STREAM_CPU;
        expected_stream_size = TECMO_ASSET_PACK_PRESENTS_STREAM_SIZE;
        stage_count = TECMO_ASSET_PACK_PRESENTS_STAGE_COUNT;
        palette_stride = TECMO_ASSET_PACK_PRESENTS_PALETTE_STRIDE;
        duration = TECMO_ASSET_PACK_PRESENTS_DURATION;
        sprite_count = TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT;
        sprite_hide_frame = TECMO_ASSET_PACK_PRESENTS_SPRITE_HIDE_FRAME;
        palette_frames = presents_frames;
        palette_levels = presents_levels;
    } else {
        screen_id = TECMO_ASSET_PACK_LICENSE_SCREEN_ID;
        stream_cpu = TECMO_ASSET_PACK_LICENSE_STREAM_CPU;
        expected_stream_size = TECMO_ASSET_PACK_LICENSE_STREAM_SIZE;
        stage_count = TECMO_ASSET_PACK_LICENSE_STAGE_COUNT;
        palette_stride = TECMO_ASSET_PACK_LICENSE_PALETTE_STRIDE;
        duration = TECMO_ASSET_PACK_LICENSE_DURATION;
        sprite_count = 0U;
        sprite_hide_frame = 0U;
        palette_frames = license_frames;
        palette_levels = license_levels;
    }
    frames_offset = palettes_offset + (uint32_t)stage_count * palette_stride;
    sprites_offset = frames_offset + (uint32_t)stage_count * 2U;
    total_size = sprites_offset + (uint32_t)sprite_count *
                                 TECMO_ASSET_PACK_PRESENTS_SPRITE_STRIDE;
    if (payload_size != total_size ||
        (kind == 0U && total_size != TECMO_ASSET_PACK_PRESENTS_SIZE) ||
        (kind == 1U && total_size != TECMO_ASSET_PACK_LICENSE_SIZE)) {
        tecmo_asset_pack_set_message(message, message_size, "Opening screen payload layout mismatch.");
        return -1;
    }

    fixed_bank = prg_banks - 1U;
    fixed_offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank00_offset = prg_offset;
    descriptor_offset = fixed_offset +
                        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                        (uint64_t)screen_id * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    stream_offset = bank00_offset + (stream_cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    palette_offset = bank00_offset +
                     (TECMO_ASSET_PACK_OPENING_PALETTE_CPU -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    if (descriptor_offset > rom_size ||
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE > rom_size - descriptor_offset ||
        stream_offset >= rom_size || palette_offset > rom_size ||
        16U > rom_size - palette_offset) {
        tecmo_asset_pack_set_message(message, message_size, "Opening screen descriptor, stream, or palette is outside PRG ROM.");
        return -1;
    }
    descriptor = rom + (size_t)descriptor_offset;
    if (descriptor[0] != TECMO_ASSET_PACK_PRESENTS_BG_R0 / 2U ||
        descriptor[1] != TECMO_ASSET_PACK_PRESENTS_BG_R1 / 2U ||
        tecmo_asset_pack_read_u16(descriptor + 2U) != TECMO_ASSET_PACK_OPENING_PALETTE_CPU ||
        tecmo_asset_pack_read_u16(descriptor + 4U) != stream_cpu || descriptor[6U] != 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Opening screen descriptor does not match the Rev1 route.");
        return -1;
    }
    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)palette_offset, 16U) !=
            TECMO_ASSET_PACK_OPENING_BG_PALETTE_FINGERPRINT) {
        tecmo_asset_pack_set_message(message, message_size, "Opening screen background palette fingerprint mismatch.");
        return -1;
    }
    if (tecmo_asset_pack_validate_chr_pair(TECMO_ASSET_PACK_PRESENTS_BG_R0,
                          TECMO_ASSET_PACK_PRESENTS_BG_R1,
                          chr_size,
                          "opening background",
                          message,
                          message_size) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)bank00_offset,
                           TECMO_ASSET_PACK_PRG_BANK_BYTES,
                           stream_cpu - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE,
                           decoded,
                           sizeof(decoded),
                           &encoded_size,
                           message,
                           message_size) != 0 ||
        encoded_size != expected_stream_size) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Opening screen %u is not the Rev1 %u-byte compressed stream.",
                     screen_id,
                     expected_stream_size);
        return -1;
    }

    memset(target_palette, 0x0FU, sizeof(target_palette));
    memcpy(target_palette, rom + (size_t)palette_offset, 16U);
    selector_code_offset = bank00_offset +
                           (TECMO_ASSET_PACK_PRESENTS_SELECTOR_CODE_CPU -
                            TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    if (kind == 0U) {
        sprite_table_offset = bank00_offset +
                              (TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        sprite_palette_offset = bank00_offset +
                                (TECMO_ASSET_PACK_PRESENTS_SPRITE_PALETTE_CPU -
                                 TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        y_base_operand_offset = bank00_offset +
                                (TECMO_ASSET_PACK_PRESENTS_Y_BASE_OPERAND_CPU -
                                 TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        tile_base_operand_offset = bank00_offset +
                                   (TECMO_ASSET_PACK_PRESENTS_TILE_BASE_OPERAND_CPU -
                                    TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        x_base_operand_offset = bank00_offset +
                                (TECMO_ASSET_PACK_PRESENTS_X_BASE_OPERAND_CPU -
                                 TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        if (selector_code_offset > rom_size ||
            sizeof(selector_contract) > rom_size - selector_code_offset ||
            sprite_table_offset > rom_size || 81U > rom_size - sprite_table_offset ||
            sprite_palette_offset > rom_size || 16U > rom_size - sprite_palette_offset ||
            y_base_operand_offset >= rom_size || tile_base_operand_offset >= rom_size ||
            x_base_operand_offset >= rom_size ||
            memcmp(rom + (size_t)selector_code_offset,
                   selector_contract,
                   sizeof(selector_contract)) != 0 ||
            rom[(size_t)sprite_table_offset] != TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT ||
            (enforce_revision_fingerprints != 0 &&
             tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_table_offset + 1U,
                     TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT * 4U) !=
                 TECMO_ASSET_PACK_PRESENTS_SPRITE_RECORD_FINGERPRINT) ||
            (enforce_revision_fingerprints != 0 &&
             tecmo_asset_pack_fnv1a32(rom + (size_t)sprite_palette_offset, 16U) !=
                 TECMO_ASSET_PACK_PRESENTS_SPRITE_PALETTE_FINGERPRINT) ||
            rom[(size_t)y_base_operand_offset] != 0x40U ||
            rom[(size_t)tile_base_operand_offset] != 0x55U ||
            rom[(size_t)x_base_operand_offset] != 0x50U) {
            tecmo_asset_pack_set_message(message, message_size, "TECMO rabbit sprite route does not match Rev1.");
            return -1;
        }
        if (((uint64_t)TECMO_ASSET_PACK_PRESENTS_SPRITE_R2 + 1U) * 1024U > chr_size ||
            ((uint64_t)TECMO_ASSET_PACK_PRESENTS_SPRITE_R3 + 1U) * 1024U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "TECMO rabbit sprite selectors resolve outside chr/all.");
            return -1;
        }
        memcpy(target_palette + 16U, rom + (size_t)sprite_palette_offset, 16U);
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TISC", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_OPENING_SCREEN_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, TECMO_ASSET_PACK_OPENING_SCREEN_WIDTH);
    tecmo_asset_pack_store_u16(payload + 10U, TECMO_ASSET_PACK_OPENING_SCREEN_HEIGHT);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_OPENING_SCREEN_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 14U, (uint16_t)kind);
    tecmo_asset_pack_store_u32(payload + 16U, TECMO_ASSET_PACK_OPENING_SCREEN_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_OPENING_SCREEN_CELLS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 24U, stage_count);
    tecmo_asset_pack_store_u16(payload + 26U, palette_stride);
    tecmo_asset_pack_store_u32(payload + 28U, palettes_offset);
    tecmo_asset_pack_store_u32(payload + 32U, frames_offset);
    tecmo_asset_pack_store_u16(payload + 36U, duration);
    tecmo_asset_pack_store_u16(payload + 38U, sprite_count);
    tecmo_asset_pack_store_u32(payload + 40U, total_size);
    tecmo_asset_pack_store_u32(payload + 44U, sprites_offset);
    tecmo_asset_pack_store_u16(payload + 48U, TECMO_ASSET_PACK_PRESENTS_SPRITE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 50U, 0U);
    tecmo_asset_pack_store_u16(payload + 52U, sprite_hide_frame);

    for (size_t i = 0U; i < TECMO_ASSET_PACK_OPENING_SCREEN_CELL_COUNT; ++i) {
        unsigned row = (unsigned)(i / TECMO_ASSET_PACK_OPENING_SCREEN_WIDTH);
        unsigned col = (unsigned)(i % TECMO_ASSET_PACK_OPENING_SCREEN_WIDTH);
        uint8_t tile = decoded[i];
        uint8_t selector = tile < 128U ? TECMO_ASSET_PACK_PRESENTS_BG_R0
                                      : TECMO_ASSET_PACK_PRESENTS_BG_R1;
        uint32_t chr_offset = (uint32_t)selector * 1024U +
                              (uint32_t)(tile & 0x7FU) * 16U;
        uint8_t *cell = payload + TECMO_ASSET_PACK_OPENING_SCREEN_CELLS_OFFSET +
                        i * TECMO_ASSET_PACK_OPENING_SCREEN_CELL_STRIDE;
        if ((uint64_t)chr_offset + 16U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "Opening screen tile resolves outside chr/all.");
            return -1;
        }
        cell[0] = tile;
        cell[1] = tecmo_asset_pack_decoded_palette_index(decoded, row, col);
        tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
    }
    for (size_t stage = 0U; stage < stage_count; ++stage) {
        uint8_t *stage_palette = payload + palettes_offset + stage * palette_stride;
        if (kind == 0U && stage >= 5U) {
            build_opening_fade_out_stage(stage_palette,
                                         target_palette,
                                         palette_stride,
                                         (unsigned int)(stage - 4U));
        } else {
            build_opening_palette_stage(stage_palette,
                                        target_palette,
                                        palette_stride,
                                        palette_levels[stage]);
        }
        tecmo_asset_pack_store_u16(payload + frames_offset + stage * 2U, palette_frames[stage]);
    }

    if (kind == 0U) {
        const uint8_t *records = rom + (size_t)sprite_table_offset + 1U;
        for (size_t i = 0U; i < TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT; ++i) {
            const uint8_t *record = records + i * 4U;
            uint16_t tile = (uint16_t)record[1] + 0x55U;
            uint8_t selector;
            uint32_t chr_offset;
            int16_t x = (int16_t)(0x50 + (int8_t)record[3]);
            int16_t y = (int16_t)(0x40 + (int8_t)record[0]);
            uint8_t flags = 0U;
            uint8_t *piece = payload + sprites_offset +
                             i * TECMO_ASSET_PACK_PRESENTS_SPRITE_STRIDE;
            if (tile < 0x40U) selector = TECMO_ASSET_PACK_PRESENTS_SPRITE_R2;
            else if (tile < 0x80U) selector = TECMO_ASSET_PACK_PRESENTS_SPRITE_R3;
            else {
                tecmo_asset_pack_set_message(message, message_size, "TECMO rabbit tile is outside its two CHR pages.");
                return -1;
            }
            chr_offset = (uint32_t)selector * 1024U + (uint32_t)(tile & 0x3FU) * 16U;
            if ((uint64_t)chr_offset + 16U > chr_size || (record[2] & 0x3CU) != 0U) {
                tecmo_asset_pack_set_message(message, message_size, "TECMO rabbit piece is invalid.");
                return -1;
            }
            if ((record[2] & 0x40U) != 0U) flags |= 0x01U;
            if ((record[2] & 0x80U) != 0U) flags |= 0x02U;
            tecmo_asset_pack_store_u16(piece + 0U, (uint16_t)x);
            tecmo_asset_pack_store_u16(piece + 2U, (uint16_t)y);
            tecmo_asset_pack_store_u32(piece + 4U, chr_offset);
            piece[8] = record[2] & 3U;
            piece[9] = flags;
            tecmo_asset_pack_store_u16(piece + 10U, 0U);
        }
    }

    memset(provenance, 0, sizeof(*provenance));
    provenance->screen_id = (uint8_t)screen_id;
    provenance->bg_r0 = TECMO_ASSET_PACK_PRESENTS_BG_R0;
    provenance->bg_r1 = TECMO_ASSET_PACK_PRESENTS_BG_R1;
    provenance->stream_cpu = stream_cpu;
    provenance->palette_cpu = TECMO_ASSET_PACK_OPENING_PALETTE_CPU;
    provenance->descriptor_offset = descriptor_offset;
    provenance->stream_offset = stream_offset;
    provenance->stream_size = encoded_size;
    provenance->palette_offset = palette_offset;
    provenance->selector_code_offset = kind == 0U ? selector_code_offset : 0U;
    provenance->sprite_table_offset = sprite_table_offset;
    provenance->sprite_palette_offset = sprite_palette_offset;
    provenance->y_base_operand_offset = y_base_operand_offset;
    provenance->tile_base_operand_offset = tile_base_operand_offset;
    provenance->x_base_operand_offset = x_base_operand_offset;
    return 0;
}
