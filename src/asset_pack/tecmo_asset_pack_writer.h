#ifndef TECMO_ASSET_PACK_WRITER_H
#define TECMO_ASSET_PACK_WRITER_H

#include "tecmo_asset_pack.h"

/* Internal import-orchestrator query; the builder must still be open. */
size_t tecmo_asset_pack_builder_entry_count(const TecmoAssetPackBuilder *builder);

#endif
