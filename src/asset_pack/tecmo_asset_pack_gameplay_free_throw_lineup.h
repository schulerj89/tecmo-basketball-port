#ifndef TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_H
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_H

#include "tecmo_gameplay_free_throw_lineup.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ID \
    "gameplay/free-throw-lineup"
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_OFFSET 384U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_SIZE 42U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_OFFSET 432U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_SIZE 334U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_OFFSET 768U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_SIZE 238U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_OFFSET 1008U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_SIZE 188U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE 1216U

#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_POSE_FNV1A32 \
    0xAD834719U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ROUND_FNV1A32 \
    0x998D84B8U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FOLLOWUP_FNV1A32 \
    0xFB7680EFU
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_TABLES_FNV1A32 \
    0xAFB31306U
#define TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FNV1A32 0xB17B9A3FU

typedef struct TecmoGameplayFreeThrowLineupExpectedSource {
    TecmoGameplayFreeThrowLineupSourceKind kind;
    uint8_t bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayFreeThrowLineupExpectedSource;

typedef struct TecmoGameplayFreeThrowLineupProvenance {
    uint64_t source_offsets[
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT];
} TecmoGameplayFreeThrowLineupProvenance;

extern const TecmoGameplayFreeThrowLineupExpectedSource
    tecmo_gameplay_free_throw_lineup_expected_sources[
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_free_throw_lineup(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayFreeThrowLineupProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_free_throw_lineup_self_test(
    char *message,
    size_t message_size);

#endif
