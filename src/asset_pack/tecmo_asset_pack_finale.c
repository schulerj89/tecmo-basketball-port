#include "tecmo_asset_pack_finale.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

static int validate_finale_fingerprint(const uint8_t *rom,
                                       uint64_t rom_size,
                                       uint64_t source_offset,
                                       size_t byte_count,
                                       uint32_t expected,
                                       const char *role,
                                       char *message,
                                       size_t message_size)
{
    if (source_offset > rom_size || (uint64_t)byte_count > rom_size - source_offset ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)source_offset, byte_count) != expected) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Finale %s revision fingerprint mismatch.",
                     role);
        return -1;
    }
    return 0;
}
static int read_finale_chr_selector_write(const uint8_t *rom,
                                          uint64_t rom_size,
                                          uint64_t bank04_offset,
                                          uint16_t instruction_cpu,
                                          uint8_t destination,
                                          uint8_t *selector_out,
                                          uint64_t *operand_offset_out)
{
    uint64_t offset = bank04_offset + (instruction_cpu - 0x8000U);
    if (offset + 4U > rom_size || rom[(size_t)offset] != 0xA9U ||
        rom[(size_t)offset + 2U] != 0x85U ||
        rom[(size_t)offset + 3U] != destination) {
        return -1;
    }
    *selector_out = rom[(size_t)offset + 1U];
    *operand_offset_out = offset + 1U;
    return 0;
}

