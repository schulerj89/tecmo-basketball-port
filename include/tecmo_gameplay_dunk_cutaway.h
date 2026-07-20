#ifndef TECMO_GAMEPLAY_DUNK_CUTAWAY_H
#define TECMO_GAMEPLAY_DUNK_CUTAWAY_H

#include "tecmo_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_DUNK_SOURCE_COUNT 18U
#define TECMO_GAMEPLAY_DUNK_SIDE_COUNT 2U
#define TECMO_GAMEPLAY_DUNK_STAGE_COUNT 7U
#define TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT 960U
#define TECMO_GAMEPLAY_DUNK_CELL_COUNT \
    (TECMO_GAMEPLAY_DUNK_SIDE_COUNT * TECMO_GAMEPLAY_DUNK_PAGE_CELL_COUNT)
#define TECMO_GAMEPLAY_DUNK_MAX_PIECES 64U

#define TECMO_GAMEPLAY_DUNK_DISPATCH_FRAME 23U
#define TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME 24U
#define TECMO_GAMEPLAY_DUNK_FIRST_ASSIGN_FRAME 27U
#define TECMO_GAMEPLAY_DUNK_FIRST_VISIBLE_FRAME 28U
#define TECMO_GAMEPLAY_DUNK_STAGE_CADENCE 5U
#define TECMO_GAMEPLAY_DUNK_LAST_ASSIGN_FRAME 57U
#define TECMO_GAMEPLAY_DUNK_LAST_VISIBLE_FRAME 62U
#define TECMO_GAMEPLAY_DUNK_PALETTE_BLACK_FRAME 63U
#define TECMO_GAMEPLAY_DUNK_CLEAR_FRAME 64U
#define TECMO_GAMEPLAY_DUNK_COURT_REBUILD_FRAME 66U
#define TECMO_GAMEPLAY_DUNK_LIVE_RETURN_FRAME 71U
#define TECMO_GAMEPLAY_DUNK_ROUTE_RESUME_FRAME 75U
#define TECMO_GAMEPLAY_DUNK_A9C5_FRAME 87U
#define TECMO_GAMEPLAY_DUNK_RESOLVE_FRAME 132U

typedef enum TecmoGameplayDunkSourceKind {
    TECMO_GAMEPLAY_DUNK_SOURCE_TRIGGER = 1,
    TECMO_GAMEPLAY_DUNK_SOURCE_CLEAR_LANE_HELPER,
    TECMO_GAMEPLAY_DUNK_SOURCE_FIXED_DISPATCH,
    TECMO_GAMEPLAY_DUNK_SOURCE_SCREEN_DESCRIPTOR,
    TECMO_GAMEPLAY_DUNK_SOURCE_SCREEN_STREAM,
    TECMO_GAMEPLAY_DUNK_SOURCE_BASE_PALETTE,
    TECMO_GAMEPLAY_DUNK_SOURCE_CONTROLLER,
    TECMO_GAMEPLAY_DUNK_SOURCE_STAGE_SETUP,
    TECMO_GAMEPLAY_DUNK_SOURCE_STAGE_TABLES,
    TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_DISPATCH,
    TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_BANK_TABLE,
    TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_ADDRESS_LOW,
    TECMO_GAMEPLAY_DUNK_SOURCE_SELECTOR_ADDRESS_HIGH,
    TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_EMITTER,
    TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_POINTERS,
    TECMO_GAMEPLAY_DUNK_SOURCE_SPRITE_GEOMETRY,
    TECMO_GAMEPLAY_DUNK_SOURCE_PALETTE_RECIPE,
    TECMO_GAMEPLAY_DUNK_SOURCE_COURT_RESTORE
} TecmoGameplayDunkSourceKind;

typedef struct TecmoGameplayDunkSourceSpan {
    TecmoGameplayDunkSourceKind kind;
    uint8_t bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayDunkSourceSpan;

typedef struct TecmoGameplayDunkCell {
    uint32_t chr_offset;
    uint8_t palette_index;
} TecmoGameplayDunkCell;

typedef struct TecmoGameplayDunkStage {
    uint16_t assignment_frame;
    uint16_t visible_frame;
    uint8_t anchor_y;
    uint8_t anchor_x[TECMO_GAMEPLAY_DUNK_SIDE_COUNT];
    uint8_t sprite_chr_page;
    uint8_t record_slot;
} TecmoGameplayDunkStage;

typedef struct TecmoGameplayDunkRecord {
    uint8_t piece_count;
    const uint8_t *pieces;
} TecmoGameplayDunkRecord;

typedef struct TecmoGameplayDunkCutawayAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[192];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayDunkSourceSpan sources[TECMO_GAMEPLAY_DUNK_SOURCE_COUNT];
    TecmoGameplayDunkCell cells[TECMO_GAMEPLAY_DUNK_CELL_COUNT];
    TecmoGameplayDunkStage stages[TECMO_GAMEPLAY_DUNK_STAGE_COUNT];
    TecmoGameplayDunkRecord
        records[TECMO_GAMEPLAY_DUNK_SIDE_COUNT][TECMO_GAMEPLAY_DUNK_STAGE_COUNT];
    const uint8_t *reference_palette;
    uint32_t chr_fingerprint;
} TecmoGameplayDunkCutawayAssets;

void tecmo_gameplay_dunk_cutaway_init(
    TecmoGameplayDunkCutawayAssets *assets);
void tecmo_gameplay_dunk_cutaway_destroy(
    TecmoGameplayDunkCutawayAssets *assets);
bool tecmo_gameplay_dunk_cutaway_parse(
    TecmoGameplayDunkCutawayAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *chr,
    size_t chr_size);
bool tecmo_gameplay_dunk_cutaway_load(
    TecmoGameplayDunkCutawayAssets *assets,
    const char *asset_pack_path);

bool tecmo_gameplay_dunk_cutaway_palette(
    const TecmoGameplayDunkCutawayAssets *assets,
    uint8_t profile,
    uint8_t uniform_color,
    uint8_t palette[32]);
bool tecmo_gameplay_dunk_cutaway_stage_for_frame(
    const TecmoGameplayDunkCutawayAssets *assets,
    uint16_t frame,
    uint8_t *stage_index);
bool tecmo_gameplay_dunk_cutaway_draw(
    const TecmoGameplayDunkCutawayAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint8_t side,
    uint8_t profile,
    uint8_t uniform_color,
    uint8_t stage_index);
bool tecmo_gameplay_dunk_cutaway_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
