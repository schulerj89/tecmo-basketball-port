#ifndef TECMO_GAMEPLAY_CPU_A9DA_ASSIGNMENT_H
#define TECMO_GAMEPLAY_CPU_A9DA_ASSIGNMENT_H

#include "tecmo_gameplay_cpu_global_latch.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_A9DA_INPUT_TAG 0x49394154U
#define TECMO_GAMEPLAY_CPU_A9DA_RESULT_TAG 0x52394154U
#define TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT 10U

typedef enum TecmoGameplayCpuA9daOutcome {
    TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ASSIGNED = 1,
    TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_BA = 2,
    TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_05A1 = 3
} TecmoGameplayCpuA9daOutcome;

/* Pure source-shaped workspace. `$AAB8` reads actor raw X low/high from
 * `$73+$X/$E8+$X` and 8-bit court depth from `$F3+$X`; altitude is not an
 * input to this helper and must never be substituted for court depth. */
typedef struct TecmoGameplayCpuA9daInput {
    uint32_t contract_tag;
    uint32_t expected_latch_serial;
    uint16_t ball_x;
    uint8_t ball_raw_depth_8;
    int16_t normalized_object10_vx_a9da;
    int16_t normalized_object10_vz_a9da;
    uint16_t multiplier_002c;
    uint8_t orientation_035a;
    uint8_t primary_0308;
    uint8_t defender_0309;
    uint8_t ba;
    uint8_t value_05a1;
    uint16_t actor_x[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint8_t actor_raw_depth_8[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    /* Canonical fixed `$06CB` tuple: 5,6,7,8,9,0,1,2,3,4. */
    uint8_t fixed_link_06cb[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint16_t stream_offset[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    /* Native command-cursor bookkeeping projected with each source stream
       seed; it is not an additional original `$A993` RAM write. */
    uint16_t last_step_offset[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint8_t state[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint8_t action_state_046e[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint8_t action_0458[TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT];
    uint8_t global_0587;
    uint8_t global_0588;
} TecmoGameplayCpuA9daInput;

typedef struct TecmoGameplayCpuA9daResult {
    uint32_t contract_tag;
    uint32_t latch_serial;
    uint16_t projected_x_038d_038e;
    uint16_t projected_depth_038f_0390;
    uint16_t winning_metric;
    uint8_t chosen_actor_002d;
    uint8_t linked_actor;
    /* `$A9DA-$A9E9` executes before every later BA/05A1 abort. `$B3DD` is
       the fixed producer `$0067=$F0,$0068=$02`; X is slot `$0A`, so the
       same bytes are stored at `$049A+$0A/$04A5+$0A`. */
    uint8_t object_state_0478;
    uint8_t raw_0067;
    uint8_t raw_0068;
    uint8_t object10_049a;
    uint8_t object10_04af;
    TecmoGameplayCpuA9daOutcome outcome;
    bool latch_overwritten;
    bool state10_and_b3dd_committed;
    bool flag_0588_set;
    bool same_loop_first_002d;
    bool linked_actor_exempt;
} TecmoGameplayCpuA9daResult;

/* Source transaction through `$A993`: `$A9DA` commits state `$10`, the fixed
 * `$B3DD` motion bytes, and its TGGL producer before `$BA/$05A1` can abort the
 * later `$AAB8->$A993` assignment. The `$4010/$4012/$4013/$4015` presentation
 * tail remains outside this CPU/AI helper. Rejection for malformed, aliased,
 * stale, or outside-source-domain input is byte-identical for all outputs. */
bool tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
    TecmoGameplayCpuGlobalLatch *latch,
    const TecmoGameplayCpuA9daInput *input,
    TecmoGameplayCpuA9daInput *output,
    TecmoGameplayCpuA9daResult *result_out);

bool tecmo_gameplay_cpu_a9da_target_assignment_subset_self_test(
    char *message,
    size_t message_size);

#endif
