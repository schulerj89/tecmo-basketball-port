#ifndef TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_H
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_H

#include "tecmo_gameplay_court_orientation.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ID \
    "gameplay/court-orientation"
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_OFFSET 384U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_SIZE 59U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_OFFSET 448U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_SIZE 18U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_OFFSET 480U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_SIZE 92U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_OFFSET 576U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_SIZE 4U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE 640U

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_GATE_FNV1A32 \
    0x7C94E5EAU
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ROLE_FNV1A32 \
    0xCE6C9466U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_DELTA_FNV1A32 \
    0xFE092D62U
#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_TARGETS_FNV1A32 \
    0xA27B0F6FU

#define TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32 0x44B0C44EU

typedef struct TecmoGameplayCourtOrientationExpectedSource {
    TecmoGameplayCourtOrientationSourceKind kind;
    uint8_t bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayCourtOrientationExpectedSource;

typedef struct TecmoGameplayCourtOrientationProvenance {
    uint64_t source_offsets[
        TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT];
} TecmoGameplayCourtOrientationProvenance;

extern const TecmoGameplayCourtOrientationExpectedSource
    tecmo_gameplay_court_orientation_expected_sources[
        TECMO_GAMEPLAY_COURT_ORIENTATION_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_court_orientation(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCourtOrientationProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_court_orientation_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_court_orientation_self_test(
    char *message,
    size_t message_size);

#endif
