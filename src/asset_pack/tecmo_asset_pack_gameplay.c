#include "tecmo_asset_pack_gameplay.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define GAMEPLAY_PRG_BANK_COUNT 8U
#define GAMEPLAY_FIXED_BANK 7U
#define GAMEPLAY_DESCRIPTOR_TABLE_CPU 0xDC85U
#define GAMEPLAY_ACTOR_POINTER_CPU 0xA5B9U
#define GAMEPLAY_ACTOR_POINTER_END_CPU 0xAEEFU
#define GAMEPLAY_ACTOR_RECORD_CPU 0x8000U
#define GAMEPLAY_ACTOR_RECORD_END_CPU 0xA5B9U

const TecmoGameplayExpectedScreen
    tecmo_gameplay_expected_screens[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT] = {
        {0x1BU, 1U, 0xDD42U, 0xB5EBU, 0xB7C4U, 474U,
         0x91D672B6U, 0xBA877A5FU, 0xA1C087E8U, 0x70952408U,
         {0x16U, 0x17U, 0xC4U, 0xB7U, 0xEBU, 0xB5U, 0x01U}},
        {0x2EU, 4U, 0xDDC7U, 0xBD71U, 0xBF52U, 482U,
         0xF36FE735U, 0x9ABA1FA9U, 0xE623A51AU, 0x70952408U,
         {0x16U, 0x17U, 0x52U, 0xBFU, 0x71U, 0xBDU, 0x04U}}
    };

