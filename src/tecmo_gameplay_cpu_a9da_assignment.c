#include "tecmo_gameplay_cpu_a9da_assignment.h"

#include <stdio.h>
#include <string.h>

static bool actor_valid(uint8_t actor)
{
    return actor < TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT;
}

static int32_t asr6(int32_t value)
{
    if (value >= 0) return value / 64;
    return -(((-value) + 63) / 64);
}

static uint16_t abs_delta16(uint16_t left, uint16_t right)
{
    uint16_t delta = (uint16_t)(left - right);
    if (left >= right) return delta;
    return (uint16_t)(0U - delta);
}

static uint16_t metric_max_plus_half_min(uint16_t first, uint16_t second)
{
    uint16_t larger = first >= second ? first : second;
    uint16_t smaller = first >= second ? second : first;
    return (uint16_t)(larger + (smaller >> 1U));
}

static bool input_valid(const TecmoGameplayCpuA9daInput *input)
{
    size_t actor;
    if (input == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_A9DA_INPUT_TAG ||
        input->multiplier_002c != 0x002CU ||
        input->orientation_035a > 1U || !actor_valid(input->primary_0308) ||
        !actor_valid(input->defender_0309) ||
        input->primary_0308 == input->defender_0309) return false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT; ++actor) {
        uint8_t expected = (uint8_t)(
            (actor + 5U) % TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT);
        if (!actor_valid(input->fixed_link_06cb[actor]) ||
            input->fixed_link_06cb[actor] != expected) return false;
    }
    return true;
}

bool tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
    TecmoGameplayCpuGlobalLatch *latch,
    const TecmoGameplayCpuA9daInput *input,
    TecmoGameplayCpuA9daInput *output,
    TecmoGameplayCpuA9daResult *result_out)
{
    TecmoGameplayCpuGlobalLatch latch_candidate;
    TecmoGameplayCpuGlobalLatchWrite write;
    TecmoGameplayCpuA9daInput output_candidate;
    TecmoGameplayCpuA9daResult result;
    int32_t projected_vx;
    int32_t projected_vz;
    uint16_t target_x;
    uint16_t best_metric = 0x0505U;
    uint8_t chosen = 0U;
    bool found = false;
    int actor;
    if (latch == NULL || input == NULL || output == NULL || result_out == NULL ||
        (const void *)latch == (const void *)input ||
        (const void *)latch == (const void *)output ||
        (const void *)latch == (const void *)result_out ||
        (const void *)input == (const void *)output ||
        (const void *)input == (const void *)result_out ||
        (const void *)output == (const void *)result_out ||
        !input_valid(input)) return false;

    /* Validate the `$AAB8` source domain before accepting any mutation. The
       raw routine assumes descending 9..0 finds a metric below `$0505`; it
       otherwise reuses uninitialized scratch `$98`, which this typed seam
       deliberately does not fabricate. Abort gates never enter `$AAB8`. */
    if ((input->ba & 3U) == 0U && input->value_05a1 == 0U) {
        target_x = input->orientation_035a == 0U ? 0x00A0U : 0x0260U;
        for (actor = 9; actor >= 0; --actor) {
            uint16_t metric;
            uint16_t x_delta;
            uint16_t depth_delta;
            if ((uint8_t)actor == input->primary_0308 ||
                (uint8_t)actor == input->defender_0309) continue;
            x_delta = abs_delta16(target_x, input->actor_x[actor]);
            depth_delta = abs_delta16(
                0x0094U, (uint16_t)input->actor_raw_depth_8[actor]);
            metric = metric_max_plus_half_min(x_delta, depth_delta);
            if (metric < best_metric) {
                best_metric = metric;
                chosen = (uint8_t)actor;
                found = true;
            }
        }
        if (!found) return false;
    }

    projected_vx = asr6((int32_t)input->normalized_object10_vx_a9da *
                        (int32_t)input->multiplier_002c);
    projected_vz = asr6((int32_t)input->normalized_object10_vz_a9da *
                        (int32_t)input->multiplier_002c);
    memset(&write, 0, sizeof(write));
    write.contract_tag = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_WRITE_TAG;
    write.expected_serial = input->expected_latch_serial;
    write.raw_x_038d_038e = (uint16_t)(input->ball_x +
                                      (uint16_t)projected_vx);
    write.raw_depth_038f_0390 = (uint16_t)(input->ball_raw_depth_8 +
                                          (uint16_t)projected_vz);
    write.producer = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA;
    latch_candidate = *latch;
    if (!tecmo_gameplay_cpu_global_latch_write(&latch_candidate, &write)) {
        return false;
    }

    output_candidate = *input;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_A9DA_RESULT_TAG;
    result.latch_serial = latch_candidate.write_serial;
    result.projected_x_038d_038e = write.raw_x_038d_038e;
    result.projected_depth_038f_0390 = write.raw_depth_038f_0390;
    result.latch_overwritten = true;
    if ((input->ba & 3U) != 0U) {
        result.outcome = TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_BA;
        *latch = latch_candidate;
        *output = output_candidate;
        *result_out = result;
        return true;
    }

    output_candidate.global_0588 = (uint8_t)(input->global_0588 | 0x80U);
    result.flag_0588_set = true;
    if (input->value_05a1 != 0U) {
        result.outcome = TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_05A1;
        *latch = latch_candidate;
        *output = output_candidate;
        *result_out = result;
        return true;
    }

    result.winning_metric = best_metric;
    result.chosen_actor_002d = chosen;
    result.linked_actor = input->fixed_link_06cb[chosen];
    result.same_loop_first_002d = true;
    output_candidate.stream_offset[chosen] = 0x002DU;
    output_candidate.last_step_offset[chosen] = 0x002DU;
    output_candidate.state[chosen] = 4U;
    output_candidate.action_state_046e[chosen] = 0U;
    if (result.linked_actor == input->primary_0308 ||
        result.linked_actor == input->defender_0309) {
        result.linked_actor_exempt = true;
    } else {
        output_candidate.stream_offset[result.linked_actor] = 0x005AU;
        output_candidate.last_step_offset[result.linked_actor] = 0x005AU;
        output_candidate.state[result.linked_actor] = 4U;
    }
    output_candidate.global_0587 = 3U;
    result.outcome = TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ASSIGNED;
    *latch = latch_candidate;
    *output = output_candidate;
    *result_out = result;
    return true;
}

