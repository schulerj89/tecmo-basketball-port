#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_court.h"

#include "asset_pack/tecmo_asset_pack_d9f6.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG 0x54474354U
#define TECMO_GAMEPLAY_COURT_FULL_MACRO_ROWS 15U
#define TECMO_GAMEPLAY_COURT_FULL_MACRO_COLUMNS 48U
#define TECMO_GAMEPLAY_COURT_LAYOUT_ROW_STRIDE 0x60U
#define TECMO_GAMEPLAY_COURT_MAXIMUM_MACRO_INDEX 360U
#define TECMO_GAMEPLAY_COURT_WORLD_UNIQUE_MACROS 346U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    return (uint64_t)read_u32(bytes) |
           ((uint64_t)read_u32(bytes + 4U) << 32U);
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t byte_count)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < byte_count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
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

static bool bytes_are_zero(const uint8_t *bytes, size_t byte_count)
{
    for (size_t index = 0U; index < byte_count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t byte_count, size_t total)
{
    return offset <= total && byte_count <= total - offset;
}

static bool reject(TecmoGameplayCourt *court, const char *message)
{
    free(court->storage);
    court->storage = NULL;
    court->storage_size = 0U;
    court->nametable = NULL;
    court->palette = NULL;
    court->available = false;
    (void)snprintf(court->status, sizeof(court->status), "%s",
                   message != NULL ? message : "TGCT-1 rejected");
    return false;
}

void tecmo_gameplay_court_init(TecmoGameplayCourt *court)
{
    if (court == NULL) return;
    memset(court, 0, sizeof(*court));
    court->lifecycle_tag = TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG;
}

void tecmo_gameplay_court_destroy(TecmoGameplayCourt *court)
{
    if (court == NULL ||
        court->lifecycle_tag != TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG) {
        return;
    }
    free(court->storage);
    tecmo_gameplay_court_init(court);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE &&
           memcmp(payload, "TGCT", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET &&
           read_u32(payload + 32U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET &&
           read_u32(payload + 40U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET &&
           read_u32(payload + 48U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE &&
           read_u16(payload + 52U) == TECMO_GAMEPLAY_COURT_WIDTH &&
           read_u16(payload + 54U) == TECMO_GAMEPLAY_COURT_HEIGHT &&
           read_u16(payload + 56U) == 15U &&
           read_u16(payload + 58U) == 16U &&
           read_u16(payload + 60U) == 0x60U &&
           read_u16(payload + 62U) == 0x20U &&
           read_u16(payload + 64U) == 360U &&
           read_u16(payload + 66U) == 130U &&
           read_u16(payload + 68U) == 0x0FU &&
           read_u16(payload + 70U) == 0U &&
           read_u32(payload + 72U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_FNV1A32 &&
           read_u32(payload + 76U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32 &&
           read_u32(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_TILES_FNV1A32 &&
           read_u32(payload + 84U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_ATTRIBUTES_FNV1A32 &&
           read_u32(payload + 88U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_FNV1A32 &&
           read_u32(payload + 92U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE &&
           read_u32(payload + 96U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32 &&
           read_u64(payload + 100U) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64 &&
           read_u32(payload + 108U) == 0x98634D94U &&
           read_u32(payload + 112U) == 0xCD524DB3U &&
           read_u32(payload + 116U) == 0x578BFD90U &&
           read_u32(payload + 120U) == 0xA6CBF6F7U &&
           read_u32(payload + 124U) == 0x2BE5CD2FU &&
           read_u32(payload + 128U) == 0xD9379154U &&
           read_u32(payload + 132U) == 0x2779098EU &&
           read_u32(payload + 136U) == 0xEEC27A7DU &&
           read_u32(payload + 140U) == 0xC869A670U &&
           read_u16(payload + 144U) == 0x99C6U &&
           read_u16(payload + 146U) == 0x9F6AU &&
           read_u16(payload + 148U) == 0xA0D3U &&
           read_u16(payload + 150U) == 0x93C6U &&
           read_u16(payload + 152U) == 0xB5E0U &&
           read_u16(payload + 154U) == 0xF2E2U &&
           read_u32(payload + 156U) == 0x00000007U &&
           bytes_are_zero(payload + 160U, 96U);
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtExpectedSource *expected =
            &tecmo_gameplay_court_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != 0U ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->payload_offset ||
            read_u32(record + 16U) != expected->fingerprint ||
            read_u16(record + 20U) != (uint16_t)cpu_end ||
            !bytes_are_zero(record + 22U, 10U) ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_generated_data(const uint8_t *payload)
{
    static const uint8_t expected_descriptor[7] = {
        0x7DU, 0x7DU, 0xE0U, 0xB5U, 0x18U, 0xB5U, 0x00U
    };
    uint8_t decoded[TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE];
    uint8_t rebuilt[TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE];
    size_t encoded_size = 0U;
    uint16_t minimum;
    uint16_t maximum;
    uint16_t unique;
    char message[160];
    const uint8_t *tuple = payload +
        tecmo_gameplay_court_expected_sources[3].payload_offset;
    const uint8_t *palette = payload +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET;

    if (memcmp(payload +
                   tecmo_gameplay_court_expected_sources[0].payload_offset,
               expected_descriptor, sizeof(expected_descriptor)) != 0 ||
        read_u16(tuple) != 0x99C6U ||
        read_u16(tuple + 2U) != 0x9F6AU ||
        read_u16(tuple + 4U) != 0xA0D3U ||
        read_u16(tuple + 6U) != 0x93C6U) {
        return false;
    }
    if (tecmo_asset_pack_decode_d9f6_stream(
            payload + tecmo_gameplay_court_expected_sources[1].payload_offset,
            tecmo_gameplay_court_expected_sources[1].byte_count, 0U,
            decoded, sizeof(decoded), &encoded_size,
            message, sizeof(message)) != 0 ||
        encoded_size !=
            tecmo_gameplay_court_expected_sources[1].byte_count ||
        fnv1a32(decoded, sizeof(decoded)) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_FNV1A32 ||
        memcmp(decoded,
               payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET,
               sizeof(decoded)) != 0) {
        return false;
    }
    if (tecmo_asset_pack_build_gameplay_court_nametable(
            payload + tecmo_gameplay_court_expected_sources[4].payload_offset,
            tecmo_gameplay_court_expected_sources[4].byte_count,
            payload + tecmo_gameplay_court_expected_sources[5].payload_offset,
            tecmo_gameplay_court_expected_sources[5].byte_count,
            payload + tecmo_gameplay_court_expected_sources[6].payload_offset,
            tecmo_gameplay_court_expected_sources[6].byte_count,
            rebuilt, sizeof(rebuilt), &minimum, &maximum, &unique,
            message, sizeof(message)) != 0 ||
        minimum != 0U || maximum != 360U || unique != 130U ||
        memcmp(rebuilt,
               payload + TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET,
               sizeof(rebuilt)) != 0 ||
        fnv1a32(rebuilt, sizeof(rebuilt)) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32 ||
        fnv1a32(rebuilt, 960U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_TILES_FNV1A32 ||
        fnv1a32(rebuilt + 960U, 64U) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ATTRIBUTES_FNV1A32 ||
        memcmp(palette,
               payload +
                   tecmo_gameplay_court_expected_sources[9].payload_offset,
               TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE) != 0 ||
        fnv1a32(palette, TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_FNV1A32) {
        return false;
    }
    for (size_t color = 0U;
         color < TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE; ++color) {
        if (palette[color] > 0x3FU) return false;
    }
    return true;
}

bool tecmo_gameplay_court_parse(TecmoGameplayCourt *court,
                                const uint8_t *payload,
                                size_t payload_size,
                                const uint8_t *chr_bytes,
                                size_t chr_size)
{
    uint8_t *storage;

    if (court == NULL ||
        court->lifecycle_tag != TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_court_destroy(court);
    if (payload == NULL || chr_bytes == NULL ||
        !validate_header(payload, payload_size)) {
        return reject(court, "TGCT-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32) {
        return reject(court, "TGCT-1 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload, payload_size) ||
        !validate_generated_data(payload)) {
        return reject(court, "TGCT-1 source/rebuild contract rejected");
    }
    if (chr_size != TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE ||
        fnv1a32(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32 ||
        fnv1a64(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64) {
        return reject(court, "TGCT-1 same-pack chr/all dependency rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) {
        return reject(court, "TGCT-1 allocation failed");
    }
    memcpy(storage, payload, payload_size);
    court->storage = storage;
    court->storage_size = payload_size;
    court->nametable = storage +
        TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET;
    court->palette = storage + TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET;
    court->minimum_macro_index = 0U;
    court->maximum_macro_index = read_u16(storage + 64U);
    court->unique_macro_count = read_u16(storage + 66U);
    court->nametable_fingerprint = read_u32(storage + 76U);
    court->palette_fingerprint = read_u32(storage + 88U);
    court->chr_fingerprint32 = read_u32(storage + 96U);
    court->chr_fingerprint64 = read_u64(storage + 100U);
    court->available = true;
    (void)snprintf(court->status, sizeof(court->status),
                   "TGCT-1 static court assetpack");
    return true;
}

bool tecmo_gameplay_court_load(TecmoGameplayCourt *court,
                               const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *chr = NULL;
    uint64_t payload_size = 0U;
    uint64_t chr_size = 0U;
    bool loaded;

    if (court == NULL ||
        court->lifecycle_tag != TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_court_destroy(court);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_COURT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE,
            &payload, &payload_size) != 0) {
        return reject(court,
                      "TGCT-1 gameplay/court entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE,
            &chr, &chr_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(court,
                      "TGCT-1 chr/all dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_court_parse(
        court, payload, (size_t)payload_size, chr, (size_t)chr_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(chr);
    return loaded;
}

const uint8_t *tecmo_gameplay_court_nametable(
    const TecmoGameplayCourt *court,
    size_t *byte_count_out)
{
    if (byte_count_out != NULL) *byte_count_out = 0U;
    if (court == NULL || !court->available || court->nametable == NULL) {
        return NULL;
    }
    if (byte_count_out != NULL) {
        *byte_count_out = TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE;
    }
    return court->nametable;
}

const uint8_t *tecmo_gameplay_court_palette(
    const TecmoGameplayCourt *court,
    size_t *byte_count_out)
{
    if (byte_count_out != NULL) *byte_count_out = 0U;
    if (court == NULL || !court->available || court->palette == NULL) {
        return NULL;
    }
    if (byte_count_out != NULL) {
        *byte_count_out = TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE;
    }
    return court->palette;
}

static bool court_world_source_contract_valid(
    const TecmoGameplayCourt *court)
{
    return court != NULL &&
           court->lifecycle_tag == TECMO_GAMEPLAY_COURT_LIFECYCLE_TAG &&
           court->available &&
           court->storage != NULL &&
           court->storage_size == TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE &&
           court->nametable ==
               court->storage +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET &&
           court->palette ==
               court->storage +
                   TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET &&
           court->minimum_macro_index == 0U &&
           court->maximum_macro_index == 360U &&
           court->unique_macro_count == 130U &&
           court->nametable_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32 &&
           court->palette_fingerprint ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_FNV1A32 &&
           court->chr_fingerprint32 ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32 &&
           court->chr_fingerprint64 ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64 &&
           fnv1a32(court->storage, court->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32;
}

static uint8_t legacy_palette_index(const uint8_t *nametable,
                                    size_t tile_row,
                                    size_t tile_column)
{
    size_t attribute_offset =
        960U + (tile_row / 4U) * 8U + tile_column / 4U;
    unsigned shift = ((tile_row & 2U) != 0U ? 4U : 0U) +
                     ((tile_column & 2U) != 0U ? 2U : 0U);
    return (uint8_t)((nametable[attribute_offset] >> shift) & 3U);
}

bool tecmo_gameplay_court_decode_world(
    const TecmoGameplayCourt *court,
    TecmoGameplayCourtWorld *world_out)
{
    TecmoGameplayCourtWorld decoded;
    bool seen[TECMO_GAMEPLAY_COURT_MAXIMUM_MACRO_INDEX + 1U] = {false};
    const uint8_t *layout;
    const uint8_t *macro_tiles;
    const uint8_t *macro_attributes;
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0U;
    uint16_t unique = 0U;

    if (world_out == NULL || !court_world_source_contract_valid(court)) {
        return false;
    }

    layout = court->storage +
        tecmo_gameplay_court_expected_sources[4U].payload_offset;
    macro_tiles = court->storage +
        tecmo_gameplay_court_expected_sources[5U].payload_offset;
    macro_attributes = court->storage +
        tecmo_gameplay_court_expected_sources[6U].payload_offset;
    memset(&decoded, 0, sizeof(decoded));

    for (size_t macro_row = 0U;
         macro_row < TECMO_GAMEPLAY_COURT_FULL_MACRO_ROWS; ++macro_row) {
        for (size_t macro_column = 0U;
             macro_column < TECMO_GAMEPLAY_COURT_FULL_MACRO_COLUMNS;
             ++macro_column) {
            size_t layout_offset =
                macro_row * TECMO_GAMEPLAY_COURT_LAYOUT_ROW_STRIDE +
                macro_column * 2U;
            uint16_t macro_index;
            size_t macro_tile_offset;
            size_t tile_row;
            size_t tile_column;
            uint8_t palette_index;

            if (!range_ok(layout_offset, 2U,
                          tecmo_gameplay_court_expected_sources[4U].
                              byte_count)) {
                return false;
            }
            macro_index = read_u16(layout + layout_offset);
            macro_tile_offset = (size_t)macro_index * 4U;
            if (macro_index > TECMO_GAMEPLAY_COURT_MAXIMUM_MACRO_INDEX ||
                !range_ok(
                    macro_tile_offset, 4U,
                    tecmo_gameplay_court_expected_sources[5U].byte_count) ||
                macro_index >=
                    tecmo_gameplay_court_expected_sources[6U].byte_count) {
                return false;
            }
            if (!seen[macro_index]) {
                seen[macro_index] = true;
                ++unique;
            }
            if (macro_index < minimum) minimum = macro_index;
            if (macro_index > maximum) maximum = macro_index;

            tile_row = macro_row * 2U;
            tile_column = macro_column * 2U;
            palette_index =
                (uint8_t)((macro_attributes[macro_index] & 0x0CU) >> 2U);
            decoded.tiles[
                tile_row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column] = macro_tiles[macro_tile_offset];
            decoded.tiles[
                tile_row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column + 1U] = macro_tiles[macro_tile_offset + 1U];
            decoded.tiles[
                (tile_row + 1U) *
                    TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column] = macro_tiles[macro_tile_offset + 2U];
            decoded.tiles[
                (tile_row + 1U) *
                    TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column + 1U] = macro_tiles[macro_tile_offset + 3U];
            decoded.palette_indices[
                tile_row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column] = palette_index;
            decoded.palette_indices[
                tile_row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column + 1U] = palette_index;
            decoded.palette_indices[
                (tile_row + 1U) *
                    TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column] = palette_index;
            decoded.palette_indices[
                (tile_row + 1U) *
                    TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                tile_column + 1U] = palette_index;
        }
    }

    decoded.contract_tag = TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG;
    decoded.width_tiles = TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES;
    decoded.height_tiles = TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES;
    decoded.width_pixels = TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS;
    decoded.height_pixels = TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS;
    decoded.minimum_macro_index = minimum;
    decoded.maximum_macro_index = maximum;
    decoded.unique_macro_count = unique;
    decoded.tiles_fingerprint =
        fnv1a32(decoded.tiles, sizeof(decoded.tiles));
    decoded.palette_indices_fingerprint =
        fnv1a32(decoded.palette_indices, sizeof(decoded.palette_indices));
    if (minimum != 0U ||
        maximum != TECMO_GAMEPLAY_COURT_MAXIMUM_MACRO_INDEX ||
        unique != TECMO_GAMEPLAY_COURT_WORLD_UNIQUE_MACROS ||
        decoded.tiles_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 ||
        decoded.palette_indices_fingerprint !=
            TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32) {
        return false;
    }

    /* TGCT-1's existing nametable is the exact center viewport beginning at
       world pixel X=$0100 (tile 32). Cross-check it independently so the
       wider interpretation cannot silently drift from the shipped boundary. */
    for (size_t row = 0U; row < TECMO_GAMEPLAY_COURT_HEIGHT; ++row) {
        for (size_t column = 0U;
             column < TECMO_GAMEPLAY_COURT_WIDTH; ++column) {
            size_t world_offset =
                row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES +
                32U + column;
            size_t legacy_offset =
                row * TECMO_GAMEPLAY_COURT_WIDTH + column;
            if (decoded.tiles[world_offset] !=
                    court->nametable[legacy_offset] ||
                decoded.palette_indices[world_offset] !=
                    legacy_palette_index(
                        court->nametable, row, column)) {
                return false;
            }
        }
    }

    *world_out = decoded;
    return true;
}

static bool world_contract_valid(const TecmoGameplayCourtWorld *world)
{
    return world != NULL &&
           world->contract_tag ==
               TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG &&
           world->width_tiles ==
               TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES &&
           world->height_tiles ==
               TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES &&
           world->width_pixels ==
               TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS &&
           world->height_pixels ==
               TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS &&
           world->minimum_macro_index == 0U &&
           world->maximum_macro_index ==
               TECMO_GAMEPLAY_COURT_MAXIMUM_MACRO_INDEX &&
           world->unique_macro_count ==
               TECMO_GAMEPLAY_COURT_WORLD_UNIQUE_MACROS &&
           world->reserved == 0U &&
           world->tiles_fingerprint ==
               TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 &&
           world->palette_indices_fingerprint ==
               TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32 &&
           fnv1a32(world->tiles, sizeof(world->tiles)) ==
               TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 &&
           fnv1a32(world->palette_indices,
                   sizeof(world->palette_indices)) ==
               TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32;
}

bool tecmo_gameplay_court_slice_viewport(
    const TecmoGameplayCourtWorld *world,
    uint16_t camera_x,
    TecmoGameplayCourtViewport *viewport_out)
{
    TecmoGameplayCourtViewport viewport;
    size_t first_tile_x;
    size_t column_count;

    if (viewport_out == NULL ||
        camera_x > TECMO_GAMEPLAY_COURT_MAX_CAMERA_X ||
        !world_contract_valid(world)) {
        return false;
    }

    memset(&viewport, 0, sizeof(viewport));
    first_tile_x = camera_x >> 3U;
    viewport.camera_x = camera_x;
    viewport.first_tile_x = (uint16_t)first_tile_x;
    viewport.fine_scroll_x = (uint8_t)(camera_x & 7U);
    viewport.column_count =
        viewport.fine_scroll_x == 0U ? 32U : 33U;
    viewport.tile_stride = TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE;
    viewport.height_tiles = TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES;
    column_count = viewport.column_count;

    if (first_tile_x + column_count >
        TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES) {
        return false;
    }
    for (size_t row = 0U;
         row < TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES; ++row) {
        size_t source_offset =
            row * TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES + first_tile_x;
        size_t viewport_offset =
            row * TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE;
        memcpy(viewport.tiles + viewport_offset,
               world->tiles + source_offset, column_count);
        memcpy(viewport.palette_indices + viewport_offset,
               world->palette_indices + source_offset, column_count);
    }
    viewport.tiles_fingerprint =
        fnv1a32(viewport.tiles, sizeof(viewport.tiles));
    viewport.palette_indices_fingerprint =
        fnv1a32(viewport.palette_indices,
                sizeof(viewport.palette_indices));
    *viewport_out = viewport;
    return true;
}
