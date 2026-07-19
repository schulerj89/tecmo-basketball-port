#ifndef TECMO_GAMEPLAY_ASSETS_H
#define TECMO_GAMEPLAY_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_ASSET_SCREEN_COUNT 2U
#define TECMO_GAMEPLAY_ASSET_SOURCE_COUNT 21U
#define TECMO_GAMEPLAY_ASSET_RULE_COUNT 10U
#define TECMO_GAMEPLAY_ASSET_POINTER_COUNT 1179U
#define TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT 2U
#define TECMO_GAMEPLAY_ASSET_DYNAMIC_SELECTOR_COUNT 8U
#define TECMO_GAMEPLAY_RESOLVED_PIECE_MAX 15U

typedef enum TecmoGameplaySourceKind {
    TECMO_GAMEPLAY_SOURCE_ACTOR_RECORDS = 1,
    TECMO_GAMEPLAY_SOURCE_ACTOR_LAYOUT = 2,
    TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_SETUP = 3,
    TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_GROUPS = 4,
    TECMO_GAMEPLAY_SOURCE_SPRITE_SELECTORS = 5,
    TECMO_GAMEPLAY_SOURCE_RULE_SETUP = 10,
    TECMO_GAMEPLAY_SOURCE_RULE_LOOKUP = 11,
    TECMO_GAMEPLAY_SOURCE_RULE_SUBTYPE = 12,
    TECMO_GAMEPLAY_SOURCE_RULE_ANIMATION = 13,
    TECMO_GAMEPLAY_SOURCE_RULE_STATE = 14,
    TECMO_GAMEPLAY_SOURCE_RULE_SHOT_RESULT = 15,
    TECMO_GAMEPLAY_SOURCE_RULE_SHOT_LAUNCH = 16,
    TECMO_GAMEPLAY_SOURCE_RULE_CLOSE_SHOT = 17,
    TECMO_GAMEPLAY_SOURCE_RULE_TRAJECTORY = 18,
    TECMO_GAMEPLAY_SOURCE_RULE_FINISH = 19,
    TECMO_GAMEPLAY_SOURCE_PERIOD_DISPATCH = 20,
    TECMO_GAMEPLAY_SOURCE_PERIOD_POINTERS = 21,
    TECMO_GAMEPLAY_SOURCE_PERIOD_STRINGS = 22,
    TECMO_GAMEPLAY_SOURCE_SCOREBOARD_VIOLATIONS = 30,
    TECMO_GAMEPLAY_SOURCE_FOUL_OVERLAY = 31,
    TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER = 32
} TecmoGameplaySourceKind;

typedef struct TecmoGameplayResolvedPiece {
    int16_t dx;
    int16_t dy;
    uint8_t tile_id;
    uint8_t palette_index;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    const uint8_t *palette;
} TecmoGameplayResolvedPiece;

typedef struct TecmoGameplayResolvedPose {
    uint16_t pointer_index;
    uint16_t pointer_cpu;
    uint16_t record_cpu;
    uint8_t width;
    uint8_t height;
    uint8_t record_tag;
    uint8_t dynamic_selector;
    uint8_t palette_group;
    uint8_t palette_index;
    uint8_t piece_count;
    TecmoGameplayResolvedPiece pieces[TECMO_GAMEPLAY_RESOLVED_PIECE_MAX];
} TecmoGameplayResolvedPose;

typedef struct TecmoGameplayScreenAsset {
    uint8_t screen_id;
    uint8_t source_bank;
    uint8_t descriptor[7];
    uint16_t descriptor_cpu;
    uint16_t stream_cpu;
    uint16_t palette_cpu;
    uint32_t encoded_size;
    uint32_t decoded_size;
    uint32_t background_chr_source_offset;
    uint32_t background_chr_size;
    uint32_t descriptor_fingerprint;
    uint32_t encoded_fingerprint;
    uint32_t decoded_fingerprint;
    uint32_t palette_fingerprint;
    const uint8_t *encoded_stream;
    const uint8_t *nametables;
    const uint8_t *palette;
} TecmoGameplayScreenAsset;

typedef struct TecmoGameplaySourceSpan {
    TecmoGameplaySourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplaySourceSpan;

typedef struct TecmoGameplayAssets {
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayScreenAsset screens[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    TecmoGameplaySourceSpan sources[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT];
    const uint8_t *actor_records;
    uint32_t actor_records_size;
    const uint8_t *actor_pointer_layout;
    uint32_t actor_pointer_layout_size;
    const uint8_t *actor_palette_groups;
    uint32_t actor_palette_groups_size;
    const uint8_t *dynamic_sprite_selectors;
    uint32_t dynamic_sprite_selector_count;
    const uint8_t *background_chr;
    uint32_t background_chr_size;
    uint32_t background_chr_source_offset;
    const uint8_t *sprite_chr;
    uint32_t sprite_chr_size;
    uint32_t sprite_chr_source_offset;
    uint32_t chr_fingerprint32;
    uint64_t chr_fingerprint64;
} TecmoGameplayAssets;

/* Parses a TGPL-1 payload and its same-pack chr/all dependency. The parser
   copies the payload so all exposed pointers remain valid until destroy. */
bool tecmo_gameplay_assets_parse(TecmoGameplayAssets *assets,
                                 const uint8_t *payload,
                                 size_t payload_size,
                                 const uint8_t *chr_bytes,
                                 size_t chr_size);

/* Loads gameplay/core and chr/all from one explicit asset pack. */
bool tecmo_gameplay_assets_load(TecmoGameplayAssets *assets,
                                const char *asset_pack_path);

void tecmo_gameplay_assets_destroy(TecmoGameplayAssets *assets);

const TecmoGameplaySourceSpan *tecmo_gameplay_assets_find_source(
    const TecmoGameplayAssets *assets,
    TecmoGameplaySourceKind kind);

/* Resolves one ROM pointer-table entry without assigning an unproven semantic
   clip name. Blank $80 cells are omitted. */
bool tecmo_gameplay_assets_resolve_pose(
    const TecmoGameplayAssets *assets,
    uint16_t pointer_index,
    uint8_t dynamic_selector_index,
    uint8_t palette_group,
    uint8_t palette_index,
    TecmoGameplayResolvedPose *pose);

#endif
