#ifndef TECMO_ASSET_PACK_FINALE_H
#define TECMO_ASSET_PACK_FINALE_H

#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_FINALE_CAPTION_COUNT 3U
#define TECMO_ASSET_PACK_FINALE_CAPTION_MAX_GLYPHS 5U
#define TECMO_ASSET_PACK_FINALE_CAPTION_GLYPH_SENTINEL 0xFFU
#define TECMO_ASSET_PACK_FINALE_TEAM_NAME_TABLE_CPU 0xAC4AU
#define TECMO_ASSET_PACK_FINALE_TEAM_NAME_TABLE_SIZE 272U
#define TECMO_ASSET_PACK_FINALE_TEAM_NAME_TABLE_FINGERPRINT 0xAA8FC37DU
#define TECMO_ASSET_PACK_FINALE_TEAM_COLOR_TABLE_CPU 0xDC19U
#define TECMO_ASSET_PACK_FINALE_TEAM_COLOR_TABLE_SIZE 29U
#define TECMO_ASSET_PACK_FINALE_TEAM_COLOR_TABLE_FINGERPRINT 0x1451114FU
#define TECMO_ASSET_PACK_FINALE_BULLS_TEAM_ID 0x03U
#define TECMO_ASSET_PACK_FINALE_BULLS_TEAM_COLOR 0x15U

int tecmo_asset_pack_finale_self_test(char *message, size_t message_size);

int tecmo_asset_pack_build_finale_sequence(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t payload[TECMO_ASSET_PACK_FINALE_SIZE],
    TecmoFinaleProvenance *provenance,
    char *message,
    size_t message_size);

#endif
