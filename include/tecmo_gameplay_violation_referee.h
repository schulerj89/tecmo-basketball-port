#ifndef TECMO_GAMEPLAY_VIOLATION_REFEREE_H
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_H

#include "tecmo_framebuffer.h"
#include "tecmo_gameplay_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT 10U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT 7U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT 5U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT 15U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_MESSAGE_TILES 20U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS 8U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_PIECES 34U

/* The group cadence and sequence start are ROM-derived from Bank04 $BA1F.
   The nine-frame loader blackout and the visible fade alignment are bounded
   by the local Rev1 FCEUX capture because the generic PPU-loader runtime has
   not yet been ported cycle-for-cycle. */
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_BLACK_FRAMES 9U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_FADE_STEP_FRAMES 4U
#define TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_START_FRAME 23U

typedef enum TecmoGameplayViolationRefereeSourceKind {
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_ROUTE = 1,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_TEXT_ROUTINE,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_CHARACTER_MAP,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SCREEN_DESCRIPTOR,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SCREEN_STREAM,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_BACKGROUND_PALETTE,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_CONTROLLER,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SPRITE_PALETTE,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_SEQUENCE_TABLES,
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_METASPRITES
} TecmoGameplayViolationRefereeSourceKind;

typedef struct TecmoGameplayViolationRefereeSourceSpan {
    TecmoGameplayViolationRefereeSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayViolationRefereeSourceSpan;

typedef struct TecmoGameplayViolationRefereeMessage {
    uint8_t selector;
    TecmoGameplayViolation violation;
    uint8_t sequence_id;
    uint8_t tile_count;
    uint16_t ppu_address;
    const uint8_t *tiles;
} TecmoGameplayViolationRefereeMessage;

typedef struct TecmoGameplayViolationRefereeSequence {
    uint8_t id;
    uint8_t group_count;
    uint8_t groups[TECMO_GAMEPLAY_VIOLATION_REFEREE_MAX_SEQUENCE_GROUPS];
} TecmoGameplayViolationRefereeSequence;

typedef struct TecmoGameplayViolationRefereeGroup {
    uint8_t id;
    uint8_t piece_count;
    uint8_t base_y;
    uint8_t base_x;
    uint16_t record_cpu;
    const uint8_t *pieces;
} TecmoGameplayViolationRefereeGroup;

typedef struct TecmoGameplayViolationRefereeAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[192];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayViolationRefereeSourceSpan
        sources[TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT];
    TecmoGameplayViolationRefereeMessage
        messages[TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT];
    TecmoGameplayViolationRefereeSequence
        sequences[TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_COUNT];
    TecmoGameplayViolationRefereeGroup
        groups[TECMO_GAMEPLAY_VIOLATION_REFEREE_GROUP_COUNT];
    const uint8_t *decoded_screen;
    const uint8_t *background_palette;
    const uint8_t *sprite_palette;
    uint8_t background_chr_selectors[2];
    uint8_t sprite_chr_selectors[4];
    uint32_t chr_fingerprint;
    uint32_t penalty_fingerprint;
} TecmoGameplayViolationRefereeAssets;

void tecmo_gameplay_violation_referee_init(
    TecmoGameplayViolationRefereeAssets *assets);
void tecmo_gameplay_violation_referee_destroy(
    TecmoGameplayViolationRefereeAssets *assets);

bool tecmo_gameplay_violation_referee_parse(
    TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *chr,
    size_t chr_size,
    const uint8_t *penalties,
    size_t penalties_size);
bool tecmo_gameplay_violation_referee_load(
    TecmoGameplayViolationRefereeAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayViolationRefereeSourceSpan *
tecmo_gameplay_violation_referee_find_source(
    const TecmoGameplayViolationRefereeAssets *assets,
    TecmoGameplayViolationRefereeSourceKind kind);

bool tecmo_gameplay_violation_referee_group_for_frame(
    const TecmoGameplayViolationRefereeAssets *assets,
    TecmoGameplayViolation violation,
    uint16_t phase_frame,
    uint8_t *group_id);

/* Fixed $E95E selects $22, whose Bank04 $BA1F controller maps selector 0
   to the first referee sequence ($B317): groups 1, 2, 2, 2.  Foul state
   currently owns no Bank02 $B0F8 text payload, so this API deliberately
   exposes only the source-derived visual controller, not synthetic text. */
bool tecmo_gameplay_violation_referee_foul_group_for_frame(
    const TecmoGameplayViolationRefereeAssets *assets,
    uint16_t phase_frame,
    uint8_t *group_id);

bool tecmo_gameplay_violation_referee_draw(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    TecmoGameplayViolation violation,
    uint16_t phase_frame);

/* Draw the fixed-script foul cutaway: TGVR screen/palettes/CHR plus the
   Bank04 selector-0 referee metasprite sequence. */
bool tecmo_gameplay_violation_referee_draw_foul(
    const TecmoGameplayViolationRefereeAssets *assets,
    const uint8_t *chr,
    size_t chr_size,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    uint16_t phase_frame);

bool tecmo_gameplay_violation_referee_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