static int validate_finale_source_contract(const uint8_t *rom,
                                           uint64_t rom_size,
                                           uint64_t prg_offset,
                                           uint32_t prg_banks,
                                           int enforce_revision_fingerprints,
                                           uint8_t chr_selectors_out[3],
                                           uint64_t chr_selector_offsets_out[3],
                                           char *message,
                                           size_t message_size)
{
    uint64_t bank04_offset;
    uint64_t fixed_offset;
    uint64_t bank00_offset;
    uint64_t bank06_offset;
    uint8_t comparison_selector;
    uint64_t comparison_offset;

    if (prg_banks <= 6U) {
        tecmo_asset_pack_set_message(message, message_size, "Finale import requires Banks 00, 04, 06 and fixed PRG.");
        return -1;
    }
    bank00_offset = prg_offset;
    bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    fixed_offset = prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (enforce_revision_fingerprints != 0) {
        if (validate_finale_fingerprint(
                rom, rom_size,
                bank04_offset + (TECMO_ASSET_PACK_FINALE_BANK04_FINGERPRINT_CPU - 0x8000U),
                TECMO_ASSET_PACK_FINALE_BANK04_FINGERPRINT_SIZE,
                TECMO_ASSET_PACK_FINALE_BANK04_FINGERPRINT,
                "Bank04 route", message, message_size) != 0 ||
            validate_finale_fingerprint(
                rom, rom_size,
                fixed_offset + (TECMO_ASSET_PACK_FINALE_DESCRIPTOR_FINGERPRINT_CPU - 0xC000U),
                TECMO_ASSET_PACK_FINALE_DESCRIPTOR_FINGERPRINT_SIZE,
                TECMO_ASSET_PACK_FINALE_DESCRIPTOR_FINGERPRINT,
                "screen descriptor", message, message_size) != 0 ||
            validate_finale_fingerprint(
                rom, rom_size,
                fixed_offset + (TECMO_ASSET_PACK_FINALE_FIXED_IRQ_CPU - 0xC000U),
                TECMO_ASSET_PACK_FINALE_FIXED_IRQ_SIZE,
                TECMO_ASSET_PACK_FINALE_IRQ_FINGERPRINT,
                "title split", message, message_size) != 0 ||
            validate_finale_fingerprint(
                rom, rom_size,
                bank00_offset + (TECMO_ASSET_PACK_FINALE_PIECE_STREAM_CPU - 0x8000U),
                1U + TECMO_ASSET_PACK_FINALE_PIECE_COUNT * 4U,
                TECMO_ASSET_PACK_FINALE_GEOMETRY_FINGERPRINT,
                "sprite geometry", message, message_size) != 0 ||
            validate_finale_fingerprint(
                rom, rom_size,
                bank06_offset + (TECMO_ASSET_PACK_FINALE_TITLE_HANDLER_CPU - 0x8000U),
                TECMO_ASSET_PACK_FINALE_TITLE_HANDLER_SIZE,
                TECMO_ASSET_PACK_FINALE_TITLE_HANDLER_FINGERPRINT,
                "title handler", message, message_size) != 0 ||
            validate_finale_fingerprint(
                rom, rom_size,
                bank06_offset + (TECMO_ASSET_PACK_FINALE_GLYPH_MAP_CPU - 0x8000U),
                TECMO_ASSET_PACK_FINALE_CHAR_MAP_FINGERPRINT_SIZE,
                TECMO_ASSET_PACK_FINALE_CHAR_MAP_FINGERPRINT,
                "character map", message, message_size) != 0) {
            return -1;
        }
        {
            const uint8_t *dispatch = rom + (size_t)bank04_offset + (0x82CFU - 0x8000U);
            const uint8_t *irq = rom + (size_t)fixed_offset +
                                 (TECMO_ASSET_PACK_FINALE_FIXED_IRQ_CPU - 0xC000U);
            if (tecmo_asset_pack_read_u16(dispatch + 12U) != 0x851CU ||
                tecmo_asset_pack_read_u16(dispatch + 14U) != 0x83EAU ||
                tecmo_asset_pack_read_u16(dispatch + 16U) != 0x852EU ||
                tecmo_asset_pack_read_u16(dispatch + 18U) != 0x83AEU ||
                tecmo_asset_pack_read_u16(dispatch + 20U) != 0x8310U ||
                tecmo_asset_pack_read_u16(dispatch + 22U) != 0xFFFFU ||
                dispatch[30U] != 50U || dispatch[31U] != 30U ||
                dispatch[32U] != 0U || dispatch[33U] != 75U || dispatch[34U] != 1U ||
                rom[(size_t)bank04_offset + (0x851CU - 0x8000U)] != 0xA9U ||
                rom[(size_t)bank04_offset + (0x851DU - 0x8000U)] != 0x1CU ||
                rom[(size_t)bank04_offset + (0x83EAU - 0x8000U)] != 0xA9U ||
                rom[(size_t)bank04_offset + (0x83EBU - 0x8000U)] != 0x20U ||
                rom[(size_t)bank04_offset + (0x83AEU - 0x8000U)] != 0xA9U ||
                rom[(size_t)bank04_offset + (0x83AFU - 0x8000U)] != 0x22U ||
                rom[(size_t)bank04_offset + (0x8310U - 0x8000U)] != 0xA9U ||
                rom[(size_t)bank04_offset + (0x8311U - 0x8000U)] != 0x2DU ||
                irq[4U] != 0x17U || irq[47U] != 0x01U ||
                irq[55U] != 0xC8U || irq[98U] != 0x02U ||
                irq[102U] != 0xB5U || irq[103U] != 0x33U ||
                irq[118U] != 0x15U || irq[119U] != 0x39U ||
                irq[123U] != 0x4CU || tecmo_asset_pack_read_u16(irq + 124U) != 0xFDDDU) {
                tecmo_asset_pack_set_message(message, message_size, "Finale native route or title-band semantics mismatch.");
                return -1;
            }
        }
    }

    if (read_finale_chr_selector_write(rom, rom_size, bank04_offset,
                                       0x8569U, 0x57U,
                                       &chr_selectors_out[0],
                                       &chr_selector_offsets_out[0]) != 0 ||
        read_finale_chr_selector_write(rom, rom_size, bank04_offset,
                                       0x856DU, 0x58U,
                                       &chr_selectors_out[1],
                                       &chr_selector_offsets_out[1]) != 0 ||
        read_finale_chr_selector_write(rom, rom_size, bank04_offset,
                                       0x8571U, 0x59U,
                                       &chr_selectors_out[2],
                                       &chr_selector_offsets_out[2]) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Finale reverse-route CHR selector writes are invalid.");
        return -1;
    }
    if (chr_selectors_out[0] != 0x91U ||
        chr_selectors_out[1] != 0x93U ||
        chr_selectors_out[2] != 0x95U) {
        tecmo_asset_pack_set_message(message, message_size, "Finale sprite CHR selector operands are unsupported.");
        return -1;
    }
    for (size_t selector = 0U; selector < 3U; ++selector) {
        uint16_t short_cpu = (uint16_t)(0x8402U + selector * 4U);
        uint16_t staged_cpu = (uint16_t)(0x83C2U + selector * 4U);
        uint8_t destination = (uint8_t)(0x57U + selector);
        if (read_finale_chr_selector_write(rom, rom_size, bank04_offset,
                                           short_cpu, destination,
                                           &comparison_selector,
                                           &comparison_offset) != 0 ||
            comparison_selector != chr_selectors_out[selector] ||
            read_finale_chr_selector_write(rom, rom_size, bank04_offset,
                                           staged_cpu, destination,
                                           &comparison_selector,
                                           &comparison_offset) != 0 ||
            comparison_selector != chr_selectors_out[selector]) {
            tecmo_asset_pack_set_message(message, message_size, "Finale scene CHR selector writes disagree.");
            return -1;
        }
    }

    if (bank00_offset + (TECMO_ASSET_PACK_FINALE_POINTER_TABLE_CPU - 0x8000U) + 4U > rom_size ||
        tecmo_asset_pack_read_u16(rom + (size_t)bank00_offset +
                 (TECMO_ASSET_PACK_FINALE_POINTER_TABLE_CPU - 0x8000U) + 2U) !=
            TECMO_ASSET_PACK_FINALE_PIECE_STREAM_CPU ||
        rom[(size_t)bank00_offset +
            (TECMO_ASSET_PACK_FINALE_PIECE_STREAM_CPU - 0x8000U)] !=
            TECMO_ASSET_PACK_FINALE_PIECE_COUNT) {
        tecmo_asset_pack_set_message(message, message_size, "Finale ten-piece selector source-contract mismatch.");
        return -1;
    }
    return 0;
}

