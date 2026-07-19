#ifndef TECMO_ASSET_PACK_SEASON_H
#define TECMO_ASSET_PACK_SEASON_H

#include "tecmo_asset_pack_import_layout.h"

#include <stddef.h>
#include <stdint.h>

int tecmo_asset_pack_build_season_menu(const uint8_t *rom,
                                       uint64_t rom_size,
                                       uint64_t prg_offset,
                                       uint32_t prg_banks,
                                       uint64_t chr_size,
                                       int enforce_revision_fingerprints,
                                       uint8_t *payload,
                                       size_t payload_size,
                                       TecmoSeasonMenuProvenance *provenance,
                                       char *message,
                                       size_t message_size);

int tecmo_asset_pack_season_self_test(char *message, size_t message_size);

#endif
