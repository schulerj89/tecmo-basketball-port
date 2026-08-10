#ifndef TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_H
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_H

#include "tecmo_gameplay_rebound_audit.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_ID \
    "gameplay/rebound-audit"
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_HEADER_SIZE 128U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE 40U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET 128U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET 328U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_SIZE 488U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE 816U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A32 0xD6363FBDU
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A64 \
    0xB6B95695306094BDULL

#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_DIRECT_CAROM_MASK 0x03U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_CAROM_READY_MASK 0x80U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_INDEX 8U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_OFFSET 0xC0U
#define TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_BASE 0x7B58U

typedef struct TecmoGameplayReboundAuditExpectedSource {
    TecmoGameplayReboundAuditSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
} TecmoGameplayReboundAuditExpectedSource;

typedef struct TecmoGameplayReboundAuditProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT];
} TecmoGameplayReboundAuditProvenance;

extern const TecmoGameplayReboundAuditExpectedSource
    tecmo_gameplay_rebound_audit_expected_sources[
        TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_rebound_audit(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayReboundAuditProvenance *provenance,
    char *message,
    size_t message_size);

/* Reads the caller's local Rev1 ROM, validates every span, then independently
 * flips one copied byte in each authoritative span and requires the matching
 * span-specific rejection. */
int tecmo_asset_pack_gameplay_rebound_audit_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_rebound_audit_self_test(
    char *message,
    size_t message_size);

#endif