const TecmoGameplayExpectedSource
    tecmo_gameplay_expected_sources[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_SOURCE_ACTOR_RECORDS, 1U, 0U, 0x8000U, 9657U,
         TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET, 0x41472384U},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_POINTERS, 1U, 0U, 0xA5B9U, 2358U,
         TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET, 0xABB3133DU},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_AEEF_DATA, 1U, 0U, 0xAEEFU, 229U,
         TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_OFFSET, 0x133461D5U},
        {TECMO_GAMEPLAY_SOURCE_BANK01_OAM_RENDERER, 1U, 0U, 0xAFD4U, 46U,
         TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_OFFSET, 0xBBA0B94BU},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_SETUP, 1U, 0U, 0xB0EDU, 71U,
         TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_OFFSET, 0x55B90F03U},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_POINTERS, 1U, 0U, 0xB134U, 4U,
         TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_POINTERS_OFFSET, 0xFCB596AFU},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_GROUPS, 1U, 0U, 0xB138U, 32U,
         TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET, 0x740B4855U},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_RENDERER, GAMEPLAY_FIXED_BANK, 1U,
         0xD413U, 334U, TECMO_ASSET_PACK_GAMEPLAY_RENDERER_OFFSET, 0xD487D107U},
        {TECMO_GAMEPLAY_SOURCE_ACTOR_RENDER_STAGING, GAMEPLAY_FIXED_BANK, 1U,
         0xF1F2U, 91U, TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_OFFSET, 0xA93E123BU},
        {TECMO_GAMEPLAY_SOURCE_SPRITE_R2_SELECTORS, GAMEPLAY_FIXED_BANK, 1U,
         0xF24DU, 8U, TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET, 0x6E11429DU},
        {TECMO_GAMEPLAY_SOURCE_RULE_SETUP, 5U, 0U, 0x81F2U, 272U,
         19290U, 0x00EAE8BAU},
        {TECMO_GAMEPLAY_SOURCE_RULE_LOOKUP, 5U, 0U, 0x8351U, 78U,
         19562U, 0x49B11D9AU},
        {TECMO_GAMEPLAY_SOURCE_RULE_SUBTYPE, 5U, 0U, 0x83E9U, 128U,
         19640U, 0xC668B1BFU},
        {TECMO_GAMEPLAY_SOURCE_RULE_ANIMATION, 5U, 0U, 0x86BBU, 224U,
         19768U, 0x15CFFC00U},
        {TECMO_GAMEPLAY_SOURCE_RULE_STATE, 5U, 0U, 0x8ABDU, 704U,
         19992U, 0x894D8796U},
        {TECMO_GAMEPLAY_SOURCE_RULE_SHOT_RESULT, 5U, 0U, 0x91BCU, 639U,
         20696U, 0x4A0C68ACU},
        {TECMO_GAMEPLAY_SOURCE_RULE_SHOT_LAUNCH, 5U, 0U, 0x9C40U, 138U,
         21335U, 0xDDA3E423U},
        {TECMO_GAMEPLAY_SOURCE_RULE_CLOSE_SHOT, 5U, 0U, 0xAF30U, 324U,
         21473U, 0xAE3EBA47U},
        {TECMO_GAMEPLAY_SOURCE_RULE_TRAJECTORY, 5U, 0U, 0xB52EU, 146U,
         21797U, 0xDB540670U},
        {TECMO_GAMEPLAY_SOURCE_RULE_FINISH, 5U, 0U, 0xB995U, 171U,
         21943U, 0xD5072F07U},
        {TECMO_GAMEPLAY_SOURCE_PERIOD_DISPATCH, 6U, 0U, 0xA05AU, 80U,
         TECMO_ASSET_PACK_GAMEPLAY_PERIOD_OFFSET, 0xF23BB5E7U},
        {TECMO_GAMEPLAY_SOURCE_PERIOD_POINTERS, 6U, 0U, 0xA0AAU, 12U,
         22194U, 0xAD887A58U},
        {TECMO_GAMEPLAY_SOURCE_PERIOD_STRINGS, 6U, 0U, 0xA0B6U, 62U,
         22206U, 0x90FB5BFBU},
        {TECMO_GAMEPLAY_SOURCE_SCOREBOARD_VIOLATIONS, 3U, 0U, 0xBE87U,
         290U, TECMO_ASSET_PACK_GAMEPLAY_EVENTS_OFFSET, 0xC8FFCCEDU},
        {TECMO_GAMEPLAY_SOURCE_FOUL_OVERLAY, 2U, 0U, 0xB0F8U, 441U,
         22558U, 0xFA6E116AU},
        {TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER, 6U, 0U, 0xBC3CU, 213U,
         22999U, 0x23B7AD01U},
        {TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_SELECT, GAMEPLAY_FIXED_BANK, 1U,
         0xE537U, 18U, TECMO_ASSET_PACK_GAMEPLAY_LIVE_OFFSET, 0xDB9972CEU},
        {TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_IDS, GAMEPLAY_FIXED_BANK, 1U,
         0xE699U, 2U, 23230U, 0xA1B4503CU},
        {TECMO_GAMEPLAY_SOURCE_LIVE_IRQ_ARM, GAMEPLAY_FIXED_BANK, 1U,
         0xCDACU, 37U, 23232U, 0x4EE3C545U},
        {TECMO_GAMEPLAY_SOURCE_LIVE_IRQ_BANDS, GAMEPLAY_FIXED_BANK, 1U,
         0xFE92U, 108U, 23269U, 0x49335A7EU},
        {TECMO_GAMEPLAY_SOURCE_LIVE_BAND_INIT, GAMEPLAY_FIXED_BANK, 1U,
         0xE937U, 39U, 23377U, 0x2E322A02U}
    };

static uint64_t fnv1a64(const uint8_t *bytes, size_t size)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool range_ok(uint64_t offset, uint64_t size, uint64_t total)
{
    return offset <= total && size <= total - offset;
}

static uint64_t bank_cpu_offset(uint64_t prg_offset,
                                uint32_t bank,
                                uint32_t cpu)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0x8000U);
}

static uint64_t fixed_cpu_offset(uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 uint32_t cpu)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) *
                            TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0xC000U);
}

