#include "tecmo_asset_pack_gameplay_court.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define GAMEPLAY_COURT_PRG_BANK_COUNT 8U
#define GAMEPLAY_COURT_FIXED_BANK 7U
#define GAMEPLAY_COURT_LAYOUT_SIZE 1440U
#define GAMEPLAY_COURT_MACRO_TILES_SIZE 1444U
#define GAMEPLAY_COURT_MACRO_ATTRIBUTES_SIZE 361U
#define GAMEPLAY_COURT_MACRO_ROWS 15U
#define GAMEPLAY_COURT_MACRO_COLUMNS 16U
#define GAMEPLAY_COURT_LAYOUT_ROW_STRIDE 0x60U
#define GAMEPLAY_COURT_LAYOUT_DATA_OFFSET 0x20U
#define GAMEPLAY_COURT_MAXIMUM_INDEX 360U
#define GAMEPLAY_COURT_UNIQUE_INDEX_COUNT 130U
#define GAMEPLAY_COURT_SCREEN_ID 0x0FU
#define GAMEPLAY_COURT_SCREEN_STREAM_OFFSET 0x3518U
#define GAMEPLAY_COURT_CONTRACT_FLAGS 0x00000007U

const TecmoGameplayCourtExpectedSource
    tecmo_gameplay_court_expected_sources[
        TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_COURT_SOURCE_SCREEN_DESCRIPTOR,
         GAMEPLAY_COURT_FIXED_BANK, 1U, 0xDCEEU, 7U, 576U, 0xEEC27A7DU},
        {TECMO_GAMEPLAY_COURT_SOURCE_ENCODED_SCREEN,
         0U, 0U, 0xB518U, 201U, 583U, 0xC869A670U},
        {TECMO_GAMEPLAY_COURT_SOURCE_DESCRIPTOR_PALETTE,
         0U, 0U, 0xB5E0U, 16U, 784U, 0x98634D94U},
        {TECMO_GAMEPLAY_COURT_SOURCE_POINTER_TUPLE,
         0U, 0U, 0xABECU, 8U, 800U, 0xCD524DB3U},
        {TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT,
         0U, 0U, 0x93C6U, GAMEPLAY_COURT_LAYOUT_SIZE,
         808U, 0x578BFD90U},
        {TECMO_GAMEPLAY_COURT_SOURCE_MACRO_TILES,
         0U, 0U, 0x99C6U, GAMEPLAY_COURT_MACRO_TILES_SIZE,
         2248U, 0xA6CBF6F7U},
        {TECMO_GAMEPLAY_COURT_SOURCE_MACRO_ATTRIBUTES,
         0U, 0U, 0x9F6AU, GAMEPLAY_COURT_MACRO_ATTRIBUTES_SIZE,
         3692U, 0x2BE5CD2FU},
        {TECMO_GAMEPLAY_COURT_SOURCE_MACRO_BUILDER,
         GAMEPLAY_COURT_FIXED_BANK, 1U, 0xD5C5U, 315U,
         4053U, 0xD9379154U},
        {TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT_LOOP,
         GAMEPLAY_COURT_FIXED_BANK, 1U, 0xDE2DU, 111U,
         4368U, 0x2779098EU},
        {TECMO_GAMEPLAY_COURT_SOURCE_LIVE_PALETTE,
         GAMEPLAY_COURT_FIXED_BANK, 1U, 0xF2E2U, 16U,
         4479U, 0xB20C1E11U}
    };

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

