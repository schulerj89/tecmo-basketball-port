#include "tecmo_intro_stage.h"
#include "tecmo_bank07.h"

#include <stdio.h>
#include <string.h>

#define TECMO_INTRO_ARENA_WAIT_FRAMES 0x96U
#define TECMO_INTRO_ARENA_INITIAL_BASE_Y 0xFEU
#define TECMO_INTRO_ARENA_FINAL_BASE_Y 0x38U
#define TECMO_INTRO_ARENA_SCROLL_Y_STEPS 99U
#define TECMO_INTRO_ARENA_SCROLL_START_8A 0x3CU
#define TECMO_INTRO_ARENA_SCROLL_WRAP_TICK 0xC4U
#define TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS 43U

size_t tecmo_intro_stage_sprite_records(const TecmoIntroSpriteRecord *records,
                                        size_t record_count,
                                        const TecmoIntroSpriteStageConfig *config,
                                        TecmoIntroStagedSprite *entries,
                                        size_t entry_capacity)
{
    TecmoBank07SpriteStageConfig bank_config;
    size_t staged_count = 0;
    if (records == NULL || config == NULL || entries == NULL || entry_capacity == 0) {
        return 0;
    }

    bank_config.base_x = config->base_x;
    bank_config.base_y = config->base_y;
    bank_config.tile_offset = config->tile_offset;
    bank_config.attribute_or = 0U;

    for (size_t i = 0; i < record_count && staged_count < entry_capacity; ++i) {
        const TecmoIntroSpriteRecord *record = &records[i];
        TecmoBank07SpriteRecord bank_record;
        TecmoBank07OamSprite bank_entry;
        TecmoIntroStagedSprite *entry = &entries[staged_count];

        bank_record.relative_y = record->relative_y;
        bank_record.tile = record->tile;
        bank_record.attributes = record->attributes;
        bank_record.relative_x = record->relative_x;
        if (tecmo_bank07_d861_stage_sprite_records(&bank_record, 1, &bank_config, &bank_entry, 1) != 1) {
            break;
        }

        entry->y = bank_entry.y;
        entry->tile = bank_entry.tile;
        entry->attributes = bank_entry.attributes;
        entry->x = bank_entry.x;
        ++staged_count;
    }

    return staged_count;
}

static void set_intro_stage_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static bool check_intro_stage_entry(const TecmoIntroStagedSprite *entry,
                                    uint8_t y,
                                    uint8_t tile,
                                    uint8_t attributes,
                                    uint8_t x)
{
    return entry->y == y &&
           entry->tile == tile &&
           entry->attributes == attributes &&
           entry->x == x;
}

static bool check_intro_arena_state(const TecmoIntroArenaTransitionState *state,
                                    TecmoIntroArenaPhase phase,
                                    uint8_t base_y,
                                    uint8_t scroll_8a,
                                    uint8_t scroll_y_0301)
{
    return state->phase == phase &&
           state->base_y == base_y &&
           state->scroll_8a == scroll_8a &&
           state->scroll_y_0301 == scroll_y_0301 &&
           state->seed_88 == 0xA8U &&
           state->seed_57 == 0x08U &&
           state->seed_58 == 0x09U &&
           state->seed_07eb == 0x00U &&
           state->seed_07ec == 0x1EU &&
           state->seed_20 == 0x00U &&
           state->seed_21 == 0x01U &&
           state->irq_0100 == 0x05U &&
           state->mapper_select_0352 == 0x01U;
}

