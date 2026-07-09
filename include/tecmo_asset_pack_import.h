#ifndef TECMO_ASSET_PACK_IMPORT_H
#define TECMO_ASSET_PACK_IMPORT_H

#include "tecmo_asset_pack.h"

#include <stddef.h>

int tecmo_asset_pack_import_intro_captures(const char *primary_project_root,
                                           const char *fallback_project_root,
                                           TecmoAssetPackBuilder *builder,
                                           unsigned *imported_count_out,
                                           char *message,
                                           size_t message_size);

#endif
