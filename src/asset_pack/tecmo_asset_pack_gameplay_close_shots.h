#ifndef TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_H
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_H

#include "tecmo_gameplay_close_shots.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_ID "gameplay/close-shots"
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_BANK 5U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_OFFSET 672U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE 2357U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_OFFSET 3029U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PADDING_SIZE 3U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_OFFSET 3032U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_OFFSET 3064U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_OFFSET 3080U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE 3144U

#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32 0x9CDEB66FU
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT0_PHASE_FNV1A32 0x3A445D6AU
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_VARIANT2_PHASE_FNV1A32 0x4BE1BE6EU
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PHASES_FNV1A32 0x0445C745U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_BASE_FNV1A32 0x50049095U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_FNV1A32 0x9BFCCE7CU
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_COUNT 208U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_FNV1A32 0xBFDB4095U
#define TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32 0xDACDC976U

typedef struct TecmoGameplayCloseShotExpectedSource {
    TecmoGameplayCloseShotSourceKind kind;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t payload_offset;
    uint32_t fingerprint;
} TecmoGameplayCloseShotExpectedSource;

typedef struct TecmoGameplayCloseShotProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT];
} TecmoGameplayCloseShotProvenance;

extern const TecmoGameplayCloseShotExpectedSource
    tecmo_gameplay_close_shot_expected_sources[
        TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT];
extern const uint8_t tecmo_gameplay_close_shot_expected_variant0_phases[
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT];
extern const uint8_t tecmo_gameplay_close_shot_expected_variant2_phases[
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT];
extern const uint16_t tecmo_gameplay_close_shot_expected_pose_bases[
    TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT];

int tecmo_asset_pack_build_gameplay_close_shots(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCloseShotProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_close_shots_self_test(char *message,
                                                     size_t message_size);

#endif
