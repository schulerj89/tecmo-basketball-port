#ifndef TECMO_GAMEPLAY_HUD_H
#define TECMO_GAMEPLAY_HUD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_HUD_SOURCE_COUNT 3U
#define TECMO_GAMEPLAY_HUD_TEAM_COUNT 29U
#define TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH 5U
#define TECMO_GAMEPLAY_HUD_FONT_FIRST 0x20U
#define TECMO_GAMEPLAY_HUD_FONT_COUNT 59U
#define TECMO_GAMEPLAY_HUD_FONT_CHR_SELECTOR 0xFAU

typedef enum TecmoGameplayHudSourceKind {
    TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LAYOUT = 1,
    TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LABELS = 2,
    TECMO_GAMEPLAY_HUD_SOURCE_TEXT_FORMAT = 3
} TecmoGameplayHudSourceKind;

typedef struct TecmoGameplayHudSourceSpan {
    TecmoGameplayHudSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayHudSourceSpan;

typedef struct TecmoGameplayHudAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayHudSourceSpan sources[TECMO_GAMEPLAY_HUD_SOURCE_COUNT];
    const uint8_t (*team_label_tiles)[TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH];
    const uint8_t *font_tiles;
    uint8_t team_ppu_low[2];
    uint8_t dot_tile;
    uint8_t blank_tile;
    uint32_t gameplay_core_fingerprint;
    uint32_t team_data_fingerprint;
    uint32_t chr_fingerprint32;
    uint64_t chr_fingerprint64;
} TecmoGameplayHudAssets;

void tecmo_gameplay_hud_assets_init(TecmoGameplayHudAssets *assets);
void tecmo_gameplay_hud_assets_destroy(TecmoGameplayHudAssets *assets);

/* Strict THUD-1 parsing validates the exact same-pack TGPL-1, TTDT-1, and
   chr/all dependencies. Successful parses own a private payload copy. */
bool tecmo_gameplay_hud_assets_parse(
    TecmoGameplayHudAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *team_data,
    size_t team_data_size,
    const uint8_t *chr_bytes,
    size_t chr_size);
bool tecmo_gameplay_hud_assets_load(
    TecmoGameplayHudAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayHudSourceSpan *tecmo_gameplay_hud_find_source(
    const TecmoGameplayHudAssets *assets,
    TecmoGameplayHudSourceKind kind);

/* Maps the original supported ASCII range through Bank02's live gameplay
   character table. Invalid or unsupported input leaves tile_out unchanged. */
bool tecmo_gameplay_hud_tile_for_ascii(
    const TecmoGameplayHudAssets *assets,
    unsigned char character,
    uint8_t *tile_out);

bool tecmo_gameplay_hud_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
