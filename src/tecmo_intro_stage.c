#include "tecmo_intro_stage.h"
#include "tecmo_bank07.h"

#include <stdio.h>
#include <string.h>

#define TECMO_INTRO_ARENA_WAIT_FRAMES 0x96U
#define TECMO_INTRO_ARENA_INITIAL_BASE_Y 0xFEU
#define TECMO_INTRO_ARENA_FINAL_BASE_Y 0x38U
#define TECMO_INTRO_ARENA_FINAL_SCROLL_Y 0x64U
#define TECMO_INTRO_ARENA_SCROLL_START_8A 0x3CU
#define TECMO_INTRO_ARENA_SCROLL_WRAP_TICK 0xC4U
#define TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS 43U
#define TECMO_INTRO_ARENA_MOTION_START_88 0xA8U
#define TECMO_INTRO_ARENA_MOTION_GATE_88 0x44U
#define TECMO_INTRO_ARENA_SCROLL_LIMIT_0301 0x77U
#define TECMO_INTRO_ARENA_STREAM1_INC_START_0301 0x32U
#define TECMO_INTRO_ARENA_STREAM1_INC_END_0301 0x50U
#define TECMO_INTRO_ARENA_STREAM0_PTR_LOW 0x00U
#define TECMO_INTRO_ARENA_STREAM0_PTR_HIGH 0x00U
#define TECMO_INTRO_ARENA_STREAM1_PTR_LOW 0xB8U
#define TECMO_INTRO_ARENA_STREAM1_PTR_HIGH 0x01U
#define TECMO_INTRO_ARENA_STREAM0_SEED_LOW 0x00U
#define TECMO_INTRO_ARENA_STREAM0_SEED_HIGH 0x00U
#define TECMO_INTRO_ARENA_STREAM1_SEED_LOW 0x1EU
#define TECMO_INTRO_ARENA_STREAM1_SEED_HIGH 0x01U
#define TECMO_INTRO_ARENA_SEED_57 0x08U
#define TECMO_INTRO_ARENA_SEED_58 0x09U
#define TECMO_INTRO_ARENA_IRQ_0100 0x05U
#define TECMO_INTRO_ARENA_MAPPER_SELECT_0352 0x01U

typedef enum TecmoIntroArenaAsmPc {
    TECMO_INTRO_ARENA_ASM_L88E7_WAIT_96,
    TECMO_INTRO_ARENA_ASM_L892C_WAIT_2,
    TECMO_INTRO_ARENA_ASM_L8983_DONE
} TecmoIntroArenaAsmPc;

typedef struct TecmoIntroArenaAsmMachine {
    TecmoIntroArenaAsmPc pc;
    unsigned loop_tick;
    unsigned next_loop_frame;
    uint8_t zp_0301;
    uint8_t zp_07eb;
    uint8_t zp_07ec;
    uint8_t zp_20;
    uint8_t zp_21;
    uint8_t zp_57;
    uint8_t zp_58;
    uint8_t zp_88;
    uint8_t zp_8a;
    uint8_t irq_0100;
    uint8_t mapper_select_0352;
    uint8_t emit_reset_058d;
    uint8_t emit_pass_count;
    TecmoIntroArenaEmitPassState emit_passes[TECMO_INTRO_ARENA_EMIT_PASS_COUNT];
} TecmoIntroArenaAsmMachine;

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

static void intro_arena_subtract_stream(uint8_t *low, uint8_t *high, uint8_t value)
{
    uint8_t before = *low;
    *low = (uint8_t)(before - value);
    if (before < value) {
        *high = (uint8_t)(*high - 1U);
    }
}

static void intro_arena_add_stream(uint8_t *low, uint8_t *high, uint8_t value)
{
    uint8_t before = *low;
    *low = (uint8_t)(before + value);
    if (*low < before) {
        *high = (uint8_t)(*high + 1U);
    }
}

