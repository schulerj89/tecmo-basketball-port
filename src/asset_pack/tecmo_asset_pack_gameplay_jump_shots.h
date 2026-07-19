#ifndef TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_H
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_H

#include "tecmo_gameplay_jump_shots.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_ID "gameplay/jump-shots"
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_BANK 5U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_OFFSET 512U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE 1034U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_OFFSET 1546U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_PADDING_SIZE 6U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_OFFSET 1552U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_OFFSET 1584U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE 1648U

#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32 0xE0A3BBB6U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32 0xC868CFE3U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32 0xA057A625U
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32 0x069DDFDBU
#define TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_FNV1A32 0x7587B099U

typedef struct TecmoGameplayJumpShotExpectedSource {
    TecmoGameplayJumpShotSourceKind kind;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t payload_offset;
    uint32_t fingerprint;
} TecmoGameplayJumpShotExpectedSource;

typedef struct TecmoGameplayJumpShotProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT];
} TecmoGameplayJumpShotProvenance;

extern const TecmoGameplayJumpShotExpectedSource
    tecmo_gameplay_jump_shot_expected_sources[
        TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT];
extern const uint8_t tecmo_gameplay_jump_shot_expected_constants[
    TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_SIZE];
extern const uint16_t tecmo_gameplay_jump_shot_expected_pose_indices[
    TECMO_GAMEPLAY_JUMP_SHOT_POSE_COUNT];

int tecmo_asset_pack_build_gameplay_jump_shots(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayJumpShotProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_jump_shots_self_test(char *message,
                                                    size_t message_size);

#endif
