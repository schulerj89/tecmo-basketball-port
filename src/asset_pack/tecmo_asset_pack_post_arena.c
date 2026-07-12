#include "tecmo_asset_pack_post_arena.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

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

void tecmo_asset_pack_post_arena_seed_contract_fixture(uint8_t *rom,
                                                       uint64_t bank04_offset,
                                                       uint64_t fixed_offset)
{
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
}

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

int tecmo_asset_pack_build_ready_screen(const uint8_t *rom,
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

int tecmo_asset_pack_build_warriors_transition(const uint8_t *rom,
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

int tecmo_asset_pack_build_clippers_transition(const uint8_t *rom,
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

int tecmo_asset_pack_build_bucks_transition(const uint8_t *rom,
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

int tecmo_asset_pack_build_pass_transition(const uint8_t *rom,
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