static void intro_arena_set_emit_pass(TecmoIntroArenaAsmMachine *machine,
                                      size_t pass_index,
                                      uint8_t stream_index)
{
    TecmoIntroArenaEmitPassState *pass;
    if (machine == NULL || pass_index >= TECMO_INTRO_ARENA_EMIT_PASS_COUNT) {
        return;
    }

    pass = &machine->emit_passes[pass_index];
    memset(pass, 0, sizeof(*pass));
    pass->stream_index = stream_index;
    if (stream_index == 1U) {
        pass->pointer_low = TECMO_INTRO_ARENA_STREAM1_PTR_LOW;
        pass->pointer_high = TECMO_INTRO_ARENA_STREAM1_PTR_HIGH;
        pass->stream_low = machine->zp_07ec;
        pass->stream_high = machine->zp_21;
    } else {
        pass->pointer_low = TECMO_INTRO_ARENA_STREAM0_PTR_LOW;
        pass->pointer_high = TECMO_INTRO_ARENA_STREAM0_PTR_HIGH;
        pass->stream_low = machine->zp_07eb;
        pass->stream_high = machine->zp_20;
    }
}

static void intro_arena_l8988_emit(TecmoIntroArenaAsmMachine *machine)
{
    if (machine == NULL) {
        return;
    }

    machine->emit_reset_058d = 0U;
    machine->emit_pass_count = TECMO_INTRO_ARENA_EMIT_PASS_COUNT;
    intro_arena_set_emit_pass(machine, 0U, 1U);
    intro_arena_set_emit_pass(machine, 1U, 0U);
}

static void intro_arena_l88e7_enter(TecmoIntroArenaAsmMachine *machine)
{
    memset(machine, 0, sizeof(*machine));
    machine->pc = TECMO_INTRO_ARENA_ASM_L88E7_WAIT_96;
    machine->next_loop_frame = TECMO_INTRO_ARENA_WAIT_FRAMES;
    machine->zp_88 = TECMO_INTRO_ARENA_MOTION_START_88;
    machine->zp_8a = TECMO_INTRO_ARENA_SCROLL_START_8A;
    machine->mapper_select_0352 = TECMO_INTRO_ARENA_MAPPER_SELECT_0352;
    machine->irq_0100 = TECMO_INTRO_ARENA_IRQ_0100;
    machine->zp_07eb = TECMO_INTRO_ARENA_STREAM0_SEED_LOW;
    machine->zp_07ec = TECMO_INTRO_ARENA_STREAM1_SEED_LOW;
    machine->zp_20 = TECMO_INTRO_ARENA_STREAM0_SEED_HIGH;
    machine->zp_21 = TECMO_INTRO_ARENA_STREAM1_SEED_HIGH;
    machine->zp_57 = TECMO_INTRO_ARENA_SEED_57;
    machine->zp_58 = TECMO_INTRO_ARENA_SEED_58;
    intro_arena_l8988_emit(machine);
}

static void intro_arena_l892c_tick(TecmoIntroArenaAsmMachine *machine)
{
    if (machine == NULL || machine->pc == TECMO_INTRO_ARENA_ASM_L8983_DONE) {
        return;
    }

    machine->pc = TECMO_INTRO_ARENA_ASM_L892C_WAIT_2;
    ++machine->loop_tick;
    machine->zp_8a = (uint8_t)(machine->zp_8a + 1U);

    if (machine->zp_88 >= TECMO_INTRO_ARENA_MOTION_GATE_88) {
        if (machine->zp_0301 < TECMO_INTRO_ARENA_SCROLL_LIMIT_0301) {
            machine->zp_0301 = (uint8_t)(machine->zp_0301 + 1U);
            intro_arena_subtract_stream(&machine->zp_07eb, &machine->zp_20, 2U);
            if (machine->zp_0301 >= TECMO_INTRO_ARENA_STREAM1_INC_START_0301) {
                if (machine->zp_0301 < TECMO_INTRO_ARENA_STREAM1_INC_END_0301) {
                    intro_arena_add_stream(&machine->zp_07ec, &machine->zp_21, 1U);
                }
                intro_arena_subtract_stream(&machine->zp_07ec, &machine->zp_21, 2U);
                machine->zp_88 = (uint8_t)(machine->zp_88 - 2U);
            }
        } else {
            intro_arena_subtract_stream(&machine->zp_07ec, &machine->zp_21, 2U);
            machine->zp_88 = (uint8_t)(machine->zp_88 - 2U);
        }
    }

    intro_arena_l8988_emit(machine);
    if (machine->zp_8a == 0U) {
        machine->pc = TECMO_INTRO_ARENA_ASM_L8983_DONE;
    }
}

