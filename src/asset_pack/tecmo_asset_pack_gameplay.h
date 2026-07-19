#ifndef TECMO_ASSET_PACK_GAMEPLAY_H
#define TECMO_ASSET_PACK_GAMEPLAY_H

#include "tecmo_gameplay_assets.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_ID "gameplay/core"
#define TECMO_ASSET_PACK_GAMEPLAY_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_SCREEN_STRIDE 64U
#define TECMO_ASSET_PACK_GAMEPLAY_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_SCREENS_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_SOURCES_OFFSET 384U
#define TECMO_ASSET_PACK_GAMEPLAY_ENCODED_OFFSET 1056U
#define TECMO_ASSET_PACK_GAMEPLAY_ENCODED_SIZE 956U
#define TECMO_ASSET_PACK_GAMEPLAY_DECODED_OFFSET 2012U
#define TECMO_ASSET_PACK_GAMEPLAY_DECODED_SIZE 4096U
#define TECMO_ASSET_PACK_GAMEPLAY_PALETTES_OFFSET 6108U
#define TECMO_ASSET_PACK_GAMEPLAY_PALETTES_SIZE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_OFFSET 6140U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_RECORDS_SIZE 9657U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_LAYOUT_OFFSET 15797U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_LAYOUT_SIZE 2631U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_OFFSET 18428U
#define TECMO_ASSET_PACK_GAMEPLAY_ACTOR_PALETTE_SIZE 92U
#define TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_OFFSET 18520U
#define TECMO_ASSET_PACK_GAMEPLAY_PALETTE_GROUPS_SIZE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_RULES_OFFSET 18552U
#define TECMO_ASSET_PACK_GAMEPLAY_RULES_SIZE 2824U
#define TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_OFFSET 21376U
#define TECMO_ASSET_PACK_GAMEPLAY_SELECTOR_SIZE 8U
#define TECMO_ASSET_PACK_GAMEPLAY_PERIOD_OFFSET 21384U
#define TECMO_ASSET_PACK_GAMEPLAY_PERIOD_SIZE 154U
#define TECMO_ASSET_PACK_GAMEPLAY_EVENTS_OFFSET 21538U
#define TECMO_ASSET_PACK_GAMEPLAY_EVENTS_SIZE 944U
#define TECMO_ASSET_PACK_GAMEPLAY_BG_CHR_OFFSET 22482U
#define TECMO_ASSET_PACK_GAMEPLAY_BG_CHR_SIZE 4096U
#define TECMO_ASSET_PACK_GAMEPLAY_SPRITE_CHR_OFFSET 26578U
#define TECMO_ASSET_PACK_GAMEPLAY_SPRITE_CHR_SIZE 9216U
#define TECMO_ASSET_PACK_GAMEPLAY_SIZE 35794U

#define TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE 262144U
#define TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 0xF6F6E854U
#define TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64 0x96A64F53B240ABB4ULL
#define TECMO_ASSET_PACK_GAMEPLAY_BG_CHR_SOURCE 0xB000U
#define TECMO_ASSET_PACK_GAMEPLAY_BG_CHR_FNV1A32 0xB3DB8411U
#define TECMO_ASSET_PACK_GAMEPLAY_SPRITE_CHR_SOURCE 0x10000U
#define TECMO_ASSET_PACK_GAMEPLAY_SPRITE_CHR_FNV1A32 0xA6E68F1FU
#define TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 0x1D7897A1U

typedef struct TecmoGameplayProvenance {
    uint64_t chr_offset;
    uint64_t descriptor_offsets[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    uint64_t stream_offsets[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    uint64_t stream_sizes[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    uint64_t palette_offsets[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    uint64_t source_offsets[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT];
} TecmoGameplayProvenance;

typedef struct TecmoGameplayExpectedScreen {
    uint8_t screen_id;
    uint8_t source_bank;
    uint16_t descriptor_cpu;
    uint16_t stream_cpu;
    uint16_t palette_cpu;
    uint32_t encoded_size;
    uint32_t descriptor_fingerprint;
    uint32_t encoded_fingerprint;
    uint32_t decoded_fingerprint;
    uint32_t palette_fingerprint;
    uint8_t descriptor[7];
} TecmoGameplayExpectedScreen;

typedef struct TecmoGameplayExpectedSource {
    TecmoGameplaySourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t payload_offset;
    uint32_t fingerprint;
} TecmoGameplayExpectedSource;

extern const TecmoGameplayExpectedScreen
    tecmo_gameplay_expected_screens[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
extern const TecmoGameplayExpectedSource
    tecmo_gameplay_expected_sources[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_self_test(char *message, size_t message_size);

#endif
