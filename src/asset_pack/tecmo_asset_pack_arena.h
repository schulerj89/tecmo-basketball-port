#ifndef TECMO_ASSET_PACK_ARENA_H
#define TECMO_ASSET_PACK_ARENA_H

#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

int tecmo_asset_pack_build_arena_background_layer(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t *payload,
    size_t payload_size,
    TecmoArenaBackgroundProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_arena_sprite_groups(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_offset,
    uint64_t chr_size,
    uint8_t *payload,
    size_t payload_size,
    TecmoArenaSpriteGroupsProvenance *provenance,
    char *message,
    size_t message_size);

#endif
