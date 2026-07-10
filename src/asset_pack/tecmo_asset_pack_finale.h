#ifndef TECMO_ASSET_PACK_FINALE_H
#define TECMO_ASSET_PACK_FINALE_H

#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

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
