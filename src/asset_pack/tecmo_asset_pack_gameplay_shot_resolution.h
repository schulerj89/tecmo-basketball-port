#ifndef TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_H
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_H

#include "tecmo_gameplay_shot_resolution.h"
#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_BANK 5U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_HEADER_SIZE 128U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SOURCES_OFFSET 128U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE 64U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_OFFSET 320U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE 8U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_PADDING_OFFSET 352U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_PADDING_SIZE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE 384U

/* Filled from the exact Rev1 ROM and the canonical native payload. */
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A32 \
    0xFD84F04BU
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_FNV1A64 \
    0x66FEEF03F870AE6BULL
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A32 \
    0xD5CB247FU
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTES_FNV1A64 \
    0x9BA367FC638FB99FULL
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32 0x8486DB33U
#define TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A64 \
    0x3DDF28659B192273ULL

typedef struct TecmoGameplayShotResolutionExpectedSource {
    TecmoGameplayShotResolutionSourceKind kind;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
} TecmoGameplayShotResolutionExpectedSource;

typedef struct TecmoGameplayShotResolutionProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT];
} TecmoGameplayShotResolutionProvenance;

extern const TecmoGameplayShotResolutionExpectedSource
    tecmo_gameplay_shot_resolution_expected_sources[
        TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT];
extern const uint8_t tecmo_gameplay_shot_resolution_expected_metadata[
    TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_METADATA_SIZE];
extern const uint8_t tecmo_gameplay_shot_resolution_expected_routes[
    TECMO_GAMEPLAY_SHOT_RESOLUTION_RIM_ROUTE_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ROUTE_STRIDE];

int tecmo_asset_pack_build_gameplay_shot_resolution(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayShotResolutionProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_shot_resolution_self_test(
    char *message,
    size_t message_size);

#endif
