#ifndef TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_H
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_H

#include "tecmo_gameplay_penalties.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID "gameplay/penalties"
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE 28U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET 480U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE 64U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET 544U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE 16U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET 592U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE 16U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET 608U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE 16U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET 720U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE 24U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE 768U

#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_FNV1A32 0x5380334EU
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_FNV1A32 0xF30E81E0U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_FNV1A32 0xFE5E7385U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_FNV1A32 0x08B339CBU
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_FNV1A32 0x27A0B1C0U
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_FNV1A32 0xA8C0A98AU
#define TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32 0x980DDC76U

typedef struct TecmoGameplayPenaltyExpectedSource {
    TecmoGameplayPenaltySourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
} TecmoGameplayPenaltyExpectedSource;

typedef struct TecmoGameplayPenaltyProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT];
} TecmoGameplayPenaltyProvenance;

extern const TecmoGameplayPenaltyExpectedSource
    tecmo_gameplay_penalty_expected_sources[
        TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT];
extern const uint8_t tecmo_gameplay_penalty_expected_rules[
    TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE];
extern const uint8_t tecmo_gameplay_penalty_expected_classes[
    TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE];
extern const uint8_t tecmo_gameplay_penalty_expected_selectors[
    TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE];
extern const uint8_t tecmo_gameplay_penalty_expected_violations[
    TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE];
extern const uint8_t tecmo_gameplay_penalty_expected_presentations[
    2U * TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE];

int tecmo_asset_pack_build_gameplay_penalties(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayPenaltyProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_penalties_self_test(char *message,
                                                  size_t message_size);

#endif
