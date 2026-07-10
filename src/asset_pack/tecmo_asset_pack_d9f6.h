#ifndef TECMO_ASSET_PACK_D9F6_H
#define TECMO_ASSET_PACK_D9F6_H

#include <stddef.h>
#include <stdint.h>

int tecmo_asset_pack_decode_d9f6_stream(const uint8_t *bank_bytes,
                                         size_t bank_size,
                                         size_t stream_offset,
                                         uint8_t *decoded,
                                         size_t decoded_size,
                                         size_t *encoded_size_out,
                                         char *message,
                                         size_t message_size);

#endif
