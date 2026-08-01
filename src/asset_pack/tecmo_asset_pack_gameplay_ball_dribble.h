#ifndef TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_H
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_H

#include "tecmo_gameplay_ball_dribble.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ID "gameplay/ball-dribble"
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_HEADER_SIZE 192U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCES_OFFSET 192U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_SIZE 146U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_OFFSET 416U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_SIZE 184U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE 608U

#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ROUTINE_FNV1A32 0xDB540670U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_TABLES_FNV1A32 0xE9784D28U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SOURCE_FNV1A32 0x9579D729U
#define TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_FNV1A32 0xE2CE6BFFU

typedef struct TecmoGameplayBallDribbleExpectedSource {
    TecmoGameplayBallDribbleSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayBallDribbleExpectedSource;

typedef struct TecmoGameplayBallDribbleProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT];
} TecmoGameplayBallDribbleProvenance;

extern const TecmoGameplayBallDribbleExpectedSource
    tecmo_gameplay_ball_dribble_expected_sources[
        TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_ball_dribble(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayBallDribbleProvenance *provenance,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_ball_dribble_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_ball_dribble_self_test(
    char *message,
    size_t message_size);

#endif