static void store_u64(uint8_t *bytes, uint64_t value)
{
    tecmo_asset_pack_store_u32(bytes, (uint32_t)value);
    tecmo_asset_pack_store_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static int validate_actor_pointer_records(const uint8_t *actor_records,
                                          const uint8_t *pointer_layout,
                                          char *message,
                                          size_t message_size)
{
    size_t pointer_bytes = (size_t)(GAMEPLAY_ACTOR_POINTER_END_CPU -
                                    GAMEPLAY_ACTOR_POINTER_CPU);
    if (pointer_bytes != TECMO_GAMEPLAY_ASSET_POINTER_COUNT * 2U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGPL-1 pointer-table size is inconsistent.");
        return -1;
    }
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_POINTER_COUNT; ++index) {
        uint16_t target = tecmo_asset_pack_read_u16(pointer_layout + index * 2U);
        size_t record_offset;
        uint8_t dimensions;
        unsigned width;
        unsigned height;
        size_t cell_count;
        if (target < GAMEPLAY_ACTOR_RECORD_CPU ||
            target >= GAMEPLAY_ACTOR_RECORD_END_CPU) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGPL-1 actor pointer %u targets $%04X outside its record block.",
                (unsigned)index, (unsigned)target);
            return -1;
        }
        record_offset = (size_t)(target - GAMEPLAY_ACTOR_RECORD_CPU);
        dimensions = actor_records[record_offset];
        width = dimensions & 0x0FU;
        height = dimensions >> 4U;
        cell_count = (size_t)width * height;
        if (width == 0U || height == 0U ||
            cell_count > TECMO_GAMEPLAY_RESOLVED_PIECE_MAX ||
            record_offset + 4U > TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE ||
            cell_count > TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE -
                             record_offset - 4U) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGPL-1 actor pointer %u has an invalid bounded grid record.",
                (unsigned)index);
            return -1;
        }
    }
    return 0;
}

static int verify_source(const uint8_t *rom,
                         uint64_t rom_size,
                         uint64_t prg_offset,
                         uint32_t prg_banks,
                         const TecmoGameplayExpectedSource *source,
                         uint64_t *source_offset_out,
                         char *message,
                         size_t message_size)
{
    uint64_t source_offset = source->fixed_bank != 0U
        ? fixed_cpu_offset(prg_offset, prg_banks, source->cpu_start)
        : bank_cpu_offset(prg_offset, source->bank, source->cpu_start);
    uint32_t fingerprint;
    if (!range_ok(source_offset, source->byte_count, rom_size)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGPL-1 source span is outside the ROM.");
        return -1;
    }
    fingerprint = tecmo_asset_pack_fnv1a32(
        rom + (size_t)source_offset, source->byte_count);
    if (fingerprint != source->fingerprint) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGPL-1 %s Bank%02u $%04X/%u fingerprint mismatch (got %08X, expected %08X).",
            source->fixed_bank != 0U ? "fixed" : "switched",
            (unsigned)source->bank, (unsigned)source->cpu_start,
            (unsigned)source->byte_count, fingerprint, source->fingerprint);
        return -1;
    }
    *source_offset_out = source_offset;
    return 0;
}

