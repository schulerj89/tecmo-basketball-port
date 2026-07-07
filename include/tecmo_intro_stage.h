#ifndef TECMO_INTRO_STAGE_H
#define TECMO_INTRO_STAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct TecmoIntroSpriteRecord {
    int relative_y;
    uint8_t tile;
    uint8_t attributes;
    int relative_x;
} TecmoIntroSpriteRecord;

typedef struct TecmoIntroSpriteStageConfig {
    int base_x;
    int base_y;
    uint8_t tile_offset;
} TecmoIntroSpriteStageConfig;

typedef struct TecmoIntroStagedSprite {
    uint8_t y;
    uint8_t tile;
    uint8_t attributes;
    uint8_t x;
} TecmoIntroStagedSprite;

typedef enum TecmoIntroArenaPhase {
    TECMO_INTRO_ARENA_PHASE_WAIT,
    TECMO_INTRO_ARENA_PHASE_SCROLL,
    TECMO_INTRO_ARENA_PHASE_SETTLE,
    TECMO_INTRO_ARENA_PHASE_WRAP
} TecmoIntroArenaPhase;

typedef struct TecmoIntroArenaTransitionState {
    unsigned frame;
    TecmoIntroArenaPhase phase;
    uint8_t base_y;
    int base_x_offset;
    uint8_t scroll_8a;
    uint8_t scroll_y_0301;
    uint8_t seed_88;
    uint8_t seed_57;
    uint8_t seed_58;
    uint8_t seed_07eb;
    uint8_t seed_07ec;
    uint8_t seed_20;
    uint8_t seed_21;
    uint8_t irq_0100;
    uint8_t mapper_select_0352;
} TecmoIntroArenaTransitionState;

size_t tecmo_intro_stage_sprite_records(const TecmoIntroSpriteRecord *records,
                                        size_t record_count,
                                        const TecmoIntroSpriteStageConfig *config,
                                        TecmoIntroStagedSprite *entries,
                                        size_t entry_capacity);
bool tecmo_intro_stage_self_test(char *message, size_t message_size);
void tecmo_intro_arena_transition_state(unsigned frame, TecmoIntroArenaTransitionState *state);
const char *tecmo_intro_arena_phase_name(TecmoIntroArenaPhase phase);
void tecmo_intro_sprite_8x16_pair_for_table(uint8_t oam_tile_low,
                                            uint32_t chr_table,
                                            uint16_t out_tiles[2]);
uint16_t tecmo_intro_oam_tile_pair_top(uint8_t oam_tile_low, uint32_t chr_table);
uint16_t tecmo_intro_oam_tile_pair_bottom(uint8_t oam_tile_low, uint32_t chr_table);

#endif