bool tecmo_intro_stage_self_test(char *message, size_t message_size)
{
    const TecmoIntroSpriteRecord records[] = {
        {2, 0x10U, 0x01U, 3},
        {-30, 0x20U, 0x02U, 4},
        {5, 0xF8U, 0x03U, 300},
    };
    const TecmoIntroSpriteStageConfig config = {10, 20, 0x0DU};
    TecmoIntroStagedSprite entries[3];
    TecmoIntroArenaTransitionState arena_state;
    uint16_t pair[2] = {0, 0};
    size_t count;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }

    if (tecmo_intro_stage_sprite_records(NULL, 1, &config, entries, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, NULL, entries, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, &config, NULL, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, &config, entries, 0) != 0) {
        set_intro_stage_test_message(message, message_size, "NULL OR ZERO INPUT CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_intro_stage_sprite_records(records, 3, &config, entries, 2);
    if (count != 2) {
        set_intro_stage_test_message(message, message_size, "CAPACITY TRUNCATION CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[0], 0x16U, 0x1DU, 0x01U, 0x0DU)) {
        set_intro_stage_test_message(message, message_size, "FIRST STAGED ENTRY CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[1], 0xF6U, 0x2DU, 0x02U, 0x0EU)) {
        set_intro_stage_test_message(message, message_size, "BYTE WRAP STAGED ENTRY CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_intro_stage_sprite_records(records, 3, &config, entries, 3);
    if (count != 3) {
        set_intro_stage_test_message(message, message_size, "FULL STAGED COUNT CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[2], 0x19U, 0x05U, 0x03U, 0x36U)) {
        set_intro_stage_test_message(message, message_size, "TILE AND X WRAP CONTRACT FAILED");
        return false;
    }

    tecmo_intro_sprite_8x16_pair_for_table(0x25U, 1U, pair);
    if (pair[0] != 0x124U || pair[1] != 0x125U) {
        set_intro_stage_test_message(message, message_size, "RABBIT 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }
    tecmo_intro_sprite_8x16_pair_for_table(0x80U, 1U, pair);
    if (pair[0] != 0x180U || pair[1] != 0x181U) {
        set_intro_stage_test_message(message, message_size, "TECMO 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }
    tecmo_intro_sprite_8x16_pair_for_table(0x25U, 0U, pair);
    if (pair[0] != 0x024U || pair[1] != 0x025U) {
        set_intro_stage_test_message(message, message_size, "TABLE ZERO 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }

    tecmo_intro_arena_transition_state(0U, &arena_state);
    if (!check_intro_arena_state(&arena_state,
                                 TECMO_INTRO_ARENA_PHASE_WAIT,
                                 TECMO_INTRO_ARENA_INITIAL_BASE_Y,
                                 TECMO_INTRO_ARENA_SCROLL_START_8A,
                                 0x00U)) {
        set_intro_stage_test_message(message, message_size, "ARENA INITIAL WAIT STATE CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + 88U, &arena_state);
    if (!check_intro_arena_state(&arena_state, TECMO_INTRO_ARENA_PHASE_SCROLL, 0xA6U, 0x69U, 0x2DU)) {
        set_intro_stage_test_message(message, message_size, "ARENA SCROLL CHECKPOINT CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + 198U, &arena_state);
    if (!check_intro_arena_state(&arena_state, TECMO_INTRO_ARENA_PHASE_SETTLE, 0x38U, 0xA0U, 0x64U)) {
        set_intro_stage_test_message(message, message_size, "ARENA SETTLE CHECKPOINT CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + ((TECMO_INTRO_ARENA_SCROLL_WRAP_TICK - 1U) * 2U), &arena_state);
    if (!check_intro_arena_state(&arena_state, TECMO_INTRO_ARENA_PHASE_WRAP, 0x38U, 0x00U, 0x77U)) {
        set_intro_stage_test_message(message, message_size, "ARENA WRAP CHECKPOINT CONTRACT FAILED");
        return false;
    }

    set_intro_stage_test_message(message, message_size, "INTRO SPRITE STAGING SELF TEST PASS");
    return true;
}

void tecmo_intro_arena_transition_state(unsigned frame, TecmoIntroArenaTransitionState *state)
{
    unsigned tick = 0;
    unsigned y_steps = 0;

    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->frame = frame;
    state->phase = TECMO_INTRO_ARENA_PHASE_WAIT;
    state->base_y = TECMO_INTRO_ARENA_INITIAL_BASE_Y;
    state->scroll_8a = TECMO_INTRO_ARENA_SCROLL_START_8A;
    state->scroll_y_0301 = 0x00U;
    state->seed_88 = 0xA8U;
    state->seed_57 = 0x08U;
    state->seed_58 = 0x09U;
    state->seed_07eb = 0x00U;
    state->seed_07ec = 0x1EU;
    state->seed_20 = 0x00U;
    state->seed_21 = 0x01U;
    state->irq_0100 = 0x05U;
    state->mapper_select_0352 = 0x01U;

    if (frame < TECMO_INTRO_ARENA_WAIT_FRAMES) {
        return;
    }

    tick = ((frame - TECMO_INTRO_ARENA_WAIT_FRAMES) / 2U) + 1U;
    state->scroll_8a = (uint8_t)(TECMO_INTRO_ARENA_SCROLL_START_8A + tick);
    state->scroll_y_0301 = (uint8_t)(tick > 0x77U ? 0x77U : tick);
    y_steps = tick - 1U;
    if (y_steps < TECMO_INTRO_ARENA_SCROLL_Y_STEPS) {
        state->phase = TECMO_INTRO_ARENA_PHASE_SCROLL;
        state->base_y = (uint8_t)(TECMO_INTRO_ARENA_INITIAL_BASE_Y - y_steps * 2U);
    } else {
        state->phase = TECMO_INTRO_ARENA_PHASE_SETTLE;
        state->base_y = TECMO_INTRO_ARENA_FINAL_BASE_Y;
    }

    if (tick > TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS) {
        state->base_x_offset = -2 * (int)(tick - TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS);
    }
    if (tick >= TECMO_INTRO_ARENA_SCROLL_WRAP_TICK) {
        state->phase = TECMO_INTRO_ARENA_PHASE_WRAP;
    }
}

const char *tecmo_intro_arena_phase_name(TecmoIntroArenaPhase phase)
{
    if (phase == TECMO_INTRO_ARENA_PHASE_WAIT) {
        return "WAIT";
    }
    if (phase == TECMO_INTRO_ARENA_PHASE_SCROLL) {
        return "SCROLL";
    }
    if (phase == TECMO_INTRO_ARENA_PHASE_SETTLE) {
        return "SETTLE";
    }
    if (phase == TECMO_INTRO_ARENA_PHASE_WRAP) {
        return "WRAP";
    }
    return "UNKNOWN";
}

void tecmo_intro_sprite_8x16_pair_for_table(uint8_t oam_tile_low, uint32_t chr_table, uint16_t out_tiles[2])
{
    tecmo_bank07_sprite_8x16_pair_for_table(oam_tile_low, chr_table, out_tiles);
}

uint16_t tecmo_intro_oam_tile_pair_top(uint8_t oam_tile_low, uint32_t chr_table)
{
    uint16_t pair[2] = {0, 0};
    tecmo_intro_sprite_8x16_pair_for_table(oam_tile_low, chr_table, pair);
    return pair[0];
}

uint16_t tecmo_intro_oam_tile_pair_bottom(uint8_t oam_tile_low, uint32_t chr_table)
{
    uint16_t pair[2] = {0, 0};
    tecmo_intro_sprite_8x16_pair_for_table(oam_tile_low, chr_table, pair);
    return pair[1];
}
