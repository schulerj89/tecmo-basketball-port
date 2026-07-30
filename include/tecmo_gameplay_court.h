#ifndef TECMO_GAMEPLAY_COURT_H
#define TECMO_GAMEPLAY_COURT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_COURT_WIDTH 32U
#define TECMO_GAMEPLAY_COURT_HEIGHT 30U
#define TECMO_GAMEPLAY_COURT_NAMETABLE_SIZE 1024U
#define TECMO_GAMEPLAY_COURT_PALETTE_SIZE 16U

#define TECMO_GAMEPLAY_COURT_WORLD_CONTRACT_TAG 0x54475731U
#define TECMO_GAMEPLAY_COURT_WORLD_WIDTH_TILES 96U
#define TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_TILES 30U
#define TECMO_GAMEPLAY_COURT_WORLD_WIDTH_PIXELS 768U
#define TECMO_GAMEPLAY_COURT_WORLD_HEIGHT_PIXELS 240U
#define TECMO_GAMEPLAY_COURT_WORLD_TILE_COUNT 2880U
#define TECMO_GAMEPLAY_COURT_WORLD_TILES_FNV1A32 0x6458B5E5U
#define TECMO_GAMEPLAY_COURT_WORLD_PALETTES_FNV1A32 0x7F650645U

#define TECMO_GAMEPLAY_COURT_VIEWPORT_WIDTH_PIXELS 256U
#define TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_STRIDE 33U
#define TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_COUNT 990U
#define TECMO_GAMEPLAY_COURT_MAX_CAMERA_X 512U

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

/* Caller-owned pure decode of all 48x15 ROM macro cells. Metadata and plane
   fingerprints are part of the slicer input contract, not advisory fields. */
typedef struct TecmoGameplayCourtWorld {
    uint32_t contract_tag;
    uint16_t width_tiles;
    uint16_t height_tiles;
    uint16_t width_pixels;
    uint16_t height_pixels;
    uint16_t minimum_macro_index;
    uint16_t maximum_macro_index;
    uint16_t unique_macro_count;
    uint16_t reserved;
    uint32_t tiles_fingerprint;
    uint32_t palette_indices_fingerprint;
    uint8_t tiles[TECMO_GAMEPLAY_COURT_WORLD_TILE_COUNT];
    uint8_t palette_indices[TECMO_GAMEPLAY_COURT_WORLD_TILE_COUNT];
} TecmoGameplayCourtWorld;

/* The fixed 33-column planes contain the 32 visible coarse tile columns plus
   the right-side fetch column required by nonzero fine scroll. Aligned slices
   report 32 columns and leave each row's unused cell zero. */
typedef struct TecmoGameplayCourtViewport {
    uint16_t camera_x;
    uint16_t first_tile_x;
    uint8_t fine_scroll_x;
    uint8_t column_count;
    uint16_t tile_stride;
    uint16_t height_tiles;
    uint32_t tiles_fingerprint;
    uint32_t palette_indices_fingerprint;
    uint8_t tiles[TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_COUNT];
    uint8_t palette_indices[TECMO_GAMEPLAY_COURT_VIEWPORT_TILE_COUNT];
} TecmoGameplayCourtViewport;

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

/* Strictly expands the TGCT-1 raw layout and macro sources. The output is
   assigned only after every source reference, golden, and legacy-middle
   cross-check succeeds. */
bool tecmo_gameplay_court_decode_world(
    const TecmoGameplayCourt *court,
    TecmoGameplayCourtWorld *world_out);

/* Slices camera X 0..512 inclusive into a fixed 33x30 tile/palette view.
   The world contract and both plane fingerprints are revalidated before the
   caller output is assigned. */
bool tecmo_gameplay_court_slice_viewport(
    const TecmoGameplayCourtWorld *world,
    uint16_t camera_x,
    TecmoGameplayCourtViewport *viewport_out);

#endif
