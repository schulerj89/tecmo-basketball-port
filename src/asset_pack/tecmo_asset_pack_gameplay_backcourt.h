#ifndef TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_H
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_H

#include "tecmo_gameplay_backcourt.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_ID "gameplay/backcourt"
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_HEADER_SIZE 192U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SOURCES_OFFSET 192U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_OFFSET 224U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_SIZE 124U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_OFFSET 352U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE 512U

#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_DETECTOR_FNV1A32 0xC137674FU
#define TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_FNV1A32 0x2C7BAF1DU

typedef struct TecmoGameplayBackcourtExpectedSource {
    TecmoGameplayBackcourtSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayBackcourtExpectedSource;

typedef struct TecmoGameplayBackcourtProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT];
} TecmoGameplayBackcourtProvenance;

extern const TecmoGameplayBackcourtExpectedSource
    tecmo_gameplay_backcourt_expected_sources[
        TECMO_GAMEPLAY_BACKCOURT_SOURCE_COUNT];
extern const uint8_t tecmo_gameplay_backcourt_expected_rules[
    TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_RULES_SIZE];

int tecmo_asset_pack_build_gameplay_backcourt(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayBackcourtProvenance *provenance,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_backcourt_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_backcourt_self_test(
    char *message,
    size_t message_size);

#endif
