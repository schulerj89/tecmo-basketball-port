#ifndef TECMO_GAMEPLAY_ASSETS_H
#define TECMO_GAMEPLAY_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_ASSET_SCREEN_COUNT 2U
#define TECMO_GAMEPLAY_ASSET_SOURCE_COUNT 30U
#define TECMO_GAMEPLAY_ASSET_RULE_COUNT 10U
#define TECMO_GAMEPLAY_ASSET_POINTER_COUNT 1179U
#define TECMO_GAMEPLAY_ASSET_PALETTE_GROUP_COUNT 2U
#define TECMO_GAMEPLAY_ASSET_R2_SELECTOR_COUNT 8U
#define TECMO_GAMEPLAY_RESOLVED_PIECE_MAX 15U
#define TECMO_GAMEPLAY_LIVE_BAND_COUNT 6U

typedef enum TecmoGameplaySourceKind {
    TECMO_GAMEPLAY_SOURCE_ACTOR_RECORDS = 1,
    TECMO_GAMEPLAY_SOURCE_ACTOR_POINTERS = 2,
    TECMO_GAMEPLAY_SOURCE_ACTOR_POINTER_TAIL = 3,
    TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_SETUP = 4,
    TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_POINTERS = 5,
    TECMO_GAMEPLAY_SOURCE_ACTOR_PALETTE_GROUPS = 6,
    TECMO_GAMEPLAY_SOURCE_ACTOR_RENDERER = 7,
    TECMO_GAMEPLAY_SOURCE_ACTOR_RENDER_STAGING = 8,
    TECMO_GAMEPLAY_SOURCE_SPRITE_R2_SELECTORS = 9,
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
    TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER = 32,
    TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_SELECT = 40,
    TECMO_GAMEPLAY_SOURCE_LIVE_ORIENTATION_IDS = 41,
    TECMO_GAMEPLAY_SOURCE_LIVE_IRQ_ARM = 42,
    TECMO_GAMEPLAY_SOURCE_LIVE_IRQ_BANDS = 43,
    TECMO_GAMEPLAY_SOURCE_LIVE_BAND_INIT = 44
} TecmoGameplaySourceKind;

typedef struct TecmoGameplayPoseContext {
    uint8_t actor_slot_base;
    uint8_t actor_attributes;
    uint8_t palette_group;
    uint8_t mmc3_r2_r5[4];
} TecmoGameplayPoseContext;

typedef struct TecmoGameplayResolvedPiece {
    int16_t dx;
    int16_t dy;
    uint8_t cell_byte;
    uint8_t tile_id;
    uint8_t oam_attributes;
    uint8_t palette_index;
    bool flip_horizontal;
    uint32_t top_chr_offset;
    uint32_t bottom_chr_offset;
    const uint8_t *top_chr;
    const uint8_t *bottom_chr;
    const uint8_t *palette;
} TecmoGameplayResolvedPiece;

typedef struct TecmoGameplayResolvedPose {
    uint16_t pointer_index;
    uint16_t pointer_cpu;
    uint16_t record_cpu;
    uint8_t columns;
    uint8_t rows;
    uint8_t record_tag;
    uint8_t actor_slot_base;
    uint8_t actor_attributes;
    uint8_t palette_group;
    uint8_t piece_count;
    uint8_t mmc3_r2_r5[4];
    TecmoGameplayResolvedPiece pieces[TECMO_GAMEPLAY_RESOLVED_PIECE_MAX];
} TecmoGameplayResolvedPose;

typedef struct TecmoGameplayLiveBackgroundContext {
    uint8_t pre_asl_r0[TECMO_GAMEPLAY_LIVE_BAND_COUNT];
    uint8_t pre_asl_r1[TECMO_GAMEPLAY_LIVE_BAND_COUNT];
} TecmoGameplayLiveBackgroundContext;

typedef struct TecmoGameplayResolvedOrientationTile {
    uint8_t screen_id;
    uint8_t nametable_page;
    uint8_t row;
    uint8_t column;
    uint8_t band;
    uint8_t tile_id;
    uint8_t palette_index;
    uint8_t mmc3_bank;
    uint32_t chr_offset;
    const uint8_t *chr;
    const uint8_t *palette;
} TecmoGameplayResolvedOrientationTile;

typedef struct TecmoGameplayScreenAsset {
    uint8_t screen_id;
    uint8_t source_bank;
    uint8_t descriptor[7];
    uint16_t descriptor_cpu;
    uint16_t stream_cpu;
    uint16_t palette_cpu;
    uint32_t encoded_size;
    uint32_t decoded_size;
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
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    uint8_t *chr_storage;
    size_t chr_storage_size;
    TecmoGameplayScreenAsset screens[TECMO_GAMEPLAY_ASSET_SCREEN_COUNT];
    TecmoGameplaySourceSpan sources[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT];
    const uint8_t *actor_records;
    uint32_t actor_records_size;
    const uint8_t *actor_pointers;
    uint32_t actor_pointers_size;
    const uint8_t *actor_palette_groups;
    uint32_t actor_palette_groups_size;
    const uint8_t *r2_selectors;
    uint32_t r2_selector_count;
    uint8_t live_band_starts[TECMO_GAMEPLAY_LIVE_BAND_COUNT];
    uint8_t live_band_defaults[12];
    uint32_t chr_fingerprint32;
    uint64_t chr_fingerprint64;
} TecmoGameplayAssets;

/* Assets must be initialized once before parse/load/destroy. Parse and load
   release any prior successful storage, including when the replacement is
   rejected. */
void tecmo_gameplay_assets_init(TecmoGameplayAssets *assets);

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
   clip name. Every bit-7 cell is omitted exactly as fixed $D498 BPL does. */
bool tecmo_gameplay_assets_resolve_pose(
    const TecmoGameplayAssets *assets,
    uint16_t pointer_index,
    const TecmoGameplayPoseContext *context,
    TecmoGameplayResolvedPose *pose);

/* Builds the six-band live-play context sourced by fixed $E937-$E95D. The
   final R1 selector is team/context dependent and therefore explicit. */
bool tecmo_gameplay_assets_build_live_background_context(
    const TecmoGameplayAssets *assets,
    uint8_t final_r1_selector,
    TecmoGameplayLiveBackgroundContext *context);

uint8_t tecmo_gameplay_assets_live_band_for_scanline(uint8_t scanline);

/* Resolves an orientation-base nametable tile through the live IRQ band CHR
   mapping. The top HUD nametable receives dynamic writes and is intentionally
   not synthesized by this ROM-static asset contract. */
bool tecmo_gameplay_assets_resolve_live_orientation_tile(
    const TecmoGameplayAssets *assets,
    uint8_t screen_index,
    uint8_t nametable_page,
    uint8_t row,
    uint8_t column,
    uint8_t scanline,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayResolvedOrientationTile *tile);

#endif