static void fixture(TecmoGameplayCpuA9daInput *input)
{
    size_t actor;
    memset(input, 0, sizeof(*input));
    input->contract_tag = TECMO_GAMEPLAY_CPU_A9DA_INPUT_TAG;
    /* Natural no-write pass 1 at `$A9DA`: normalized 004B/FFFB from ball
       009D/93 yields the observed latch 00D0/008F. */
    input->ball_x = 0x009DU;
    input->ball_raw_depth_8 = 0x93U;
    input->normalized_object10_vx_a9da = 0x004B;
    input->normalized_object10_vz_a9da = -5;
    input->multiplier_002c = 0x002CU;
    input->primary_0308 = 0U;
    input->defender_0309 = 5U;
    input->global_0587 = 0xA7U;
    input->global_0588 = 0x12U;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT; ++actor) {
        input->actor_x[actor] = (uint16_t)(0x0100U + actor * 0x10U);
        input->actor_raw_depth_8[actor] = 0x94U;
        input->fixed_link_06cb[actor] =
            (uint8_t)((actor + 5U) % TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT);
        input->stream_offset[actor] = (uint16_t)(0x1000U + actor);
        input->last_step_offset[actor] = (uint16_t)(0x2000U + actor);
        input->state[actor] = (uint8_t)(0x20U + actor);
        input->action_state_046e[actor] = (uint8_t)(0x30U + actor);
        input->action_0458[actor] = (uint8_t)(0x40U + actor);
    }
}

