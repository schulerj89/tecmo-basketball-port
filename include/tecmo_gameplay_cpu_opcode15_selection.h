#ifndef TECMO_GAMEPLAY_CPU_OPCODE15_SELECTION_H
#define TECMO_GAMEPLAY_CPU_OPCODE15_SELECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_OPCODE15_SELECTION_TAG 0x4C353154U
#define TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_INPUT_TAG 0x49373154U
#define TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_RESULT_TAG 0x52373154U
#define TECMO_GAMEPLAY_CPU_OPCODE15_ACTOR_COUNT 10U

typedef struct TecmoGameplayCpuOpcode15SelectionLatch {
    uint32_t contract_tag;
    uint32_t write_serial;
    uint8_t actor_059e;
    bool valid;
    bool stale;
} TecmoGameplayCpuOpcode15SelectionLatch;

typedef struct TecmoGameplayCpuOpcode15State7Input {
    uint32_t contract_tag;
    uint32_t expected_write_serial;
    uint8_t primary_0308;
    uint8_t actor_state_057c[TECMO_GAMEPLAY_CPU_OPCODE15_ACTOR_COUNT];
    uint8_t actor_timer_046e[TECMO_GAMEPLAY_CPU_OPCODE15_ACTOR_COUNT];
} TecmoGameplayCpuOpcode15State7Input;

typedef struct TecmoGameplayCpuOpcode15State7Result {
    uint32_t contract_tag;
    uint32_t write_serial;
    uint8_t actor_059e;
    uint8_t scratch_00bf_actor;
    uint8_t scratch_00be_side;
    uint8_t c711_selector;
    bool consumed_stale_value;
    bool selector3_observed_unexecuted;
    bool retired_state7;
} TecmoGameplayCpuOpcode15State7Result;

bool tecmo_gameplay_cpu_opcode15_selection_init(
    TecmoGameplayCpuOpcode15SelectionLatch *latch);
bool tecmo_gameplay_cpu_opcode15_selection_write_920d(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    uint32_t expected_serial,
    uint8_t actor);
bool tecmo_gameplay_cpu_opcode15_selection_full_reset(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    uint32_t expected_serial);
bool tecmo_gameplay_cpu_opcode15_selection_retain_period(
    const TecmoGameplayCpuOpcode15SelectionLatch *latch);
bool tecmo_gameplay_cpu_opcode15_selection_retain_possession(
    const TecmoGameplayCpuOpcode15SelectionLatch *latch);

/* Pure `$9248-$926F` state-7 consumer. `$059E` is never source-cleared here:
 * the accepted consume only marks its typed provenance stale until `$920D`
 * writes again. State 7 is the source validity gate. */
bool tecmo_gameplay_cpu_opcode15_state7_consume(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    const TecmoGameplayCpuOpcode15State7Input *input,
    TecmoGameplayCpuOpcode15State7Input *output,
    TecmoGameplayCpuOpcode15State7Result *result_out);

bool tecmo_gameplay_cpu_opcode15_selection_self_test(char *message,
                                                     size_t message_size);

#endif
