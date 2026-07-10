#ifndef TECMO_ASSET_PACK_UTIL_H
#define TECMO_ASSET_PACK_UTIL_H

#include "tecmo_asset_pack.h"

#include <stddef.h>
#include <stdint.h>

void tecmo_asset_pack_set_message(char *message,
                                  size_t message_size,
                                  const char *text);
void tecmo_asset_pack_set_messagef(char *message,
                                   size_t message_size,
                                   const char *format,
                                   ...);
int tecmo_asset_pack_copy_path(char *dest,
                               size_t dest_size,
                               const char *src);
int tecmo_asset_pack_append_text(char *buffer,
                                 size_t capacity,
                                 size_t *length,
                                 const char *format,
                                 ...);

uint16_t tecmo_asset_pack_read_u16(const uint8_t *bytes);
uint32_t tecmo_asset_pack_read_u32(const uint8_t *bytes);
uint32_t tecmo_asset_pack_fnv1a32(const uint8_t *bytes, size_t byte_count);
void tecmo_asset_pack_store_u16(uint8_t *bytes, uint16_t value);
void tecmo_asset_pack_store_u32(uint8_t *bytes, uint32_t value);

TecmoAssetPackEntryInfo tecmo_asset_pack_make_entry_info(
    const char *id,
    uint32_t type,
    uint32_t bank,
    uint32_t cpu_address,
    uint64_t source_offset,
    uint32_t flags);

int tecmo_asset_pack_read_file(const char *path,
                               uint8_t **bytes_out,
                               uint64_t *size_out);

#endif
