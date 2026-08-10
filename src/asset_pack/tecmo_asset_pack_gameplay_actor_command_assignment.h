#ifndef TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_H
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_H

#include "tecmo_gameplay_actor_command_assignment.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_ID \
    "gameplay/actor-command-assignment"
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_HEADER_SIZE 128U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET 128U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET 416U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE 1064U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_OFFSET 1480U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_SIZE 8U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE 1488U

#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32 \
    0x741A149EU
/* Filled by the exact importer payload contract; both importer and parser
   reject a change independently of individual source-span fingerprints. */
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_FNV1A32 \
    0x4C7C2B34U

#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_BAD_INPUT 0x01U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_DESCRIPTOR 0x02U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_FNV1A32 0x04U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_FNV1A64 0x08U
#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SPAN_VERIFY_SEMANTICS 0x10U

typedef struct TecmoGameplayActorCommandAssignmentExpectedSource {
    TecmoGameplayActorCommandAssignmentSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t payload_offset;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
} TecmoGameplayActorCommandAssignmentExpectedSource;

typedef struct TecmoGameplayActorCommandAssignmentProvenance {
    uint64_t source_offsets[
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT];
} TecmoGameplayActorCommandAssignmentProvenance;

extern const TecmoGameplayActorCommandAssignmentExpectedSource
    tecmo_gameplay_actor_command_assignment_expected_sources[
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT];
extern const uint8_t
    tecmo_gameplay_actor_command_assignment_rev1_sha256[32];

/* Bounded verifier used by both importer/parser and focused tests.  It checks
 * one canonical 32-byte descriptor and one source span without requiring a
 * whole ROM, so FNV32/FNV64 and descriptor rejection remain independently
 * reachable behind the outer Rev1 fingerprint. */
uint32_t tecmo_asset_pack_gameplay_actor_command_assignment_verify_span(
    size_t index,
    const uint8_t *record,
    size_t record_size,
    const uint8_t *span_bytes,
    size_t span_size);

int tecmo_asset_pack_build_gameplay_actor_command_assignment(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayActorCommandAssignmentProvenance *provenance,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_actor_command_assignment_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_actor_command_assignment_self_test(
    char *message,
    size_t message_size);

#endif
