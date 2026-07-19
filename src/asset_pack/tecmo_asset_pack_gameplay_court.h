#ifndef TECMO_ASSET_PACK_GAMEPLAY_COURT_H
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_H

#include "tecmo_gameplay_court.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ID "gameplay/court"
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT 10U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_OFFSET 576U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_RAW_SIZE 3919U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_OFFSET 4495U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_SIZE 1024U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_OFFSET 5519U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_SIZE 1024U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_OFFSET 6543U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_SIZE 16U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE 6559U

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE 262144U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A32 0xF6F6E854U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_FNV1A64 \
    0x96A64F53B240ABB4ULL

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_DECODED_FNV1A32 0x483171E7U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_NAMETABLE_FNV1A32 0x0CF54A0EU
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_TILES_FNV1A32 0xD2F8364AU
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ATTRIBUTES_FNV1A32 0xB54833D1U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_PALETTE_FNV1A32 0xB20C1E11U

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32 0xECAB7A93U

typedef enum TecmoGameplayCourtSourceKind {
    TECMO_GAMEPLAY_COURT_SOURCE_SCREEN_DESCRIPTOR = 1,
    TECMO_GAMEPLAY_COURT_SOURCE_ENCODED_SCREEN = 2,
    TECMO_GAMEPLAY_COURT_SOURCE_DESCRIPTOR_PALETTE = 3,
    TECMO_GAMEPLAY_COURT_SOURCE_POINTER_TUPLE = 4,
    TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT = 5,
    TECMO_GAMEPLAY_COURT_SOURCE_MACRO_TILES = 6,
    TECMO_GAMEPLAY_COURT_SOURCE_MACRO_ATTRIBUTES = 7,
    TECMO_GAMEPLAY_COURT_SOURCE_MACRO_BUILDER = 8,
    TECMO_GAMEPLAY_COURT_SOURCE_LAYOUT_LOOP = 9,
    TECMO_GAMEPLAY_COURT_SOURCE_LIVE_PALETTE = 10
} TecmoGameplayCourtSourceKind;

typedef struct TecmoGameplayCourtExpectedSource {
    TecmoGameplayCourtSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t payload_offset;
    uint32_t fingerprint;
} TecmoGameplayCourtExpectedSource;

typedef struct TecmoGameplayCourtProvenance {
    uint64_t chr_offset;
    uint64_t source_offsets[TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT];
} TecmoGameplayCourtProvenance;

extern const TecmoGameplayCourtExpectedSource
    tecmo_gameplay_court_expected_sources[
        TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT];

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
    size_t message_size);

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
    size_t message_size);

int tecmo_asset_pack_gameplay_court_self_test(char *message,
                                               size_t message_size);

#endif
