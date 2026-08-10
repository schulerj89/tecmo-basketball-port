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
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET 384U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE 969U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_OFFSET 1353U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_SIZE 7U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE 1360U

#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32 \
    0xCE60861FU
/* Filled by the exact importer payload contract; both importer and parser
   reject a change independently of individual source-span fingerprints. */
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_FNV1A32 \
    0xB38C93F5U

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
