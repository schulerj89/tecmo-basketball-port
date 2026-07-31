#ifndef TECMO_ASSET_PACK_GAMEPLAY_HUD_H
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_H

#include "tecmo_gameplay_hud.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_HUD_ID "gameplay/hud"
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_OFFSET 112U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_STRIDE 12U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET 352U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_SIZE 47U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET 400U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_SIZE 174U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET 576U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE 280U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE 864U

#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_FNV1A32 0x5EA411D3U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_FNV1A32 0xC222B5A1U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_FNV1A32 0xFACE1F66U
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_FNV1A32 0x3D13AA89U

#define TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_TILE_OFFSET \
    (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET + \
     TECMO_GAMEPLAY_HUD_TEAM_COUNT)
#define TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET \
    (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET + \
     (0xB006U - 0xAF64U))

typedef struct TecmoGameplayHudExpectedSource {
    TecmoGameplayHudSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayHudExpectedSource;

typedef struct TecmoGameplayHudProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_HUD_SOURCE_COUNT];
} TecmoGameplayHudProvenance;

extern const TecmoGameplayHudExpectedSource
    tecmo_gameplay_hud_expected_sources[TECMO_GAMEPLAY_HUD_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_hud(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayHudProvenance *provenance,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_hud_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);
int tecmo_asset_pack_gameplay_hud_self_test(
    char *message,
    size_t message_size);

#endif