bool tecmo_gameplay_cpu_a9da_target_assignment_subset_self_test(
    char *message,
    size_t message_size)
{
    TecmoGameplayCpuGlobalLatch latch;
    TecmoGameplayCpuGlobalLatch latch_before;
    TecmoGameplayCpuA9daInput input;
    TecmoGameplayCpuA9daInput input_before;
    TecmoGameplayCpuA9daInput output;
    TecmoGameplayCpuA9daInput output_before;
    TecmoGameplayCpuA9daResult result;
    TecmoGameplayCpuA9daResult result_before;
    if (message == NULL || message_size == 0U) return false;
    memset(&latch, 0, sizeof(latch));
    if (!tecmo_gameplay_cpu_global_latch_init(&latch)) goto fail;
    fixture(&input);
    input.actor_x[9U] = 0x00A0U;
    input.actor_x[8U] = 0x00A0U;
    input.actor_raw_depth_8[9U] = 0x84U;
    input.actor_raw_depth_8[8U] = 0x84U;
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.outcome != TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ASSIGNED ||
        result.chosen_actor_002d != 9U || !result.same_loop_first_002d ||
        result.winning_metric != 0x0010U ||
        result.projected_x_038d_038e != 0x00D0U ||
        result.projected_depth_038f_0390 != 0x008FU ||
        output.stream_offset[9U] != 0x002DU ||
        output.last_step_offset[9U] != 0x002DU ||
        output.state[9U] != 4U || output.action_state_046e[9U] != 0U ||
        output.action_0458[9U] != input.action_0458[9U] ||
        output.stream_offset[4U] != 0x005AU ||
        output.last_step_offset[4U] != 0x005AU ||
        output.state[4U] != 4U ||
        output.action_state_046e[4U] != input.action_state_046e[4U] ||
        output.global_0587 != 3U || output.global_0588 != 0x92U ||
        latch.producer != TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA ||
        latch.write_serial != 1U) goto fail;

    /* Orientation 1, strict ties, and primary/defender exclusion. */
    fixture(&input);
    input.expected_latch_serial = 1U;
    input.orientation_035a = 1U;
    /* Natural no-write pass 2 normalized 003E/FFF0 and yielded C7/88. */
    input.normalized_object10_vx_a9da = 0x003E;
    input.normalized_object10_vz_a9da = -16;
    input.actor_x[9U] = 0x0260U;
    input.actor_x[8U] = 0x0260U;
    input.primary_0308 = 9U;
    input.defender_0309 = 8U;
    input.actor_x[7U] = 0x0260U;
    input.actor_x[6U] = 0x0260U;
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.chosen_actor_002d != 7U || result.linked_actor != 2U ||
        result.projected_x_038d_038e != 0x00C7U ||
        result.projected_depth_038f_0390 != 0x0088U) goto fail;

    /* Both post-A9DA aborts retain the admitted latch overwrite. Only the
       later `$05A1` gate observes the preceding `$0588|=$80` store. */
    fixture(&input);
    input.expected_latch_serial = 2U;
    input.ba = 1U;
    input.ball_x = 0xFFF0U;
    input.ball_raw_depth_8 = 0x08U;
    input.normalized_object10_vx_a9da = 64;
    input.normalized_object10_vz_a9da = -64;
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.outcome != TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_BA ||
        result.projected_x_038d_038e != 0x001CU ||
        result.projected_depth_038f_0390 != 0xFFDCU ||
        output.global_0588 != input.global_0588 || latch.write_serial != 3U)
        goto fail;
    fixture(&input);
    input.expected_latch_serial = 3U;
    input.value_05a1 = 1U;
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.outcome != TECMO_GAMEPLAY_CPU_A9DA_OUTCOME_ABORT_05A1 ||
        output.global_0588 != 0x92U || latch.write_serial != 4U) goto fail;

    /* Linked selected roles are exempt; every untouched plane, including
       action and the linked timer, is preserved. */
    fixture(&input);
    input.expected_latch_serial = 4U;
    input.actor_x[4U] = 0x00A0U;
    input.primary_0308 = 9U;
    if (!tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        result.chosen_actor_002d != 4U || !result.linked_actor_exempt ||
        output.stream_offset[9U] != input.stream_offset[9U] ||
        output.state[9U] != input.state[9U]) goto fail;

    /* Source-domain no-winner, stale serial, malformed fixed multiplier/link,
       overflow, and every public alias reject without changing caller bytes. */
    latch_before = latch;
    output_before = output;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    {
        size_t actor;
        for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_A9DA_ACTOR_COUNT; ++actor) {
            input.actor_x[actor] = 0x8000U;
            input.actor_raw_depth_8[actor] = 0U;
        }
    }
    input.expected_latch_serial = 5U;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;
    fixture(&input);
    input.expected_latch_serial = 4U;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;
    input.expected_latch_serial = 5U;
    input.multiplier_002c = 0x002DU;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result)) goto fail;
    input.multiplier_002c = 0x002CU;
    input.fixed_link_06cb[3U] = 3U;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result)) goto fail;
    input.fixed_link_06cb[3U] = 8U;
    latch_before = latch;
    input_before = input;
    output_before = output;
    result_before = result;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, (const TecmoGameplayCpuA9daInput *)(const void *)&latch,
            &output, &result) ||
        tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, (TecmoGameplayCpuA9daInput *)(void *)&latch,
            &result) ||
        tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output,
            (TecmoGameplayCpuA9daResult *)(void *)&latch) ||
        tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &input, &result) ||
        tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output,
            (TecmoGameplayCpuA9daResult *)(void *)&input) ||
        tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output,
            (TecmoGameplayCpuA9daResult *)(void *)&output) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&input, &input_before, sizeof(input)) != 0 ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;
    latch.write_serial = UINT32_MAX;
    input.expected_latch_serial = UINT32_MAX;
    latch_before = latch;
    if (tecmo_gameplay_cpu_a9da_target_assignment_subset_apply(
            &latch, &input, &output, &result) ||
        memcmp(&latch, &latch_before, sizeof(latch)) != 0 ||
        memcmp(&output, &output_before, sizeof(output)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;

    (void)snprintf(message, message_size,
                   "TGA9-1 helper: projection=asr6 latch=A9DA "
                   "selector=AAB8 assignment=A993 live=unbound");
    return true;
fail:
    (void)snprintf(message, message_size,
                   "TGA9-1 A9DA/AAB8/A993 transaction failed");
    return false;
}
