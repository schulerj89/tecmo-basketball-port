#ifndef TECMO_ASSET_PACK_POST_ARENA_H
#define TECMO_ASSET_PACK_POST_ARENA_H

#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

void tecmo_asset_pack_post_arena_seed_contract_fixture(
    uint8_t *rom,
    uint64_t bank04_offset,
    uint64_t fixed_offset);

int tecmo_asset_pack_build_ready_screen(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t payload[TECMO_ASSET_PACK_READY_SIZE],
    TecmoPostArenaProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_warriors_transition(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t payload[TECMO_ASSET_PACK_WARRIORS_SIZE],
    TecmoPostArenaProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_clippers_transition(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t payload[TECMO_ASSET_PACK_CLIPPERS_SIZE],
    TecmoPostArenaProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_bucks_transition(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t payload[TECMO_ASSET_PACK_BUCKS_SIZE],
    TecmoPostArenaProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_build_pass_transition(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    uint8_t payload[TECMO_ASSET_PACK_PASS_SIZE],
    TecmoPostArenaProvenance *provenance,
    char *message,
    size_t message_size);

#endif