int tecmo_asset_pack_build_gameplay(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayProvenance *provenance,
    char *message,
    size_t message_size)
{
    size_t screen_encoded_offset = TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET;
    size_t screen_decoded_offset = TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET;
    size_t screen_palette_offset = TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET;
    uint32_t rule_count = 0U;

    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        prg_banks != GAMEPLAY_PRG_BANK_COUNT ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGPL-1 gameplay import requires the exact Rev1 ROM contract.");
        return -1;
    }
    if (!range_ok(chr_offset, chr_size, rom_size) ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 ||
        fnv1a64(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGPL-1 full CHR revision fingerprint mismatch.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    provenance->chr_offset = chr_offset;

    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++index) {
        const TecmoGameplayExpectedScreen *expected =
            &tecmo_gameplay_expected_screens[index];
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks, expected->descriptor_cpu);
        uint64_t stream_offset = bank_cpu_offset(
            prg_offset, expected->source_bank, expected->stream_cpu);
        uint64_t palette_offset = bank_cpu_offset(
            prg_offset, expected->source_bank, expected->palette_cpu);
        uint64_t stream_bank_offset = prg_offset +
            (uint64_t)expected->source_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint8_t decoded[2048];
        uint8_t *record = payload + TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET +
                          index * TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE;
        size_t encoded_size = 0U;
        if (!range_ok(descriptor_offset, 7U, rom_size) ||
            !range_ok(stream_offset, expected->encoded_size, rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(rom + (size_t)descriptor_offset, expected->descriptor, 7U) != 0 ||
            tecmo_asset_pack_fnv1a32(rom + (size_t)descriptor_offset, 7U) !=
                expected->descriptor_fingerprint ||
            tecmo_asset_pack_fnv1a32(rom + (size_t)stream_offset,
                                     expected->encoded_size) !=
                expected->encoded_fingerprint ||
            tecmo_asset_pack_fnv1a32(rom + (size_t)palette_offset, 16U) !=
                expected->palette_fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGPL-1 screen $%02X source fingerprint mismatch.",
                (unsigned)expected->screen_id);
            return -1;
        }
        if (tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)stream_bank_offset,
                (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES,
                (size_t)(expected->stream_cpu - 0x8000U), decoded,
                sizeof(decoded), &encoded_size, message, message_size) != 0 ||
            encoded_size != expected->encoded_size ||
            tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
                expected->decoded_fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGPL-1 screen $%02X decode contract mismatch.",
                (unsigned)expected->screen_id);
            return -1;
        }
        for (size_t color = 0U; color < 16U; ++color) {
            if (rom[(size_t)palette_offset + color] > 0x3FU) {
                tecmo_asset_pack_set_message(message, message_size,
                                             "TGPL-1 palette contains an invalid NES color.");
                return -1;
            }
        }

        record[0U] = expected->screen_id;
        record[1U] = expected->source_bank;
        memcpy(record + 2U, expected->descriptor, 7U);
        tecmo_asset_pack_store_u16(record + 10U, expected->descriptor_cpu);
        tecmo_asset_pack_store_u16(record + 12U, expected->stream_cpu);
        tecmo_asset_pack_store_u16(record + 14U, expected->palette_cpu);
        tecmo_asset_pack_store_u32(record + 16U, (uint32_t)screen_encoded_offset);
        tecmo_asset_pack_store_u32(record + 20U, expected->encoded_size);
        tecmo_asset_pack_store_u32(record + 24U, (uint32_t)screen_decoded_offset);
        tecmo_asset_pack_store_u32(record + 28U, (uint32_t)sizeof(decoded));
        tecmo_asset_pack_store_u32(record + 32U, (uint32_t)screen_palette_offset);
        tecmo_asset_pack_store_u32(record + 36U, 16U);
        /* These are orientation base nametables. Live play reinterprets their
           tile IDs through the IRQ-driven band mapping; descriptor CHR alone
           is only the tipoff close-up. */
        tecmo_asset_pack_store_u32(record + 40U, 1U);
        tecmo_asset_pack_store_u32(record + 44U, 0U);
        tecmo_asset_pack_store_u32(record + 48U,
                                   expected->descriptor_fingerprint);
        tecmo_asset_pack_store_u32(record + 52U,
                                   expected->encoded_fingerprint);
        tecmo_asset_pack_store_u32(record + 56U,
                                   expected->decoded_fingerprint);
        tecmo_asset_pack_store_u32(record + 60U,
                                   expected->palette_fingerprint);
        memcpy(payload + screen_encoded_offset,
               rom + (size_t)stream_offset, expected->encoded_size);
        memcpy(payload + screen_decoded_offset, decoded, sizeof(decoded));
        memcpy(payload + screen_palette_offset,
               rom + (size_t)palette_offset, 16U);
        provenance->descriptor_offsets[index] = descriptor_offset;
        provenance->stream_offsets[index] = stream_offset;
        provenance->stream_sizes[index] = expected->encoded_size;
        provenance->palette_offsets[index] = palette_offset;
        screen_encoded_offset += expected->encoded_size;
        screen_decoded_offset += sizeof(decoded);
        screen_palette_offset += 16U;
    }

    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        const TecmoGameplayExpectedSource *expected =
            &tecmo_gameplay_expected_sources[index];
        uint64_t source_offset;
        uint8_t *record = payload + TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET +
                          index * TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (verify_source(rom, rom_size, prg_offset, prg_banks, expected,
                          &source_offset, message, message_size) != 0 ||
            expected->payload_offset > payload_size ||
            expected->byte_count > payload_size - expected->payload_offset ||
            cpu_end > 0xFFFFU) {
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, expected->fingerprint);
        tecmo_asset_pack_store_u16(record + 20U, (uint16_t)cpu_end);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)source_offset, expected->byte_count);
        provenance->source_offsets[index] = source_offset;
        if (expected->kind >= TECMO_GAMEPLAY_SOURCE_RULE_SETUP &&
            expected->kind <= TECMO_GAMEPLAY_SOURCE_RULE_FINISH) {
            ++rule_count;
        }
    }

    if (rule_count != TECMO_GAMEPLAY_ASSET_RULE_COUNT ||
        validate_actor_pointer_records(
            payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET,
            payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET,
            message, message_size) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT; ++index) {
        if (payload[TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET + index] !=
            (uint8_t)(0x40U + index)) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TGPL-1 sprite R2 selector table mismatch.");
            return -1;
        }
    }
    for (size_t color = 0U;
         color < TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE; ++color) {
        if (payload[TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET + color] >
            0x3FU) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TGPL-1 actor palette group contains an invalid color.");
            return -1;
        }
    }

    memcpy(payload, "TGPL", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_VERSION);
    tecmo_asset_pack_store_u16(payload + 6U,
                               TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u16(payload + 12U,
                               TECMO_GAMEPLAY_ASSET_SCREEN_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U,
                               TECMO_GAMEPLAY_ASSET_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(payload + 16U,
                               TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE);
    tecmo_asset_pack_store_u16(payload + 18U,
                               TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(payload + 20U,
                               TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U,
                               TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U,
                               TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U,
                               TECMO_ASSET_PACK_GAMEPLAY_ENCODED_SIZE);
    tecmo_asset_pack_store_u32(payload + 36U,
                               TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U,
                               TECMO_ASSET_PACK_GAMEPLAY_DECODED_SIZE);
    tecmo_asset_pack_store_u32(payload + 44U,
                               TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 48U,
                               TECMO_ASSET_PACK_GAMEPLAY_PALETTES_SIZE);
    tecmo_asset_pack_store_u32(payload + 52U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 56U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE);
    tecmo_asset_pack_store_u32(payload + 60U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 64U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_POINTERS_SIZE);
    tecmo_asset_pack_store_u32(payload + 68U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_OFFSET);
    tecmo_asset_pack_store_u32(payload + 72U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_SIZE);
    tecmo_asset_pack_store_u32(payload + 76U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 80U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_SIZE);
    tecmo_asset_pack_store_u32(payload + 84U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_POINTERS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 88U,
                               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_POINTERS_SIZE);
    tecmo_asset_pack_store_u32(payload + 92U,
                               TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 96U,
                               TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE);
    tecmo_asset_pack_store_u32(payload + 100U,
                               TECMO_ASSET_PACK_GAMEPLAY_RENDERER_OFFSET);
    tecmo_asset_pack_store_u32(payload + 104U,
                               TECMO_ASSET_PACK_GAMEPLAY_RENDERER_SIZE);
    tecmo_asset_pack_store_u32(payload + 108U,
                               TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_OFFSET);
    tecmo_asset_pack_store_u32(payload + 112U,
                               TECMO_ASSET_PACK_GAMEPLAY_RENDER_STAGING_SIZE);
    tecmo_asset_pack_store_u32(payload + 116U,
                               TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET);
    tecmo_asset_pack_store_u32(payload + 120U,
                               TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_SIZE);
    tecmo_asset_pack_store_u32(payload + 124U,
                               TECMO_ASSET_PACK_GAMEPLAY_RULES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 128U,
                               TECMO_ASSET_PACK_GAMEPLAY_RULES_SIZE);
    tecmo_asset_pack_store_u32(payload + 140U,
                               TECMO_ASSET_PACK_GAMEPLAY_EVENTS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 144U,
                               TECMO_ASSET_PACK_GAMEPLAY_EVENTS_SIZE);
    tecmo_asset_pack_store_u32(payload + 148U,
                               TECMO_ASSET_PACK_GAMEPLAY_LIVE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 152U,
                               TECMO_ASSET_PACK_GAMEPLAY_LIVE_SIZE);
    tecmo_asset_pack_store_u32(payload + 156U,
                               TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 160U,
                               TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32);
    store_u64(payload + 164U, TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64);
    tecmo_asset_pack_store_u32(payload + 132U,
                               TECMO_ASSET_PACK_GAMEPLAY_PERIOD_OFFSET);
    tecmo_asset_pack_store_u32(payload + 136U,
                               TECMO_ASSET_PACK_GAMEPLAY_PERIOD_SIZE);
    tecmo_asset_pack_store_u16(payload + 172U,
                               TECMO_GAMEPLAY_ASSET_POINTER_COUNT);
    tecmo_asset_pack_store_u16(payload + 174U,
                               TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT);
    tecmo_asset_pack_store_u16(payload + 176U,
                               TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT);
    tecmo_asset_pack_store_u16(payload + 178U,
                               TECMO_GAMEPLAY_ASSET_RULE_COUNT);
    tecmo_asset_pack_store_u16(payload + 180U,
                               TECMO_GAMEPLAY_LIVE_BAND_COUNT);
    tecmo_asset_pack_store_u16(payload + 182U, 0U);
    tecmo_asset_pack_store_u32(payload + 184U, 0x41472384U);
    tecmo_asset_pack_store_u32(payload + 188U, 0xABB3133DU);
    tecmo_asset_pack_store_u32(payload + 192U, 0x133461D5U);
    tecmo_asset_pack_store_u32(payload + 196U, 0x55B90F03U);
    tecmo_asset_pack_store_u32(payload + 200U, 0xFCB596AFU);
    tecmo_asset_pack_store_u32(payload + 204U, 0x740B4855U);
    tecmo_asset_pack_store_u32(payload + 208U, 0xD487D107U);
    tecmo_asset_pack_store_u32(payload + 212U, 0xA93E123BU);
    tecmo_asset_pack_store_u32(payload + 216U, 0x6E11429DU);
    tecmo_asset_pack_store_u32(payload + 220U, 0x5041054FU);
    tecmo_asset_pack_store_u32(payload + 224U, 0xE77E8F44U);
    tecmo_asset_pack_store_u32(payload + 228U, 0xF4C8965CU);
    tecmo_asset_pack_store_u32(payload + 232U, 0x76401D85U);
    tecmo_asset_pack_store_u32(payload + 236U, 0x0000001FU);
    tecmo_asset_pack_store_u32(
        payload + 240U, TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 244U, TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_SIZE);
    tecmo_asset_pack_store_u32(payload + 248U, 0xBBA0B94BU);
    tecmo_asset_pack_store_u32(payload + 252U, 0x2397C99BU);

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGPL-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "Built strict ROM-derived TGPL-1 gameplay asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_self_test(char *message, size_t message_size)
{
    size_t rule_bytes = 0U;
    size_t period_bytes = 0U;
    size_t event_bytes = 0U;
    size_t live_bytes = 0U;
    unsigned rules = 0U;
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        const TecmoGameplayExpectedSource *source =
            &tecmo_gameplay_expected_sources[index];
        if (source->kind >= TECMO_GAMEPLAY_SOURCE_RULE_SETUP &&
            source->kind <= TECMO_GAMEPLAY_SOURCE_RULE_FINISH) {
            rule_bytes += source->byte_count;
            ++rules;
        }
        if (source->kind >= TECMO_GAMEPLAY_SOURCE_PERIOD_DISPATCH &&
            source->kind <= TECMO_GAMEPLAY_SOURCE_PERIOD_STRINGS) {
            period_bytes += source->byte_count;
        }
        if (source->kind >= TECMO_GAMEPLAY_SOURCE_SCOREBOARD_VIOLATIONS &&
            source->kind <= TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER) {
            event_bytes += source->byte_count;
        }
        if (source->kind >= TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_SELECT &&
            source->kind <= TECMO_GAMEPLAY_SOURCE_LIVE_BAND_INIT) {
            live_bytes += source->byte_count;
        }
    }
    if (rules != TECMO_GAMEPLAY_ASSET_RULE_COUNT ||
        rule_bytes != TECMO_ASSET_PACK_GAMEPLAY_RULES_SIZE ||
        period_bytes != TECMO_ASSET_PACK_GAMEPLAY_PERIOD_SIZE ||
        event_bytes != TECMO_ASSET_PACK_GAMEPLAY_EVENTS_SIZE ||
        live_bytes != TECMO_ASSET_PACK_GAMEPLAY_LIVE_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET +
                TECMO_GAMEPLAY_ASSET_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE !=
            TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_ACTOR_AEEF_DATA_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_BANK01_OAM_RENDERER_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_LIVE_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_LIVE_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_SIZE) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGPL-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGPL-1 layout self-test passed.");
    return 0;
}