static int finale_title_char_to_tile(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t bank06_offset,
                                     uint8_t code,
                                     uint8_t *tile_out)
{
    uint64_t map_offset;
    if (tile_out == NULL) return -1;
    if (code == 0x2EU || code == 0x20U) {
        *tile_out = 0x18U;
        return 0;
    }
    if (code == 0x2DU) {
        *tile_out = 0x25U;
        return 0;
    }
    if (code >= 0x17U && code < 0x3AU) {
        *tile_out = (uint8_t)(code - 0x17U);
        return 0;
    }
    map_offset = bank06_offset +
                 (TECMO_ASSET_PACK_FINALE_GLYPH_MAP_CPU - 0x8000U) + code;
    if (map_offset >= rom_size) return -1;
    *tile_out = rom[(size_t)map_offset];
    return 0;
}

int tecmo_asset_pack_finale_self_test(char *message, size_t message_size)
{
    uint8_t space_tile = 0U;
    uint8_t period_tile = 0U;
    if (finale_title_char_to_tile(NULL, 0U, 0U, 0x20U, &space_tile) != 0 ||
        finale_title_char_to_tile(NULL, 0U, 0U, 0x2EU, &period_tile) != 0 ||
        space_tile != 0x18U || period_tile != 0x18U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test finale space/period mapping mismatch.");
        return -1;
    }
    return 0;
}

