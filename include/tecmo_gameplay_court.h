#ifndef TECMO_GAMEPLAY_COURT_H
#define TECMO_GAMEPLAY_COURT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_COURT_WIDTH 32U
#define TECMO_GAMEPLAY_COURT_HEIGHT 30U
#define TECMO_GAMEPLAY_COURT_NAMETABLE_SIZE 1024U
#define TECMO_GAMEPLAY_COURT_PALETTE_SIZE 16U

typedef struct TecmoGameplayCourt {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    const uint8_t *nametable;
    const uint8_t *palette;
    uint16_t minimum_macro_index;
    uint16_t maximum_macro_index;
    uint16_t unique_macro_count;
    uint32_t nametable_fingerprint;
    uint32_t palette_fingerprint;
    uint32_t chr_fingerprint32;
    uint64_t chr_fingerprint64;
} TecmoGameplayCourt;

/* Objects must be initialized once before parse/load/destroy. Parse and load
   release any prior successful storage, including when a replacement fails. */
void tecmo_gameplay_court_init(TecmoGameplayCourt *court);

/* Parses one TGCT-1 payload and validates the same-pack chr/all revision
   dependency. The payload is copied; no caller-owned buffer is retained. */
bool tecmo_gameplay_court_parse(TecmoGameplayCourt *court,
                                const uint8_t *payload,
                                size_t payload_size,
                                const uint8_t *chr_bytes,
                                size_t chr_size);

/* Loads gameplay/court and chr/all from one explicit asset pack. */
bool tecmo_gameplay_court_load(TecmoGameplayCourt *court,
                               const char *asset_pack_path);

void tecmo_gameplay_court_destroy(TecmoGameplayCourt *court);

/* The returned views remain valid until the next parse/load/destroy. */
const uint8_t *tecmo_gameplay_court_nametable(
    const TecmoGameplayCourt *court,
    size_t *byte_count_out);
const uint8_t *tecmo_gameplay_court_palette(
    const TecmoGameplayCourt *court,
    size_t *byte_count_out);

#endif