static uint64_t fnv1a64(const uint8_t *bytes, size_t byte_count)
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t index = 0U; index < byte_count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void store_u64(uint8_t *bytes, uint64_t value)
{
    tecmo_asset_pack_store_u32(bytes, (uint32_t)value);
    tecmo_asset_pack_store_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

int tecmo_asset_pack_build_gameplay_court_nametable(
    const uint8_t *layout,
    size_t layout_size,
    const uint8_t *macro_tiles,
    size_t macro_tiles_size,
    const uint8_t *macro_attributes,
    size_t macro_attributes_size,
    uint8_t *nametable,
    size_t nametable_size,
    uint16_t *minimum_index_out,
    uint16_t *maximum_index_out,
    uint16_t *unique_count_out,
    char *message,
    size_t message_size)
{
    bool seen[GAMEPLAY_COURT_MAXIMUM_INDEX + 1U] = {false};
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0U;
    uint16_t unique = 0U;

    if (layout == NULL || macro_tiles == NULL || macro_attributes == NULL ||
        nametable == NULL ||
        layout_size != GAMEPLAY_COURT_LAYOUT_SIZE ||
        macro_tiles_size != GAMEPLAY_COURT_MACRO_TILES_SIZE ||
        macro_attributes_size != GAMEPLAY_COURT_MACRO_ATTRIBUTES_SIZE ||
        nametable_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 court builder requires exact source and output sizes.");
        return -1;
    }

    /* Fixed $D290 initializes both nametable pages to $FF before $DE61
       dispatches the 15x16 macro grid. The grid replaces all 960 visible
       tiles and only the visible top quadrants of the final attribute row. */
    memset(nametable, 0xFF, nametable_size);
    for (size_t row = 0U; row < GAMEPLAY_COURT_MACRO_ROWS; ++row) {
        for (size_t column = 0U;
             column < GAMEPLAY_COURT_MACRO_COLUMNS; ++column) {
            size_t layout_offset = row * GAMEPLAY_COURT_LAYOUT_ROW_STRIDE +
                                   GAMEPLAY_COURT_LAYOUT_DATA_OFFSET +
                                   column * 2U;
            uint16_t index;
            size_t tile_offset;
            size_t tile_row = row * 2U;
            size_t tile_column = column * 2U;
            size_t attribute_offset;
            unsigned shift;
            uint8_t palette;
            uint8_t mask;

            if (layout_offset + 2U > layout_size) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TGCT-1 court layout read escaped its bounded row.");
                return -1;
            }
            index = tecmo_asset_pack_read_u16(layout + layout_offset);
            if (index > GAMEPLAY_COURT_MAXIMUM_INDEX ||
                (size_t)index * 4U + 4U > macro_tiles_size ||
                index >= macro_attributes_size) {
                tecmo_asset_pack_set_messagef(
                    message, message_size,
                    "TGCT-1 court macro index %u is out of range.",
                    (unsigned)index);
                return -1;
            }
            if (!seen[index]) {
                seen[index] = true;
                ++unique;
            }
            if (index < minimum) minimum = index;
            if (index > maximum) maximum = index;

            tile_offset = (size_t)index * 4U;
            nametable[tile_row * TECMO_GAMEPLAY_COURT_WIDTH + tile_column] =
                macro_tiles[tile_offset];
            nametable[tile_row * TECMO_GAMEPLAY_COURT_WIDTH +
                      tile_column + 1U] = macro_tiles[tile_offset + 1U];
            nametable[(tile_row + 1U) * TECMO_GAMEPLAY_COURT_WIDTH +
                      tile_column] = macro_tiles[tile_offset + 2U];
            nametable[(tile_row + 1U) * TECMO_GAMEPLAY_COURT_WIDTH +
                      tile_column + 1U] = macro_tiles[tile_offset + 3U];

            palette = (uint8_t)((macro_attributes[index] & 0x0CU) >> 2U);
            attribute_offset = 960U + (tile_row / 4U) * 8U +
                               tile_column / 4U;
            shift = ((tile_row & 2U) != 0U ? 4U : 0U) +
                    ((tile_column & 2U) != 0U ? 2U : 0U);
            mask = (uint8_t)(3U << shift);
            nametable[attribute_offset] = (uint8_t)(
                (nametable[attribute_offset] & (uint8_t)~mask) |
                (uint8_t)(palette << shift));
        }
    }

    if (minimum_index_out != NULL) *minimum_index_out = minimum;
    if (maximum_index_out != NULL) *maximum_index_out = maximum;
    if (unique_count_out != NULL) *unique_count_out = unique;
    tecmo_asset_pack_set_message(message, message_size,
                                 "Built bounded TGCT-1 static court base.");
    return 0;
}