int tecmo_asset_pack_build_finale_sequence(const uint8_t *rom,
                                 uint64_t rom_size,
                                 uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 uint64_t chr_size,
                                 int enforce_revision_fingerprints,
                                 uint8_t payload[TECMO_ASSET_PACK_FINALE_SIZE],
                                 TecmoFinaleProvenance *provenance,
                                 char *message,
                                 size_t message_size)
{
    static const uint8_t screen_ids[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
        0x1CU, 0x20U, 0x1FU, 0x22U, 0x2DU
    };
    static const uint16_t route_cpu[TECMO_ASSET_PACK_FINALE_ROUTE_COUNT] = {
        0x851CU, 0x83EAU, 0x852EU, 0x83AEU, 0x8310U
    };
    static const uint8_t route_kinds[TECMO_ASSET_PACK_FINALE_ROUTE_COUNT] = {
        0U, 1U, 2U, 3U, 4U
    };
    static const uint8_t route_groups[TECMO_ASSET_PACK_FINALE_ROUTE_COUNT] = {
        0xFFU, 0U, 1U, 0U, 0xFFU
    };
    static const uint16_t route_internal_frames[TECMO_ASSET_PACK_FINALE_ROUTE_COUNT] = {
        0U, 16U, 45U, 80U, 601U
    };
    static const uint16_t reverse_palette_frames[TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_COUNT] = {
        10U, 14U, 18U, 22U, 27U
    };
    uint8_t sprite_selectors[3];
    uint64_t sprite_selector_offsets[3];
    uint8_t decoded[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT][2048];
    uint8_t screen_r0[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT];
    uint8_t screen_r1[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT];
    uint64_t fixed_offset;
    uint64_t bank00_offset = prg_offset;
    uint64_t bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t helper_palette_offset;
    uint64_t short_palette_offset;
    uint64_t special_palette_offset;
    uint64_t piece_stream_offset;
    uint64_t title_source_offset;

    if (payload == NULL || provenance == NULL || chr_size == 0U ||
        validate_finale_source_contract(rom, rom_size, prg_offset, prg_banks,
                                        enforce_revision_fingerprints,
                                        sprite_selectors,
                                        sprite_selector_offsets,
                                        message, message_size) != 0) {
        return -1;
    }
    fixed_offset = prg_offset + (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    memset(payload, 0, TECMO_ASSET_PACK_FINALE_SIZE);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t selector = 0U; selector < 3U; ++selector) {
        if ((uint64_t)(sprite_selectors[selector] + 1U) * 1024U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "Finale sprite CHR selector resolves outside chr/all.");
            return -1;
        }
        provenance->sprite_chr_selector[selector] = sprite_selectors[selector];
        provenance->sprite_chr_selector_offset[selector] = sprite_selector_offsets[selector];
    }

    for (size_t screen = 0U; screen < TECMO_ASSET_PACK_FINALE_SCREEN_COUNT; ++screen) {
        uint64_t descriptor_offset = fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            (uint64_t)screen_ids[screen] * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
        const uint8_t *descriptor;
        uint32_t palette_cpu;
        uint32_t stream_cpu;
        uint32_t stream_bank;
        uint64_t stream_bank_offset;
        uint64_t palette_offset;
        size_t decoded_size = (screen == 0U || screen == 3U) ? 1024U : 2048U;
        size_t encoded_size = 0U;

        if (descriptor_offset + TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE > rom_size) {
            tecmo_asset_pack_set_message(message, message_size, "Finale screen descriptor crosses fixed PRG.");
            return -1;
        }
        descriptor = rom + (size_t)descriptor_offset;
        palette_cpu = tecmo_asset_pack_read_u16(descriptor + 2U);
        stream_cpu = tecmo_asset_pack_read_u16(descriptor + 4U);
        stream_bank = descriptor[6U];
        if (descriptor[0] > 0x7FU || descriptor[1] > 0x7FU ||
            stream_bank >= prg_banks || palette_cpu < 0x8000U || palette_cpu >= 0xC000U ||
            stream_cpu < 0x8000U || stream_cpu >= 0xC000U) {
            tecmo_asset_pack_set_message(message, message_size, "Finale screen descriptor is invalid.");
            return -1;
        }
        screen_r0[screen] = (uint8_t)(descriptor[0] * 2U);
        screen_r1[screen] = (uint8_t)(descriptor[1] * 2U);
        if (tecmo_asset_pack_validate_chr_pair(screen_r0[screen], screen_r1[screen], chr_size,
                              "finale screen", message, message_size) != 0) return -1;
        stream_bank_offset = prg_offset + (uint64_t)stream_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        palette_offset = stream_bank_offset + (palette_cpu - 0x8000U);
        if (palette_offset + 16U > rom_size ||
            tecmo_asset_pack_decode_d9f6_stream(rom + (size_t)stream_bank_offset,
                               TECMO_ASSET_PACK_PRG_BANK_BYTES,
                               stream_cpu - 0x8000U,
                               decoded[screen],
                               decoded_size,
                               &encoded_size,
                               message,
                               message_size) != 0) {
            tecmo_asset_pack_set_messagef(message, message_size, "Finale screen %u decode failed.", (unsigned)screen);
            return -1;
        }
        if (decoded_size == 1024U) memcpy(decoded[screen] + 1024U, decoded[screen], 1024U);
        memcpy(payload + TECMO_ASSET_PACK_FINALE_BACKGROUND_PALETTES_OFFSET + screen * 16U,
               rom + (size_t)palette_offset,
               16U);
        for (size_t page = 0U; page < 2U; ++page) {
            const uint8_t *decoded_page = decoded[screen] + page * 1024U;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_TILES_PER_PAGE; ++i) {
                unsigned row = (unsigned)(i / 32U);
                unsigned col = (unsigned)(i % 32U);
                uint8_t tile = decoded_page[i];
                uint32_t chr_offset = tecmo_asset_pack_bg_chr_offset(tile,
                                                               screen_r0[screen],
                                                               screen_r1[screen]);
                uint8_t *cell = payload + TECMO_ASSET_PACK_FINALE_SCREENS_OFFSET +
                    (screen * TECMO_ASSET_PACK_FINALE_CELLS_PER_SCREEN + page * 960U + i) *
                        TECMO_ASSET_PACK_FINALE_CELL_STRIDE;
                if ((uint64_t)chr_offset + 16U > chr_size) {
                    tecmo_asset_pack_set_message(message, message_size, "Finale background tile resolves outside chr/all.");
                    return -1;
                }
                cell[0] = tile;
                cell[1] = tecmo_asset_pack_decoded_palette_index(decoded_page, row, col);
                tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
            }
        }
        provenance->screens[screen].descriptor_offset = descriptor_offset;
        provenance->screens[screen].stream_offset = stream_bank_offset + (stream_cpu - 0x8000U);
        provenance->screens[screen].stream_size = encoded_size;
        provenance->screens[screen].palette_offset = palette_offset;
        provenance->screens[screen].stream_bank = stream_bank;
        provenance->screens[screen].stream_cpu = stream_cpu;
        provenance->screens[screen].palette_cpu = palette_cpu;
    }

    helper_palette_offset = bank04_offset +
                            (TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_CPU - 0x8000U);
    short_palette_offset = bank04_offset +
                           (TECMO_ASSET_PACK_FINALE_SHORT_PALETTE_CPU - 0x8000U);
    special_palette_offset = bank04_offset +
                             (TECMO_ASSET_PACK_FINALE_SPECIAL_PALETTE_CPU - 0x8000U);
    if (short_palette_offset + 16U > rom_size || helper_palette_offset + 16U > rom_size ||
        special_palette_offset + 16U > rom_size) {
        tecmo_asset_pack_set_message(message, message_size, "Finale palette sources cross PRG.");
        return -1;
    }
    for (size_t stage = 0U; stage < 4U; ++stage) {
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)provenance->screens[2].palette_offset + i];
            if (i == 4U || i == 8U || i == 12U) color = rom[(size_t)helper_palette_offset + i];
            if (i != 4U && i != 8U && i != 12U) color = tecmo_asset_pack_palette_brightness_cap(color, (uint8_t)stage);
            payload[TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + stage * 16U + i] = color;
        }
    }
    memcpy(payload + TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + 64U,
           rom + (size_t)provenance->screens[2].palette_offset,
           16U);
    memcpy(payload + TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + 64U,
           rom + (size_t)special_palette_offset,
           4U);
    payload[TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + 68U] =
        rom[(size_t)helper_palette_offset + 4U];
    payload[TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + 72U] =
        rom[(size_t)helper_palette_offset + 8U];
    payload[TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET + 76U] =
        rom[(size_t)helper_palette_offset + 12U];
    for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_COUNT; ++i) {
        tecmo_asset_pack_store_u16(payload + TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_FRAMES_OFFSET + i * 2U,
                  reverse_palette_frames[i]);
    }

    for (size_t group = 0U; group < TECMO_ASSET_PACK_FINALE_GROUP_COUNT; ++group) {
        uint64_t source_offset = group == 0U ? short_palette_offset : helper_palette_offset;
        uint8_t *dest = payload + TECMO_ASSET_PACK_FINALE_SPRITE_PALETTES_OFFSET + group * 16U;
        uint8_t *descriptor = payload + TECMO_ASSET_PACK_FINALE_GROUPS_OFFSET +
                              group * TECMO_ASSET_PACK_FINALE_GROUP_STRIDE;
        for (size_t palette = 0U; palette < 4U; ++palette) {
            const uint8_t *source = rom + (size_t)source_offset + palette * 4U;
            if (source[0] != 0x02U) {
                tecmo_asset_pack_set_message(message, message_size, "Finale sprite palette control is not Rev1.");
                return -1;
            }
            dest[palette * 4U] = 0x0FU;
            memcpy(dest + palette * 4U + 1U, source + 1U, 3U);
        }
        descriptor[0] = (uint8_t)group;
        descriptor[1] = (uint8_t)group;
        tecmo_asset_pack_store_u16(descriptor + 2U, TECMO_ASSET_PACK_FINALE_PIECE_COUNT);
        tecmo_asset_pack_store_u32(descriptor + 4U, TECMO_ASSET_PACK_FINALE_PIECES_OFFSET);
        tecmo_asset_pack_store_u32(descriptor + 8U,
                  TECMO_ASSET_PACK_FINALE_SPRITE_PALETTES_OFFSET + (uint32_t)group * 16U);
        tecmo_asset_pack_store_u16(descriptor + 12U, group == 0U ? 0x0005U : 0x0002U);
        provenance->sprite_palette_offset[group] = source_offset;
    }

    piece_stream_offset = bank00_offset +
                          (TECMO_ASSET_PACK_FINALE_PIECE_STREAM_CPU - 0x8000U);
    for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_PIECE_COUNT; ++i) {
        const uint8_t *record = rom + (size_t)piece_stream_offset + 1U + i * 4U;
        uint8_t top_tile = (uint8_t)((record[1] + 1U) & 0xFEU);
        unsigned slot = top_tile / 64U;
        uint8_t attributes = record[2];
        uint8_t flags = 0U;
        uint32_t top_offset;
        uint8_t *piece = payload + TECMO_ASSET_PACK_FINALE_PIECES_OFFSET +
                         i * TECMO_ASSET_PACK_FINALE_PIECE_STRIDE;
        if (slot >= 3U || (attributes & 0x3CU) != 0U) {
            tecmo_asset_pack_set_message(message, message_size, "Finale sprite piece has invalid selector or attributes.");
            return -1;
        }
        top_offset = (uint32_t)sprite_selectors[slot] * 1024U +
                     (uint32_t)(top_tile & 0x3FU) * 16U;
        if ((uint64_t)top_offset + 32U > chr_size) {
            tecmo_asset_pack_set_message(message, message_size, "Finale sprite piece resolves outside chr/all.");
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

    for (size_t route = 0U; route < TECMO_ASSET_PACK_FINALE_ROUTE_COUNT; ++route) {
        uint8_t *record = payload + TECMO_ASSET_PACK_FINALE_ROUTES_OFFSET +
                          route * TECMO_ASSET_PACK_FINALE_ROUTE_STRIDE;
        uint16_t dispatch_wait = rom[(size_t)bank04_offset + (0x82E7U - 0x8000U) + 6U + route];
        record[0] = (uint8_t)route;
        record[1] = route_groups[route];
        record[2] = route_kinds[route];
        record[3] = route == 4U ? 1U : 0U;
        tecmo_asset_pack_store_u16(record + 4U, route_internal_frames[route]);
        tecmo_asset_pack_store_u16(record + 6U, dispatch_wait);
        provenance->route_offsets[route] = bank04_offset + (route_cpu[route] - 0x8000U);
    }

    for (size_t step = 0U; step < 8U; ++step) {
        size_t table_index = step / 2U;
        uint8_t *anchor = payload + TECMO_ASSET_PACK_FINALE_SHORT_ANCHORS_OFFSET + step * 2U;
        anchor[0] = rom[(size_t)bank04_offset + (0x8463U - 0x8000U) + table_index];
        anchor[1] = rom[(size_t)bank04_offset + (0x8473U - 0x8000U) + table_index];
    }
    payload[TECMO_ASSET_PACK_FINALE_SHORT_ANCHORS_OFFSET + 16U] =
        rom[(size_t)bank04_offset + (0x83DAU - 0x8000U)];
    payload[TECMO_ASSET_PACK_FINALE_SHORT_ANCHORS_OFFSET + 17U] =
        rom[(size_t)bank04_offset + (0x83D8U - 0x8000U)];
    {
        uint8_t selector = rom[(size_t)bank04_offset + (0x852FU - 0x8000U)];
        uint8_t *meta = payload + TECMO_ASSET_PACK_FINALE_REVERSE_METADATA_OFFSET;
        if (selector != 2U ||
            rom[(size_t)bank04_offset + (0x863CU - 0x8000U) + selector] != 0x1FU ||
            rom[(size_t)bank04_offset + (0x863EU - 0x8000U) + selector] != 0x78U ||
            rom[(size_t)bank04_offset + (0x8640U - 0x8000U) + selector] != 0xD8U ||
            rom[(size_t)bank04_offset + (0x8642U - 0x8000U) + selector] != 0xF8U) {
            tecmo_asset_pack_set_message(message, message_size, "Finale selector-two operand contract mismatch.");
            return -1;
        }
        meta[0] = rom[(size_t)bank04_offset + (0x863EU - 0x8000U) + selector];
        meta[1] = rom[(size_t)bank04_offset + (0x8640U - 0x8000U) + selector];
        meta[2] = rom[(size_t)bank04_offset + (0x8642U - 0x8000U) + selector];
        meta[3] = 0x54U;
        tecmo_asset_pack_store_u16(meta + 4U, 18U);
        tecmo_asset_pack_store_u16(meta + 6U, 1U);
        tecmo_asset_pack_store_u16(meta + 8U, 26U);
        tecmo_asset_pack_store_u16(meta + 10U, TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_COUNT);
    }

    {
        uint8_t *meta = payload + TECMO_ASSET_PACK_FINALE_TITLE_METADATA_OFFSET;
        tecmo_asset_pack_store_u16(meta + 0U, 128U);
        tecmo_asset_pack_store_u16(meta + 2U, TECMO_ASSET_PACK_FINALE_TITLE_SLOT_COUNT);
        tecmo_asset_pack_store_u16(meta + 4U, 1U);
        tecmo_asset_pack_store_u16(meta + 6U, 7U);
        tecmo_asset_pack_store_u16(meta + 8U, 301U);
        tecmo_asset_pack_store_u16(meta + 10U, 345U);
        tecmo_asset_pack_store_u16(meta + 12U, 128U);
        tecmo_asset_pack_store_u16(meta + 14U, 1U);
        tecmo_asset_pack_store_u16(meta + 16U, 2U);
        tecmo_asset_pack_store_u16(meta + 18U, 2U);
        tecmo_asset_pack_store_u16(meta + 20U, 8U);
        tecmo_asset_pack_store_u16(meta + 22U, 1U);
        tecmo_asset_pack_store_u16(meta + 24U, 16U);
        tecmo_asset_pack_store_u16(meta + 26U, 2U);
        tecmo_asset_pack_store_u16(meta + 28U, 2U);
    }
    {
        static const uint16_t starts[3] = {0U, 200U, 223U};
        static const uint16_t ends[3] = {200U, 223U, 240U};
        static const uint8_t channels[3] = {0U, 1U, 2U};
        uint32_t low_base = (uint32_t)screen_r0[4] * 1024U;
        uint32_t high_base = (uint32_t)screen_r1[4] * 1024U;
        for (size_t band = 0U; band < 3U; ++band) {
            uint8_t *record = payload + TECMO_ASSET_PACK_FINALE_BANDS_OFFSET + band * 16U;
            tecmo_asset_pack_store_u16(record + 0U, starts[band]);
            tecmo_asset_pack_store_u16(record + 2U, ends[band]);
            record[4] = channels[band];
            record[5] = channels[band];
            tecmo_asset_pack_store_u32(record + 8U, low_base);
            tecmo_asset_pack_store_u32(record + 12U, high_base);
        }
    }

    title_source_offset = bank04_offset +
                          (TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_CPU - 0x8000U);
    for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_SIZE; ++i) {
        uint8_t value = rom[(size_t)title_source_offset + i];
        if (value < 0x20U || value > 0x5AU) {
            tecmo_asset_pack_set_message(message, message_size, "Finale title source record is not the expected 26-character form.");
            return -1;
        }
    }
    for (size_t slot_index = 0U; slot_index < TECMO_ASSET_PACK_FINALE_TITLE_SLOT_COUNT; ++slot_index) {
        uint8_t source_value = slot_index < TECMO_ASSET_PACK_FINALE_TITLE_TEXT_SLOT_COUNT
                                   ? rom[(size_t)title_source_offset + slot_index]
                                   : 0x20U;
        uint8_t mapped;
        uint64_t glyph_offset;
        unsigned virtual_slot = (unsigned)((slot_index + 16U) & 31U);
        unsigned page = virtual_slot >= 16U ? 1U : 0U;
        unsigned col = (virtual_slot & 15U) * 2U;
        unsigned row = 16U;
        uint8_t *slot = payload + TECMO_ASSET_PACK_FINALE_TITLE_SLOTS_OFFSET +
                        slot_index * TECMO_ASSET_PACK_FINALE_TITLE_SLOT_STRIDE;
        if (finale_title_char_to_tile(rom,
                                      rom_size,
                                      bank06_offset,
                                      source_value,
                                      &mapped) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Finale title character mapping is outside Bank06.");
            return -1;
        }
        glyph_offset = bank06_offset +
                       (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U) +
                       (uint64_t)mapped * 4U;
        if (glyph_offset + 4U > rom_size) {
            tecmo_asset_pack_set_message(message, message_size, "Finale title glyph quad crosses Bank06.");
            return -1;
        }
        slot[0] = (uint8_t)page;
        slot[1] = (uint8_t)col;
        slot[2] = (uint8_t)row;
        for (size_t tile_index = 0U; tile_index < 4U; ++tile_index) {
            unsigned local_row = (unsigned)(tile_index / 2U);
            unsigned local_col = (unsigned)(tile_index % 2U);
            uint8_t tile = rom[(size_t)glyph_offset + tile_index];
            uint32_t chr_offset = tecmo_asset_pack_bg_chr_offset(tile, screen_r0[4], screen_r1[4]);
            uint8_t *cell = slot + 4U + tile_index * TECMO_ASSET_PACK_FINALE_CELL_STRIDE;
            if ((uint64_t)chr_offset + 16U > chr_size) {
                tecmo_asset_pack_set_message(message, message_size, "Finale title glyph resolves outside chr/all.");
                return -1;
            }
            cell[0] = tile;
            cell[1] = tecmo_asset_pack_decoded_palette_index(decoded[4] + page * 1024U,
                                             row + local_row,
                                             col + local_col);
            tecmo_asset_pack_store_u32(cell + 2U, chr_offset);
        }
    }

    memcpy(payload, "TFIN", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_FINALE_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, TECMO_ASSET_PACK_FINALE_SCREEN_COUNT);
    tecmo_asset_pack_store_u16(payload + 10U, TECMO_ASSET_PACK_FINALE_WIDTH);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_FINALE_HEIGHT);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_FINALE_PAGE_COUNT);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_FINALE_CELL_STRIDE);
    tecmo_asset_pack_store_u16(payload + 18U, TECMO_ASSET_PACK_FINALE_BACKGROUND_PALETTE_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_FINALE_SCREENS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_FINALE_BACKGROUND_PALETTES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 28U, TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_COUNT);
    tecmo_asset_pack_store_u16(payload + 30U, 2U);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_FINALE_REVERSE_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_FINALE_REVERSE_PALETTE_FRAMES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 40U, TECMO_ASSET_PACK_FINALE_GROUP_COUNT);
    tecmo_asset_pack_store_u16(payload + 42U, TECMO_ASSET_PACK_FINALE_GROUP_STRIDE);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_FINALE_GROUPS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 48U, 16U);
    tecmo_asset_pack_store_u16(payload + 50U, TECMO_ASSET_PACK_FINALE_PIECE_STRIDE);
    tecmo_asset_pack_store_u16(payload + 52U, TECMO_ASSET_PACK_FINALE_PIECE_COUNT);
    tecmo_asset_pack_store_u32(payload + 56U, TECMO_ASSET_PACK_FINALE_PIECES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 60U, TECMO_ASSET_PACK_FINALE_ROUTE_COUNT);
    tecmo_asset_pack_store_u16(payload + 62U, TECMO_ASSET_PACK_FINALE_ROUTE_STRIDE);
    tecmo_asset_pack_store_u32(payload + 64U, TECMO_ASSET_PACK_FINALE_ROUTES_OFFSET);
    tecmo_asset_pack_store_u16(payload + 68U, TECMO_ASSET_PACK_FINALE_SHORT_ANCHOR_COUNT);
    tecmo_asset_pack_store_u16(payload + 70U, TECMO_ASSET_PACK_FINALE_ANCHOR_STRIDE);
    tecmo_asset_pack_store_u32(payload + 72U, TECMO_ASSET_PACK_FINALE_SHORT_ANCHORS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 76U, TECMO_ASSET_PACK_FINALE_REVERSE_METADATA_SIZE);
    tecmo_asset_pack_store_u16(payload + 78U, TECMO_ASSET_PACK_FINALE_TITLE_METADATA_SIZE);
    tecmo_asset_pack_store_u32(payload + 80U, TECMO_ASSET_PACK_FINALE_REVERSE_METADATA_OFFSET);
    tecmo_asset_pack_store_u32(payload + 84U, TECMO_ASSET_PACK_FINALE_TITLE_METADATA_OFFSET);
    tecmo_asset_pack_store_u16(payload + 88U, TECMO_ASSET_PACK_FINALE_TITLE_SLOT_COUNT);
    tecmo_asset_pack_store_u16(payload + 90U, TECMO_ASSET_PACK_FINALE_TITLE_TEXT_SLOT_COUNT);
    tecmo_asset_pack_store_u16(payload + 92U, TECMO_ASSET_PACK_FINALE_TITLE_SLOT_STRIDE);
    tecmo_asset_pack_store_u16(payload + 94U, TECMO_ASSET_PACK_FINALE_TITLE_CELL_COUNT);
    tecmo_asset_pack_store_u32(payload + 96U, TECMO_ASSET_PACK_FINALE_TITLE_SLOTS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 100U, TECMO_ASSET_PACK_FINALE_BAND_COUNT);
    tecmo_asset_pack_store_u16(payload + 102U, TECMO_ASSET_PACK_FINALE_BAND_STRIDE);
    tecmo_asset_pack_store_u32(payload + 104U, TECMO_ASSET_PACK_FINALE_BANDS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 108U, 1U);
    tecmo_asset_pack_store_u16(payload + 110U, 1U);
    tecmo_asset_pack_store_u32(payload + 112U, TECMO_ASSET_PACK_FINALE_SIZE);

    provenance->dispatch_offset = bank04_offset + (0x82CFU - 0x8000U);
    provenance->short_anchor_table_offset = bank04_offset + (0x8463U - 0x8000U);
    provenance->reverse_selector_offset = bank04_offset + (0x852FU - 0x8000U);
    provenance->reverse_indexed_operand_offset[0] = bank04_offset + (0x863EU - 0x8000U);
    provenance->reverse_indexed_operand_offset[1] = bank04_offset + (0x8640U - 0x8000U);
    provenance->reverse_indexed_operand_offset[2] = bank04_offset + (0x8642U - 0x8000U);
    provenance->reverse_indexed_operand_offset[3] = bank04_offset + (0x8644U - 0x8000U);
    provenance->sprite_pointer_offset = bank00_offset +
                                        (TECMO_ASSET_PACK_FINALE_POINTER_TABLE_CPU - 0x8000U) + 2U;
    provenance->sprite_stream_offset = piece_stream_offset;
    provenance->title_code_offset = bank04_offset + (0x8310U - 0x8000U);
    provenance->title_source_offset = title_source_offset;
    provenance->glyph_map_offset = bank06_offset +
                                   (TECMO_ASSET_PACK_FINALE_GLYPH_MAP_CPU - 0x8000U);
    provenance->glyph_table_offset = bank06_offset +
                                     (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U);
    provenance->fixed_irq_offset = fixed_offset +
                                   (TECMO_ASSET_PACK_FINALE_FIXED_IRQ_CPU - 0xC000U);
    return 0;
}
