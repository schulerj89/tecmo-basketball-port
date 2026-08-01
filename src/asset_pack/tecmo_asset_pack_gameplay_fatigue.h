#ifndef TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_H
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_H

#include "tecmo_gameplay_fatigue.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_ID "gameplay/fatigue"
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_HEADER_SIZE 192U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET 192U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_OFFSET 80U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_STRIDE 12U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_SIZE 226U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_OFFSET 496U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_SIZE 16U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE 512U

#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_FNV1A32 0xF61DFFF7U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_FNV1A32 0x09342B88U
#define TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_FNV1A32 0xF80F170DU

typedef struct TecmoGameplayFatigueExpectedSource {
    TecmoGameplayFatigueSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayFatigueExpectedSource;

typedef struct TecmoGameplayFatigueProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT];
} TecmoGameplayFatigueProvenance;

extern const TecmoGameplayFatigueExpectedSource
    tecmo_gameplay_fatigue_expected_sources[
        TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_fatigue(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayFatigueProvenance *provenance,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_fatigue_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_fatigue_self_test(
    char *message,
    size_t message_size);

#endif