static int verify_source(const uint8_t *rom,
                         uint64_t rom_size,
                         uint64_t prg_offset,
                         uint32_t prg_banks,
                         const TecmoGameplayCourtExpectedSource *source,
                         uint64_t *source_offset_out,
                         char *message,
                         size_t message_size)
{
    uint64_t offset = source->fixed_bank != 0U
        ? fixed_cpu_offset(prg_offset, prg_banks, source->cpu_start)
        : bank_cpu_offset(prg_offset, source->bank, source->cpu_start);
    uint32_t fingerprint;

    if (!range_ok(offset, source->byte_count, rom_size)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 source span is outside the ROM.");
        return -1;
    }
    fingerprint = tecmo_asset_pack_fnv1a32(
        rom + (size_t)offset, source->byte_count);
    if (fingerprint != source->fingerprint) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCT-1 %s Bank%02u $%04X/%u fingerprint mismatch "
            "(got %08X, expected %08X).",
            source->fixed_bank != 0U ? "fixed" : "switched",
            (unsigned)source->bank, (unsigned)source->cpu_start,
            (unsigned)source->byte_count, fingerprint,
            source->fingerprint);
        return -1;
    }
    *source_offset_out = offset;
    return 0;
}

int tecmo_asset_pack_build_gameplay_court(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCourtProvenance *provenance,
    char *message,
    size_t message_size)
{
    static const uint8_t expected_descriptor[7] = {
        0x7DU, 0x7DU, 0xE0U, 0xB5U, 0x18U, 0xB5U, 0x00U
    };
    uint8_t decoded[TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE];
    size_t encoded_size = 0U;
    uint16_t minimum;
    uint16_t maximum;
    uint16_t unique;
    uint32_t nametable_fingerprint;
    uint32_t tile_fingerprint;
    uint32_t attribute_fingerprint;

    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE ||
        prg_banks != GAMEPLAY_COURT_PRG_BANK_COUNT ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 court import requires the exact Rev1 ROM contract.");
        return -1;
    }
    if (!range_ok(chr_offset, chr_size, rom_size) ||
        tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset,
                                 (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32 ||
        fnv1a64(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 full CHR revision fingerprint mismatch.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    provenance->chr_offset = chr_offset;
    for (size_t index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtExpectedSource *source =
            &tecmo_gameplay_court_expected_sources[index];
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE;
        uint64_t source_offset;
        uint32_t cpu_end = (uint32_t)source->cpu_start +
                           source->byte_count - 1U;

        if (verify_source(rom, rom_size, prg_offset, prg_banks, source,
                          &source_offset, message, message_size) != 0) {
            return -1;
        }
        provenance->source_offsets[index] = source_offset;
        memcpy(payload + source->payload_offset,
               rom + (size_t)source_offset, source->byte_count);
        tecmo_asset_pack_store_u16(record, (uint16_t)source->kind);
        record[2U] = source->bank;
        record[3U] = source->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, source->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, 0U);
        tecmo_asset_pack_store_u32(record + 8U, source->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, source->payload_offset);
        tecmo_asset_pack_store_u32(record + 16U, source->fingerprint);
        tecmo_asset_pack_store_u16(record + 20U, (uint16_t)cpu_end);
    }

    if (memcmp(payload +
                   tecmo_gameplay_court_expected_sources[0].payload_offset,
               expected_descriptor, sizeof(expected_descriptor)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 screen $0F descriptor bytes changed.");
        return -1;
    }
    {
        const uint8_t *tuple = payload +
            tecmo_gameplay_court_expected_sources[3].payload_offset;
        if (tecmo_asset_pack_read_u16(tuple) != 0x99C6U ||
            tecmo_asset_pack_read_u16(tuple + 2U) != 0x9F6AU ||
            tecmo_asset_pack_read_u16(tuple + 4U) != 0xA0D3U ||
            tecmo_asset_pack_read_u16(tuple + 6U) != 0x93C6U) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGCT-1 court pointer tuple changed.");
            return -1;
        }
    }

    if (tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)prg_offset, TECMO_ASSET_PACK_PRG_BANK_BYTES,
            GAMEPLAY_COURT_SCREEN_STREAM_OFFSET, decoded, sizeof(decoded),
            &encoded_size, message, message_size) != 0 ||
        encoded_size !=
            tecmo_gameplay_court_expected_sources[1].byte_count ||
        tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGCT-1 screen $0F decode contract changed.");
        return -1;
    }
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET,
           decoded, sizeof(decoded));

    if (tecmo_asset_pack_build_gameplay_court_nametable(
            payload + tecmo_gameplay_court_expected_sources[4].payload_offset,
            GAMEPLAY_COURT_LAYOUT_SIZE,
            payload + tecmo_gameplay_court_expected_sources[5].payload_offset,
            GAMEPLAY_COURT_MACRO_TILES_SIZE,
            payload + tecmo_gameplay_court_expected_sources[6].payload_offset,
            GAMEPLAY_COURT_MACRO_ATTRIBUTES_SIZE,
            payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE,
            &minimum, &maximum, &unique, message, message_size) != 0) {
        return -1;
    }
    nametable_fingerprint = tecmo_asset_pack_fnv1a32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE);
    tile_fingerprint = tecmo_asset_pack_fnv1a32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET, 960U);
    attribute_fingerprint = tecmo_asset_pack_fnv1a32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET + 960U,
        64U);
    if (minimum != 0U || maximum != GAMEPLAY_COURT_MAXIMUM_INDEX ||
        unique != GAMEPLAY_COURT_UNIQUE_INDEX_COUNT ||
        nametable_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32 ||
        tile_fingerprint != TECMO_ASSET_PACK_GAMEPLAY_COURT_TILES_FNV1A32 ||
        attribute_fingerprint !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ATTRIBUTES_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCT-1 canonical static court rebuild changed "
            "(all=%08X tiles=%08X attrs=%08X min=%u max=%u unique=%u).",
            nametable_fingerprint, tile_fingerprint, attribute_fingerprint,
            (unsigned)minimum, (unsigned)maximum, (unsigned)unique);
        return -1;
    }

    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET,
           payload + tecmo_gameplay_court_expected_sources[9].payload_offset,
           TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE);
    for (size_t color = 0U;
         color < TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE; ++color) {
        if (payload[TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET + color] >
            0x3FU) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGCT-1 live background palette contains an invalid color.");
            return -1;
        }
    }

    memcpy(payload, "TGCT", 4U);
    tecmo_asset_pack_store_u16(payload + 4U,
                               TECMO_ASSET_PACK_GAMEPLAY_COURT_VERSION);
    tecmo_asset_pack_store_u16(payload + 6U,
                               TECMO_ASSET_PACK_GAMEPLAY_COURT_HEADER_SIZE);
    tecmo_asset_pack_store_u32(payload + 8U,
                               TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 48U, TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE);
    tecmo_asset_pack_store_u16(payload + 52U, TECMO_GAMEPLAY_COURT_WIDTH);
    tecmo_asset_pack_store_u16(payload + 54U, TECMO_GAMEPLAY_COURT_HEIGHT);
    tecmo_asset_pack_store_u16(payload + 56U, GAMEPLAY_COURT_MACRO_ROWS);
    tecmo_asset_pack_store_u16(payload + 58U, GAMEPLAY_COURT_MACRO_COLUMNS);
    tecmo_asset_pack_store_u16(payload + 60U,
                               GAMEPLAY_COURT_LAYOUT_ROW_STRIDE);
    tecmo_asset_pack_store_u16(payload + 62U,
                               GAMEPLAY_COURT_LAYOUT_DATA_OFFSET);
    tecmo_asset_pack_store_u16(payload + 64U, maximum);
    tecmo_asset_pack_store_u16(payload + 66U, unique);
    tecmo_asset_pack_store_u16(payload + 68U, GAMEPLAY_COURT_SCREEN_ID);
    tecmo_asset_pack_store_u16(payload + 70U, 0U);
    tecmo_asset_pack_store_u32(
        payload + 72U, TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 76U, TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 80U, TECMO_ASSET_PACK_GAMEPLAY_COURT_TILES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 84U, TECMO_ASSET_PACK_GAMEPLAY_COURT_ATTRIBUTES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 88U, TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 92U, TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 96U, TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32);
    store_u64(payload + 100U,
              TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64);
    tecmo_asset_pack_store_u32(payload + 108U, 0x98634D94U);
    tecmo_asset_pack_store_u32(payload + 112U, 0xCD524DB3U);
    tecmo_asset_pack_store_u32(payload + 116U, 0x578BFD90U);
    tecmo_asset_pack_store_u32(payload + 120U, 0xA6CBF6F7U);
    tecmo_asset_pack_store_u32(payload + 124U, 0x2BE5CD2FU);
    tecmo_asset_pack_store_u32(payload + 128U, 0xD9379154U);
    tecmo_asset_pack_store_u32(payload + 132U, 0x2779098EU);
    tecmo_asset_pack_store_u32(payload + 136U, 0xEEC27A7DU);
    tecmo_asset_pack_store_u32(payload + 140U, 0xC869A670U);
    tecmo_asset_pack_store_u16(payload + 144U, 0x99C6U);
    tecmo_asset_pack_store_u16(payload + 146U, 0x9F6AU);
    tecmo_asset_pack_store_u16(payload + 148U, 0xA0D3U);
    tecmo_asset_pack_store_u16(payload + 150U, 0x93C6U);
    tecmo_asset_pack_store_u16(payload + 152U, 0xB5E0U);
    tecmo_asset_pack_store_u16(payload + 154U, 0xF2E2U);
    tecmo_asset_pack_store_u32(payload + 156U,
                               GAMEPLAY_COURT_CONTRACT_FLAGS);

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGCT-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGCT-1 static court asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_court_self_test(char *message,
                                               size_t message_size)
{
    uint8_t layout[GAMEPLAY_COURT_LAYOUT_SIZE] = {0U};
    uint8_t macro_tiles[GAMEPLAY_COURT_MACRO_TILES_SIZE] = {0U};
    uint8_t macro_attributes[GAMEPLAY_COURT_MACRO_ATTRIBUTES_SIZE] = {0U};
    uint8_t nametable[TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE];
    uint16_t minimum;
    uint16_t maximum;
    uint16_t unique;

    macro_tiles[0U] = 1U;
    macro_tiles[1U] = 2U;
    macro_tiles[2U] = 3U;
    macro_tiles[3U] = 4U;
    macro_attributes[0U] = 0x0CU;
    if (TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE ||
        tecmo_asset_pack_build_gameplay_court_nametable(
            layout, sizeof(layout), macro_tiles, sizeof(macro_tiles),
            macro_attributes, sizeof(macro_attributes), nametable,
            sizeof(nametable), &minimum, &maximum, &unique,
            message, message_size) != 0 ||
        minimum != 0U || maximum != 0U || unique != 1U ||
        nametable[0U] != 1U || nametable[1U] != 2U ||
        nametable[32U] != 3U || nametable[33U] != 4U ||
        nametable[960U] != 0xFFU || nametable[1023U] != 0xFFU) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TGCT-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TGCT-1 layout self-test passed.");
    return 0;
}