static void intro_arena_run_until_frame(unsigned frame, TecmoIntroArenaAsmMachine *machine)
{
    intro_arena_l88e7_enter(machine);
    while (machine->pc != TECMO_INTRO_ARENA_ASM_L8983_DONE &&
           frame >= machine->next_loop_frame) {
        intro_arena_l892c_tick(machine);
        if (machine->pc != TECMO_INTRO_ARENA_ASM_L8983_DONE) {
            machine->next_loop_frame += 2U;
        }
    }
}

static bool check_intro_arena_emit_pass(const TecmoIntroArenaTransitionState *state,
                                        size_t pass_index,
                                        uint8_t stream_index,
                                        uint8_t pointer_low,
                                        uint8_t pointer_high,
                                        uint8_t stream_low,
                                        uint8_t stream_high)
{
    const TecmoIntroArenaEmitPassState *pass;
    if (state == NULL || pass_index >= TECMO_INTRO_ARENA_EMIT_PASS_COUNT) {
        return false;
    }

    pass = &state->emit_passes[pass_index];
    return pass->stream_index == stream_index &&
           pass->pointer_low == pointer_low &&
           pass->pointer_high == pointer_high &&
           pass->stream_low == stream_low &&
           pass->stream_high == stream_high;
}

static bool check_intro_arena_state(const TecmoIntroArenaTransitionState *state,
                                    TecmoIntroArenaPhase phase,
                                    uint8_t base_y,
                                    uint8_t scroll_8a,
                                    uint8_t scroll_y_0301,
                                    uint8_t stream0_low,
                                    uint8_t stream0_high,
                                    uint8_t stream1_low,
                                    uint8_t stream1_high,
                                    unsigned loop_tick,
                                    uint8_t motion_counter_88)
{
    return state->phase == phase &&
           state->base_y == base_y &&
           state->scroll_8a == scroll_8a &&
           state->scroll_y_0301 == scroll_y_0301 &&
           state->seed_88 == motion_counter_88 &&
           state->seed_57 == TECMO_INTRO_ARENA_SEED_57 &&
           state->seed_58 == TECMO_INTRO_ARENA_SEED_58 &&
           state->seed_07eb == stream0_low &&
           state->seed_07ec == stream1_low &&
           state->seed_20 == stream0_high &&
           state->seed_21 == stream1_high &&
           state->irq_0100 == TECMO_INTRO_ARENA_IRQ_0100 &&
           state->mapper_select_0352 == TECMO_INTRO_ARENA_MAPPER_SELECT_0352 &&
           state->stream0_low == stream0_low &&
           state->stream0_high == stream0_high &&
           state->stream1_low == stream1_low &&
           state->stream1_high == stream1_high &&
           state->loop_tick == loop_tick &&
           state->handoff_timer_8a == scroll_8a &&
           state->motion_counter_88 == motion_counter_88 &&
           state->emit_reset_058d == 0U &&
           state->emit_pass_count == TECMO_INTRO_ARENA_EMIT_PASS_COUNT &&
           check_intro_arena_emit_pass(state,
                                       0U,
                                       1U,
                                       TECMO_INTRO_ARENA_STREAM1_PTR_LOW,
                                       TECMO_INTRO_ARENA_STREAM1_PTR_HIGH,
                                       stream1_low,
                                       stream1_high) &&
           check_intro_arena_emit_pass(state,
                                       1U,
                                       0U,
                                       TECMO_INTRO_ARENA_STREAM0_PTR_LOW,
                                       TECMO_INTRO_ARENA_STREAM0_PTR_HIGH,
                                       stream0_low,
                                       stream0_high);
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
                                 0x00U,
                                 TECMO_INTRO_ARENA_STREAM0_SEED_LOW,
                                 TECMO_INTRO_ARENA_STREAM0_SEED_HIGH,
                                 TECMO_INTRO_ARENA_STREAM1_SEED_LOW,
                                 TECMO_INTRO_ARENA_STREAM1_SEED_HIGH,
                                 0U,
                                 TECMO_INTRO_ARENA_MOTION_START_88)) {
        set_intro_stage_test_message(message, message_size, "ARENA INITIAL WAIT STATE CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + 88U, &arena_state);
    if (!check_intro_arena_state(&arena_state,
                                 TECMO_INTRO_ARENA_PHASE_SCROLL,
                                 0xA6U,
                                 0x69U,
                                 0x2DU,
                                 0xA6U,
                                 0xFFU,
                                 0x1EU,
                                 0x01U,
                                 45U,
                                 TECMO_INTRO_ARENA_MOTION_START_88)) {
        set_intro_stage_test_message(message, message_size, "ARENA SCROLL CHECKPOINT CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + 198U, &arena_state);
    if (!check_intro_arena_state(&arena_state,
                                 TECMO_INTRO_ARENA_PHASE_SETTLE,
                                 0x38U,
                                 0xA0U,
                                 0x64U,
                                 0x38U,
                                 0xFFU,
                                 0xD6U,
                                 0x00U,
                                 100U,
                                 0x42U)) {
        set_intro_stage_test_message(message, message_size, "ARENA SETTLE CHECKPOINT CONTRACT FAILED");
        return false;
    }
    tecmo_intro_arena_transition_state(TECMO_INTRO_ARENA_WAIT_FRAMES + ((TECMO_INTRO_ARENA_SCROLL_WRAP_TICK - 1U) * 2U), &arena_state);
    if (!check_intro_arena_state(&arena_state,
                                 TECMO_INTRO_ARENA_PHASE_WRAP,
                                 0x38U,
                                 0x00U,
                                 TECMO_INTRO_ARENA_FINAL_SCROLL_Y,
                                 0x38U,
                                 0xFFU,
                                 0xD6U,
                                 0x00U,
                                 TECMO_INTRO_ARENA_SCROLL_WRAP_TICK,
                                 0x42U)) {
        set_intro_stage_test_message(message, message_size, "ARENA WRAP CHECKPOINT CONTRACT FAILED");
        return false;
    }

    set_intro_stage_test_message(message, message_size, "INTRO SPRITE STAGING SELF TEST PASS");
    return true;
}

void tecmo_intro_arena_transition_state(unsigned frame, TecmoIntroArenaTransitionState *state)
{
    TecmoIntroArenaAsmMachine machine;

    if (state == NULL) {
        return;
    }

    intro_arena_run_until_frame(frame, &machine);

    memset(state, 0, sizeof(*state));
    state->frame = frame;
    state->base_y = machine.loop_tick == 0U ? TECMO_INTRO_ARENA_INITIAL_BASE_Y : machine.zp_07eb;
    state->scroll_8a = machine.zp_8a;
    state->scroll_y_0301 = machine.zp_0301;
    state->seed_88 = machine.zp_88;
    state->seed_57 = machine.zp_57;
    state->seed_58 = machine.zp_58;
    state->seed_07eb = machine.zp_07eb;
    state->seed_07ec = machine.zp_07ec;
    state->seed_20 = machine.zp_20;
    state->seed_21 = machine.zp_21;
    state->irq_0100 = machine.irq_0100;
    state->mapper_select_0352 = machine.mapper_select_0352;
    state->stream0_low = machine.zp_07eb;
    state->stream0_high = machine.zp_20;
    state->stream1_low = machine.zp_07ec;
    state->stream1_high = machine.zp_21;
    state->loop_tick = machine.loop_tick;
    state->handoff_timer_8a = machine.zp_8a;
    state->motion_counter_88 = machine.zp_88;
    state->emit_reset_058d = machine.emit_reset_058d;
    state->emit_pass_count = machine.emit_pass_count;
    memcpy(state->emit_passes, machine.emit_passes, sizeof(state->emit_passes));

    if (machine.loop_tick == 0U) {
        state->phase = TECMO_INTRO_ARENA_PHASE_WAIT;
    } else if (machine.pc == TECMO_INTRO_ARENA_ASM_L8983_DONE) {
        state->phase = TECMO_INTRO_ARENA_PHASE_WRAP;
    } else if (machine.zp_07eb != TECMO_INTRO_ARENA_FINAL_BASE_Y) {
        state->phase = TECMO_INTRO_ARENA_PHASE_SCROLL;
    } else {
        state->phase = TECMO_INTRO_ARENA_PHASE_SETTLE;
    }

    if (machine.loop_tick > TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS) {
        state->base_x_offset = -2 * (int)(machine.loop_tick - TECMO_INTRO_ARENA_X_SHIFT_DELAY_TICKS);
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
