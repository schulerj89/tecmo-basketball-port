#include "tecmo_gameplay_cpu_opcode15_selection.h"

#include <stdio.h>
#include <string.h>

static bool actor_valid(uint8_t actor)
{
    return actor < TECMO_GAMEPLAY_CPU_OPCODE15_ACTOR_COUNT;
}

static bool latch_valid(const TecmoGameplayCpuOpcode15SelectionLatch *latch)
{
    return latch != NULL &&
           latch->contract_tag == TECMO_GAMEPLAY_CPU_OPCODE15_SELECTION_TAG &&
           ((!latch->valid && latch->actor_059e == 0U && !latch->stale) ||
            (latch->valid && actor_valid(latch->actor_059e) &&
             latch->write_serial != 0U));
}

bool tecmo_gameplay_cpu_opcode15_selection_init(
    TecmoGameplayCpuOpcode15SelectionLatch *latch)
{
    static const TecmoGameplayCpuOpcode15SelectionLatch virgin = {0};
    TecmoGameplayCpuOpcode15SelectionLatch candidate;
    if (latch == NULL || memcmp(latch, &virgin, sizeof(*latch)) != 0) {
        return false;
    }
    candidate = virgin;
    candidate.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE15_SELECTION_TAG;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_opcode15_selection_write_920d(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    uint32_t expected_serial,
    uint8_t actor)
{
    TecmoGameplayCpuOpcode15SelectionLatch candidate;
    if (!latch_valid(latch) || expected_serial != latch->write_serial ||
        latch->write_serial == UINT32_MAX || !actor_valid(actor)) return false;
    candidate = *latch;
    candidate.write_serial++;
    candidate.actor_059e = actor;
    candidate.valid = true;
    candidate.stale = false;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_opcode15_selection_full_reset(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    uint32_t expected_serial)
{
    TecmoGameplayCpuOpcode15SelectionLatch candidate;
    if (!latch_valid(latch) || expected_serial != latch->write_serial ||
        latch->write_serial == UINT32_MAX) return false;
    candidate = *latch;
    candidate.write_serial++;
    candidate.actor_059e = 0U;
    candidate.valid = false;
    candidate.stale = false;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_opcode15_selection_retain_period(
    const TecmoGameplayCpuOpcode15SelectionLatch *latch)
{
    return latch_valid(latch);
}

bool tecmo_gameplay_cpu_opcode15_selection_retain_possession(
    const TecmoGameplayCpuOpcode15SelectionLatch *latch)
{
    return latch_valid(latch);
}

bool tecmo_gameplay_cpu_opcode15_state7_consume(
    TecmoGameplayCpuOpcode15SelectionLatch *latch,
    const TecmoGameplayCpuOpcode15State7Input *input,
    TecmoGameplayCpuOpcode15State7Input *output,
    TecmoGameplayCpuOpcode15State7Result *result_out)
{
    TecmoGameplayCpuOpcode15SelectionLatch latch_candidate;
    TecmoGameplayCpuOpcode15State7Input output_candidate;
    TecmoGameplayCpuOpcode15State7Result result;
    uint8_t actor;
    if (latch == NULL || input == NULL || output == NULL || result_out == NULL ||
        (const void *)latch == (const void *)input ||
        (const void *)latch == (const void *)output ||
        (const void *)latch == (const void *)result_out ||
        (const void *)input == (const void *)output ||
        (const void *)input == (const void *)result_out ||
        (const void *)output == (const void *)result_out) return false;
    if (!latch_valid(latch) ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_INPUT_TAG ||
        input->expected_write_serial != latch->write_serial ||
        !actor_valid(input->dispatch_actor) ||
        input->actor_state_057c[input->dispatch_actor] != 7U ||
        !actor_valid(input->primary_0308) || !latch->valid) {
        return false;
    }
    actor = latch->actor_059e;
    latch_candidate = *latch;
    output_candidate = *input;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_RESULT_TAG;
    result.write_serial = latch->write_serial;
    result.dispatch_actor = input->dispatch_actor;
    result.actor_059e = actor;
    result.scratch_00bf_actor = actor;
    result.scratch_00be_side = actor < 5U ? 0U : 1U;
    result.c711_selector = 3U;
    result.consumed_stale_value = latch->stale;
    result.selector3_observed_unexecuted = true;
    if (actor != input->primary_0308 &&
        input->actor_timer_046e[actor] == 0U) {
        output_candidate.actor_state_057c[actor] = 0U;
        result.retired_state7 = true;
    }
    latch_candidate.stale = true;
    *latch = latch_candidate;
    *output = output_candidate;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_opcode15_selection_self_test(char *message,
                                                     size_t message_size)
{
    TecmoGameplayCpuOpcode15SelectionLatch latch;
    TecmoGameplayCpuOpcode15SelectionLatch latch_before;
    TecmoGameplayCpuOpcode15SelectionLatch malformed;
    TecmoGameplayCpuOpcode15State7Input input;
    TecmoGameplayCpuOpcode15State7Input input_before;
    TecmoGameplayCpuOpcode15State7Input output;
    TecmoGameplayCpuOpcode15State7Input output_before;
    TecmoGameplayCpuOpcode15State7Result result;
    TecmoGameplayCpuOpcode15State7Result result_before;
    if (message == NULL || message_size == 0U) return false;
    memset(&latch, 0, sizeof(latch));
    if (!tecmo_gameplay_cpu_opcode15_selection_init(&latch) ||
        !tecmo_gameplay_cpu_opcode15_selection_write_920d(&latch, 0U, 6U)) {
        goto fail;
    }
    latch_before = latch;
    if (tecmo_gameplay_cpu_opcode15_selection_init(&latch) ||
        tecmo_gameplay_cpu_opcode15_selection_write_920d(&latch, 0U, 4U) ||
        tecmo_gameplay_cpu_opcode15_selection_write_920d(&latch, 1U, 10U) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0) goto fail;
    memset(&malformed, 0xA5, sizeof(malformed));
    latch_before = malformed;
    if (tecmo_gameplay_cpu_opcode15_selection_init(&malformed) ||
        memcmp(&malformed, &latch_before, sizeof(malformed)) != 0) goto fail;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_INPUT_TAG;
    input.expected_write_serial = 1U;
    input.dispatch_actor = 1U;
    input.primary_0308 = 4U;
    input.actor_state_057c[1U] = 7U;
    input.actor_state_057c[6U] = 4U;
    if (!tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &input, &output, &result) ||
        result.scratch_00bf_actor != 6U || result.scratch_00be_side != 1U ||
        result.dispatch_actor != 1U ||
        result.c711_selector != 3U || !result.selector3_observed_unexecuted ||
        result.consumed_stale_value || !result.retired_state7 ||
        output.actor_state_057c[6U] != 0U || !latch.stale ||
        output.actor_state_057c[1U] != 7U ||
        latch.actor_059e != 6U || latch.write_serial != 1U) goto fail;

    /* The dispatching actor remains state 7 while stale `$059E` names the
       distinct actor 6, which is already no longer state 7. `$9248` still
       reloads actor 6 and applies selector/retirement to that stored actor. */
    if (!tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &output, &input, &result) ||
        !result.consumed_stale_value || result.dispatch_actor != 1U ||
        result.actor_059e != 6U || result.scratch_00bf_actor != 6U ||
        result.scratch_00be_side != 1U || !result.retired_state7 ||
        input.actor_state_057c[1U] != 7U ||
        input.actor_state_057c[6U] != 0U) goto fail;

    /* A non-retiring consume proves persistence and stale reuse. */
    if (!tecmo_gameplay_cpu_opcode15_selection_write_920d(&latch, 1U, 2U))
        goto fail;
    input.expected_write_serial = 2U;
    input.dispatch_actor = 2U;
    input.primary_0308 = 2U;
    input.actor_state_057c[2U] = 7U;
    if (!tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &input, &output, &result) || result.retired_state7 ||
        result.consumed_stale_value || output.actor_state_057c[2U] != 7U ||
        !tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &output, &input, &result) ||
        !result.consumed_stale_value || result.retired_state7) goto fail;

    /* Every public output is disjoint from the latch. Reject base-address
       aliases before reading the wider typed view or writing any result. */
    latch_before = latch;
    input_before = input;
    output_before = output;
    result_before = result;
    if (tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, (const TecmoGameplayCpuOpcode15State7Input *)(const void *)&latch,
            &output, &result) ||
        tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &output,
            (TecmoGameplayCpuOpcode15State7Input *)(void *)&latch, &result) ||
        tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &output, &input,
            (TecmoGameplayCpuOpcode15State7Result *)(void *)&latch) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&input, &input_before, sizeof(input)) != 0 ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;

    if (!tecmo_gameplay_cpu_opcode15_selection_write_920d(&latch, 2U, 8U))
        goto fail;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE15_STATE7_INPUT_TAG;
    input.expected_write_serial = 3U;
    input.dispatch_actor = 1U;
    input.primary_0308 = 4U;
    input.actor_state_057c[1U] = 7U;
    input.actor_state_057c[8U] = 4U;
    input.actor_timer_046e[8U] = 1U;
    if (!tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &input, &output, &result) || result.retired_state7 ||
        output.actor_state_057c[8U] != 4U) goto fail;

    latch_before = latch;
    output_before = output;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    output.actor_state_057c[1U] = 4U;
    if (tecmo_gameplay_cpu_opcode15_state7_consume(
            &latch, &output, &input, &result) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0 ||
        memcmp(&input, &output_before, sizeof(input)) != 0) goto fail;
    if (!tecmo_gameplay_cpu_opcode15_selection_retain_period(&latch) ||
        !tecmo_gameplay_cpu_opcode15_selection_retain_possession(&latch) ||
        !tecmo_gameplay_cpu_opcode15_selection_full_reset(&latch, 3U) ||
        latch.valid || latch.stale || latch.actor_059e != 0U ||
        latch.write_serial != 4U) goto fail;
    latch.write_serial = UINT32_MAX;
    latch_before = latch;
    if (tecmo_gameplay_cpu_opcode15_selection_write_920d(
            &latch, UINT32_MAX, 1U) ||
        tecmo_gameplay_cpu_opcode15_selection_full_reset(
            &latch, UINT32_MAX) ||
        tecmo_gameplay_cpu_opcode15_selection_init(&latch) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0) goto fail;
    (void)snprintf(message, message_size,
                   "TGO15-1 selection latch: writer=920D consumer=9248-926F "
                   "dispatch-state=7 selector=3 stale=persistent reset=full-only");
    return true;
fail:
    (void)snprintf(message, message_size,
                   "TGO15-1 selection latch transaction failed");
    return false;
}
