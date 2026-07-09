#ifndef TECMO_ASSET_PACK_H
#define TECMO_ASSET_PACK_H

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_ID_SIZE 64U

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size);

int tecmo_asset_pack_read_entry(const char *pack_path,
                                const char *entry_id,
                                uint8_t **bytes_out,
                                uint64_t *byte_count);

void tecmo_asset_pack_free(void *buffer);

#endif
