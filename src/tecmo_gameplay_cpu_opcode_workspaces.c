#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_cpu_opcode_workspaces.h"

#include <stdio.h>
#include <string.h>

static bool workspace_coordinate_valid(
    const TecmoGameplayCourtCoordinate *coordinate)
{
    return coordinate != NULL &&
           coordinate->x >= TECMO_GAMEPLAY_COURT_WORLD_MIN_X &&
           coordinate->x <= TECMO_GAMEPLAY_COURT_WORLD_MAX_X &&
           coordinate->y >= TECMO_GAMEPLAY_COURT_WORLD_MIN_Y &&
           coordinate->y <= TECMO_GAMEPLAY_COURT_WORLD_MAX_Y;
}

static bool workspace_actor_valid(uint8_t actor)
{
    return actor < 10U;
}

static uint16_t workspace_abs_u16(int32_t value)
{
    return (uint16_t)(value < 0 ? -value : value);
}

static int16_t workspace_signed_from_magnitude(uint16_t magnitude,
                                                bool negative)
{
    uint16_t value = negative
        ? (uint16_t)(0U - magnitude)
        : magnitude;
    return (int16_t)value;
}

bool tecmo_gameplay_cpu_opcode_workspace_assess(
    uint8_t opcode,
    const TecmoGameplayCpuOpcodeWorkspaceEvidence *evidence,
    TecmoGameplayCpuOpcodeWorkspaceAssessment *assessment_out)
{
    TecmoGameplayCpuOpcodeWorkspaceAssessment assessment;
    if (evidence == NULL || assessment_out == NULL ||
        evidence == (const void *)assessment_out ||
        evidence->contract_tag !=
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_EVIDENCE_TAG ||
        (evidence->observed_mask &
             ~TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_KNOWN_MASK) != 0U) {
        return false;
    }
    memset(&assessment, 0, sizeof(assessment));
    assessment.contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ASSESSMENT_TAG;
    assessment.opcode = opcode;
    switch (opcode) {
    case 7U:
        /* Bank06 $8F12-$8F29 reads $046E[C8].  C8 can be the ball object;
           no actor timer or visible ball coordinate is a valid substitute. */
        assessment.handler_cpu = 0x8F12U;
        assessment.required_mask =
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_046E_PROBE;
        break;
    case 10U:
        /* Bank06 $8CD0-$8CE2 chooses the branch/context, $8D59-$8E21
           creates the relative window, and $8E4F/$92A8 own the continuation.
           Existing scene fixed links do not establish any of those callers. */
        assessment.handler_cpu = 0x8CD0U;
        assessment.required_mask =
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_ENTRY_LINK |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_RELATIVE_WINDOW |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_10_CONTINUATION |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_TARGET_APPLY |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_BA_LOW_BITS;
        break;
    case 16U:
        /* Bank06 $9085-$90D7 dereferences C8/C9, compares the two captured
           distances, then enters $92A8 where BA&3 gates $8FD9. */
        assessment.handler_cpu = 0x9085U;
        assessment.required_mask =
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_POINTER_TARGET |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_16_DISTANCE |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_TARGET_APPLY |
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_BA_LOW_BITS;
        break;
    default:
        break;
    }
    assessment.missing_mask = assessment.required_mask &
        ~evidence->observed_mask;
    assessment.capture_complete = assessment.missing_mask == 0U;
    assessment.deferred = assessment.missing_mask != 0U;
    /* This module owns no LIVE producer/cadence. A complete debugger capture
       is enough for harness proof, never enough to wire a scene tick. */
    assessment.live_producer_available = false;
    *assessment_out = assessment;
    return true;
}

bool tecmo_gameplay_cpu_opcode10_workspace_harness(
    const TecmoGameplayCpuOpcode10WorkspaceInput *input,
    TecmoGameplayCpuOpcode10WorkspaceResult *result_out)
{
    static const uint8_t rate_threshold[3U] = {0U, 1U, 2U};
    static const uint8_t rate_reload[3U] = {0x0AU, 0x1EU, 0x32U};
    TecmoGameplayCpuOpcode10WorkspaceResult result;
    uint8_t linked_actor;
    uint16_t magnitude_x;
    uint16_t magnitude_depth;
    bool negative_x;
    bool negative_depth;
    uint8_t shift_count = 0U;
    if (input == NULL || result_out == NULL ||
        input == (const void *)result_out ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_INPUT_TAG ||
        !workspace_actor_valid(input->actor_index) ||
        input->orientation_035a > 1U || input->rate_index_075f >= 3U) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_RESULT_TAG;
    linked_actor = input->actor_index == input->special_actor_07df
        ? input->primary_actor_0308 : input->dynamic_link_06cb;
    /* Bank06 $8D59 compares X to $07DF; $07DF itself is never dereferenced.
       Validate only the selected $0308/$06CB target before its workspace read. */
    if (!workspace_actor_valid(linked_actor)) return false;
    result.linked_actor = linked_actor;

    /* Bank06 $8D6E-$8D9B uses the exact orientation-indexed anchors at
       $9C97/$9C99 and depth $94. The anchors match TGCT's pinned hoop
       coordinates, but this function only translates captured helper input. */
    {
        uint16_t raw_x = (uint16_t)(
            (input->orientation_035a == 0U
                 ? TECMO_GAMEPLAY_COURT_LEFT_HOOP_X
                 : TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X) -
            input->linked_target_x);
        uint16_t raw_depth = (uint16_t)(
            TECMO_GAMEPLAY_COURT_HOOP_Y - input->linked_target_depth);
        negative_x = (raw_x & 0x8000U) != 0U;
        negative_depth = (raw_depth & 0x8000U) != 0U;
        magnitude_x = negative_x ? (uint16_t)(0U - raw_x) : raw_x;
        magnitude_depth = negative_depth
            ? (uint16_t)(0U - raw_depth) : raw_depth;
    }
    result.timer_0798_after = input->timer_0798;

    if (linked_actor == input->primary_actor_0308) {
        /* Canonical Rev1 $8DCE is BCC $8E03 (raw 90 33), not the lifted
           file's misleading $8DFB label. A zero timer with threshold<$6A
           enters at $8E03: exactly three shifts, no reload or decrement. */
        if (result.timer_0798_after == 0U &&
            rate_threshold[input->rate_index_075f] < input->sample_006a) {
            shift_count = 3U;
        } else {
            if (result.timer_0798_after == 0U) {
                result.timer_0798_after = (uint8_t)(
                    rate_reload[input->rate_index_075f] + input->timer_0760);
                result.timer_reloaded = true;
            }
            result.timer_0798_after = (uint8_t)(
                result.timer_0798_after - 1U);
            result.timer_decremented = true;
            shift_count = 4U;
        }
    } else {
        uint16_t absolute_sum = (uint16_t)(magnitude_x + magnitude_depth);
        /* Canonical branches are $8DE6->$8E03 (three shifts),
           $8DEF->$8E03 (three), $8DF3->$8E0B (two), and
           $8DF5->$8E13 (one). They never decrement $0798. */
        if (absolute_sum >= 0x80U) {
            shift_count = 3U;
        } else if (absolute_sum >= 0x50U) {
            shift_count = 2U;
        } else {
            shift_count = 1U;
        }
    }
    if (shift_count == 0U) {
        return false;
    }
    magnitude_x >>= shift_count;
    magnitude_depth >>= shift_count;
    /* Bank06 $8E22-$8E4E restores the independently recorded X/depth signs.
       It is intentionally part of the harness result because opcode 10 C
       stores its workspace only after both helpers have run. */
    result.linked_relative_x = workspace_signed_from_magnitude(
        magnitude_x, negative_x);
    result.linked_relative_depth = workspace_signed_from_magnitude(
        magnitude_depth, negative_depth);
    result.right_shift_count = shift_count;
    *result_out = result;
    return true;
}

static uint16_t selector_abs_x_delta(
    const TecmoGameplayCourtCoordinate *candidate,
    const TecmoGameplayCourtCoordinate *reference)
{
    uint16_t raw = (uint16_t)((uint16_t)candidate->x -
                              (uint16_t)reference->x);
    return (raw & 0x8000U) != 0U ? (uint16_t)(0U - raw) : raw;
}

static void selector_explicit_ff(
    TecmoGameplayCpuOpcode10SelectorResult *result)
{
    result->special_actor_07df_after = 0xFFU;
    result->explicit_ff_stored = true;
}

bool tecmo_gameplay_cpu_opcode10_selector_b02_harness(
    const TecmoGameplayCpuOpcode10SelectorInput *input,
    TecmoGameplayCpuOpcode10SelectorResult *result_out)
{
    static const uint8_t orientation_sign[2U] = {0x00U, 0x80U};
    TecmoGameplayCpuOpcode10SelectorResult result;
    uint8_t initial_actor = 0xFFU;
    uint16_t window;
    int actor;
    size_t index;
    if (input == NULL || result_out == NULL ||
        input == (const void *)result_out ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_OPCODE10_SELECTOR_INPUT_TAG ||
        !workspace_actor_valid(input->primary_actor_0308) ||
        !workspace_actor_valid(input->defender_actor_0309) ||
        input->orientation_035a > 1U ||
        (input->prior_special_actor_07df != 0xFFU &&
         !workspace_actor_valid(input->prior_special_actor_07df))) {
        return false;
    }
    for (index = 0U; index < 10U; ++index) {
        if (!workspace_actor_valid(input->dynamic_link_06cb[index]) ||
            !workspace_coordinate_valid(&input->actor_position[index]) ||
            input->actor_position[index].y > UINT8_MAX) {
            return false;
        }
    }

    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE10_SELECTOR_RESULT_TAG;
    result.prior_special_actor_07df = input->prior_special_actor_07df;
    result.special_actor_07df_after = input->prior_special_actor_07df;
    result.initial_linked_actor = 0xFFU;
    /* $BEE7-$BEE9 seeds $99 to $FF on every invocation. */
    result.candidate_actor_99 = 0xFFU;

    /* $BEEB-$BF02: the two early failure paths explicitly store $FF. */
    if ((input->flags_0588 & 0x10U) == 0U) {
        selector_explicit_ff(&result);
        *result_out = result;
        return true;
    }
    result.gate_0588_passed = true;
    if (input->context_0478 != 0U && (input->flags_ba & 0x40U) == 0U) {
        selector_explicit_ff(&result);
        *result_out = result;
        return true;
    }
    result.context_gate_passed = true;

    /* $BF03-$BF17: descending first match on bit-$10 and $06CB==$0308. */
    for (actor = 9; actor >= 0; --actor) {
        if ((input->actor_selector_04b0[actor] & 0x10U) != 0U &&
            input->dynamic_link_06cb[actor] == input->primary_actor_0308) {
            initial_actor = (uint8_t)actor;
            break;
        }
    }
    if (initial_actor == 0xFFU) {
        selector_explicit_ff(&result);
        *result_out = result;
        return true;
    }
    result.initial_link_found = true;
    result.initial_linked_actor = initial_actor;

    /* $BF1A-$BF77: abs(16-bit X delta)+abs(8-bit depth delta). The branch
       compares only the wrapped low byte of $9495 with threshold $97. */
    {
        uint16_t raw_x = (uint16_t)(
            (uint16_t)input->actor_position[initial_actor].x -
            (uint16_t)input->actor_position[input->primary_actor_0308].x);
        uint8_t raw_depth = (uint8_t)(
            (uint8_t)input->actor_position[initial_actor].y -
            (uint8_t)input->actor_position[input->primary_actor_0308].y);
        uint16_t abs_x = (raw_x & 0x8000U) != 0U
            ? (uint16_t)(0U - raw_x) : raw_x;
        uint8_t abs_depth = (raw_depth & 0x80U) != 0U
            ? (uint8_t)(0U - raw_depth) : raw_depth;
        result.threshold_97 =
            ((uint8_t)(raw_x >> 8U) & 0x80U) ==
                    orientation_sign[input->orientation_035a]
                ? 0x08U : 0x1CU;
        window = (uint16_t)(abs_x + abs_depth);
    }
    result.initial_window_9495 = window;
    result.winning_distance_9495 = window;
    if ((uint8_t)window < result.threshold_97) {
        selector_explicit_ff(&result);
        *result_out = result;
        return true;
    }
    result.window_passed = true;

    /* $BF79-$BFD0: descending scan, excluding $0309 and initial $98.
       Candidate equality updates because the source rejects only borrow;
       therefore the lowest slot wins an exact distance tie. */
    for (actor = 9; actor >= 0; --actor) {
        uint16_t distance;
        if ((uint8_t)actor == input->defender_actor_0309 ||
            (uint8_t)actor == initial_actor ||
            (input->actor_selector_04b0[actor] & 0x10U) == 0U) {
            continue;
        }
        distance = selector_abs_x_delta(
            &input->actor_position[actor],
            &input->actor_position[input->primary_actor_0308]);
        if (result.winning_distance_9495 < distance) continue;
        result.winning_distance_9495 = distance;
        result.candidate_actor_99 = (uint8_t)actor;
    }
    if (result.candidate_actor_99 == 0xFFU) {
        /* $BFD1 BMI $BFD8: no $07DF store. */
        result.prior_07df_retained = true;
    } else {
        result.special_actor_07df_after = result.candidate_actor_99;
        result.candidate_stored = true;
    }
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_opcode16_workspace_harness(
    const TecmoGameplayCpuOpcode16WorkspaceInput *input,
    TecmoGameplayCpuOpcode16WorkspaceResult *result_out)
{
    TecmoGameplayCpuOpcode16WorkspaceResult result;
    int32_t hoop_x;
    if (input == NULL || result_out == NULL ||
        input == (const void *)result_out ||
        input->contract_tag !=
            TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_INPUT_TAG ||
        input->orientation_035a > 1U ||
        !workspace_coordinate_valid(&input->actor_position)) {
        return false;
    }
    /* Bank05 $9054-$90AF: $036E/$036F is abs(($73/$E8)-BDEF/BDF1[Y]) and
       $0370/$0371 is abs($F3-$94). This pure function owns no cadence; LIVE
       separately binds the fixed once-per-loop pre-motion scene capture. */
    hoop_x = input->orientation_035a == 0U
        ? TECMO_GAMEPLAY_COURT_LEFT_HOOP_X
        : TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X;
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_RESULT_TAG;
    result.workspace_036e = workspace_abs_u16(
        (int32_t)input->actor_position.x - hoop_x);
    result.workspace_0370 = workspace_abs_u16(
        (int32_t)input->actor_position.y - TECMO_GAMEPLAY_COURT_HOOP_Y);
    *result_out = result;
    return true;
}

uint8_t tecmo_gameplay_cpu_opcode_workspace_ba_low_bits(uint8_t flags_ba)
{
    /* Bank06 $92CA-$92CE reads only BA&3 here. It has no timer increment or
       frame-phase operation, so this accessor deliberately has no cadence. */
    return (uint8_t)(flags_ba & 0x03U);
}

static bool workspace_test_assessment(char *message, size_t message_size)
{
    TecmoGameplayCpuOpcodeWorkspaceEvidence evidence;
    TecmoGameplayCpuOpcodeWorkspaceAssessment assessment;
    TecmoGameplayCpuOpcodeWorkspaceAssessment before;
    memset(&evidence, 0, sizeof(evidence));
    evidence.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_EVIDENCE_TAG;
    if (!tecmo_gameplay_cpu_opcode_workspace_assess(
            7U, &evidence, &assessment) ||
        assessment.handler_cpu != 0x8F12U ||
        assessment.required_mask !=
            TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_046E_PROBE ||
        assessment.missing_mask != assessment.required_mask ||
        assessment.capture_complete || !assessment.deferred ||
        assessment.live_producer_available) {
        (void)snprintf(message, message_size,
                       "opcode-7 $046E defer contract failed");
        return false;
    }
    evidence.observed_mask = TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_KNOWN_MASK;
    if (!tecmo_gameplay_cpu_opcode_workspace_assess(
            10U, &evidence, &assessment) ||
        assessment.handler_cpu != 0x8CD0U || !assessment.capture_complete ||
        assessment.deferred || assessment.live_producer_available ||
        (assessment.required_mask &
             TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_BA_LOW_BITS) == 0U ||
        !tecmo_gameplay_cpu_opcode_workspace_assess(
            16U, &evidence, &assessment) ||
        assessment.handler_cpu != 0x9085U || !assessment.capture_complete ||
        assessment.live_producer_available) {
        (void)snprintf(message, message_size,
                       "opcode-10/16 workspace assessment failed");
        return false;
    }
    before = assessment;
    evidence.observed_mask = 0x80000000U;
    if (tecmo_gameplay_cpu_opcode_workspace_assess(
            16U, &evidence, &assessment) ||
        memcmp(&assessment, &before, sizeof(assessment)) != 0) {
        (void)snprintf(message, message_size,
                       "workspace assessment malformed transaction failed");
        return false;
    }
    return true;
}

static bool workspace_test_opcode10(char *message, size_t message_size)
{
    TecmoGameplayCpuOpcode10WorkspaceInput input;
    TecmoGameplayCpuOpcode10WorkspaceResult result;
    TecmoGameplayCpuOpcode10WorkspaceResult before;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_INPUT_TAG;
    input.actor_index = 2U;
    input.special_actor_07df = 2U;
    input.primary_actor_0308 = 4U;
    /* This unselected $06CB value is intentionally invalid: source does not
       dereference it while X==$07DF, so the strict harness must accept it. */
    input.dynamic_link_06cb = 0xFFU;
    input.orientation_035a = 0U;
    input.linked_target_x = TECMO_GAMEPLAY_COURT_LEFT_HOOP_X;
    input.linked_target_depth = TECMO_GAMEPLAY_COURT_HOOP_Y;
    input.rate_index_075f = 1U;
    input.sample_006a = 0U;
    input.timer_0760 = 5U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_actor != 4U || result.linked_relative_x != 0 ||
        result.linked_relative_depth != 0 || result.right_shift_count != 4U ||
        !result.timer_reloaded || !result.timer_decremented ||
        result.timer_0798_after != 0x22U) {
        (void)snprintf(message, message_size,
                       "opcode-10 primary reload/shift failed");
        return false;
    }
    /* Canonical Rev1 $8DCE is raw BCC $8E03 (90 33).  With X==$07DF,
       selected Y==$0308, a zero timer, and table[$075F]<$006A, execution
       enters the second shift pair: three right shifts, no $0798 reload or
       decrement. The signed output proves $8E22-$8E4E restoration too. */
    input.actor_index = 2U;
    input.special_actor_07df = 2U;
    input.primary_actor_0308 = 4U;
    input.dynamic_link_06cb = 0xFFU;
    input.orientation_035a = 0U;
    input.linked_target_x = 0x0120U;
    input.linked_target_depth = TECMO_GAMEPLAY_COURT_HOOP_Y;
    input.timer_0798 = 0U;
    input.rate_index_075f = 1U;
    input.sample_006a = 2U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_actor != 4U || result.linked_relative_x != -16 ||
        result.linked_relative_depth != 0 || result.right_shift_count != 3U ||
        result.timer_0798_after != 0U || result.timer_reloaded ||
        result.timer_decremented) {
        (void)snprintf(message, message_size,
                       "opcode-10 primary $8DCE/$8E03 shift failed");
        return false;
    }
    input.actor_index = 1U;
    input.dynamic_link_06cb = 7U;
    input.orientation_035a = 0U;
    input.linked_target_x = 0x0268U;
    input.linked_target_depth = 0x9AU;
    input.timer_0798 = 9U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_actor != 7U || result.linked_relative_x != -57 ||
        result.linked_relative_depth != 0 || result.right_shift_count != 3U ||
        result.timer_0798_after != 9U || result.timer_reloaded ||
        result.timer_decremented) {
        (void)snprintf(message, message_size,
                       "opcode-10 large relative branch failed");
        return false;
    }
    input.linked_target_x = 0x00F0U;
    input.linked_target_depth = TECMO_GAMEPLAY_COURT_HOOP_Y;
    input.timer_0798 = 0U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_relative_x != -20 || result.linked_relative_depth != 0 ||
        result.right_shift_count != 2U || result.timer_0798_after != 0U ||
        result.timer_decremented) {
        (void)snprintf(message, message_size,
                       "opcode-10 $50 boundary branch failed");
        return false;
    }
    input.linked_target_x = 0x00CFU;
    input.timer_0798 = 7U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_relative_x != -23 || result.right_shift_count != 1U ||
        result.timer_0798_after != 7U) {
        (void)snprintf(message, message_size,
                       "opcode-10 unscaled branch failed");
        return false;
    }
    input.actor_index = 2U;
    input.special_actor_07df = 0xFFU;
    input.dynamic_link_06cb = 6U;
    input.linked_target_x = 0x0120U;
    input.linked_target_depth = TECMO_GAMEPLAY_COURT_HOOP_Y;
    input.timer_0798 = 0U;
    input.rate_index_075f = 1U;
    input.sample_006a = 2U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        result.linked_actor != 6U || result.linked_relative_x != -16 ||
        result.right_shift_count != 3U || result.timer_0798_after != 0U ||
        result.timer_reloaded || result.timer_decremented) {
        (void)snprintf(message, message_size,
                       "opcode-10 sentinel/sample branch failed");
        return false;
    }
    before = result;
    input.orientation_035a = 2U;
    if (tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &result) ||
        memcmp(&result, &before, sizeof(result)) != 0) {
        (void)snprintf(message, message_size,
                       "opcode-10 malformed transaction failed");
        return false;
    }
    return true;
}

static void workspace_selector_test_input_init(
    TecmoGameplayCpuOpcode10SelectorInput *input)
{
    size_t actor;
    memset(input, 0, sizeof(*input));
    input->contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE10_SELECTOR_INPUT_TAG;
    input->flags_0588 = 0x10U;
    input->primary_actor_0308 = 0U;
    input->defender_actor_0309 = 8U;
    input->prior_special_actor_07df = 3U;
    for (actor = 0U; actor < 10U; ++actor) {
        input->dynamic_link_06cb[actor] = 1U;
        input->actor_position[actor].x =
            (int16_t)(100U + actor * 10U);
        input->actor_position[actor].y = 100;
    }
}

static bool workspace_test_opcode10_selector(char *message,
                                              size_t message_size)
{
    TecmoGameplayCpuOpcode10SelectorInput input;
    TecmoGameplayCpuOpcode10SelectorResult result;
    TecmoGameplayCpuOpcode10SelectorResult before;

    /* $BEEB-$BEFD: a failed $0588 gate explicitly stores $FF. */
    workspace_selector_test_input_init(&input);
    input.flags_0588 = 0U;
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        result.special_actor_07df_after != 0xFFU ||
        !result.explicit_ff_stored || result.gate_0588_passed ||
        result.candidate_stored || result.prior_07df_retained) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector explicit-$FF gate failed");
        return false;
    }

    /* $BEF2-$BEFD: nonzero $0478 requires BA bit $40. */
    workspace_selector_test_input_init(&input);
    input.context_0478 = 1U;
    input.flags_ba = 0U;
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        !result.gate_0588_passed || result.context_gate_passed ||
        !result.explicit_ff_stored ||
        result.special_actor_07df_after != 0xFFU) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector $0478/BA gate failed");
        return false;
    }

    /* Initial actor 9 establishes a 40-unit window. Candidate 8 is excluded
       as $0309; candidates 7 and 6 tie at 20, and descending equality makes
       the lower actor slot 6 the final $99/$07DF store. */
    workspace_selector_test_input_init(&input);
    input.actor_selector_04b0[9U] = 0x10U;
    input.dynamic_link_06cb[9U] = 0U;
    input.actor_position[9U].x = 140;
    input.actor_selector_04b0[8U] = 0x10U;
    input.actor_position[8U].x = 101;
    input.actor_selector_04b0[7U] = 0x10U;
    input.actor_position[7U].x = 120;
    input.actor_selector_04b0[6U] = 0x10U;
    input.actor_position[6U].x = 120;
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        !result.gate_0588_passed || !result.context_gate_passed ||
        !result.initial_link_found || !result.window_passed ||
        result.initial_linked_actor != 9U || result.threshold_97 != 8U ||
        result.initial_window_9495 != 40U ||
        result.winning_distance_9495 != 20U ||
        result.candidate_actor_99 != 6U ||
        result.special_actor_07df_after != 6U ||
        !result.candidate_stored || result.explicit_ff_stored ||
        result.prior_07df_retained) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector candidate/tie scan failed");
        return false;
    }

    /* $BFD1-$BFD8: after a valid initial scan/window, no final candidate
       leaves $99 negative and performs no $07DF store. */
    workspace_selector_test_input_init(&input);
    input.actor_selector_04b0[9U] = 0x10U;
    input.dynamic_link_06cb[9U] = 0U;
    input.actor_position[9U].x = 140;
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        result.candidate_actor_99 != 0xFFU ||
        result.special_actor_07df_after != 3U ||
        !result.initial_link_found || !result.window_passed ||
        !result.prior_07df_retained || result.candidate_stored ||
        result.explicit_ff_stored) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector retained no-store failed");
        return false;
    }

    /* The initial scan is descending too: actor 9 wins even when actor 5
       also links to $0308. A sub-threshold window explicitly stores $FF. */
    workspace_selector_test_input_init(&input);
    input.orientation_035a = 1U;
    input.actor_selector_04b0[9U] = 0x10U;
    input.dynamic_link_06cb[9U] = 0U;
    input.actor_position[9U].x = 105;
    input.actor_selector_04b0[5U] = 0x10U;
    input.dynamic_link_06cb[5U] = 0U;
    input.actor_position[5U].x = 140;
    if (!tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        result.initial_linked_actor != 9U || result.threshold_97 != 0x1CU ||
        result.initial_window_9495 != 5U || result.window_passed ||
        !result.explicit_ff_stored ||
        result.special_actor_07df_after != 0xFFU) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector initial/window scan failed");
        return false;
    }

    /* Actor-index and coordinate validation is transactional. */
    before = result;
    input.dynamic_link_06cb[4U] = 0xFFU;
    if (tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        memcmp(&result, &before, sizeof(result)) != 0) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector malformed transaction failed");
        return false;
    }
    input.dynamic_link_06cb[4U] = 1U;
    input.actor_position[4U].x = TECMO_GAMEPLAY_COURT_WORLD_MAX_X + 1;
    if (tecmo_gameplay_cpu_opcode10_selector_b02_harness(&input, &result) ||
        memcmp(&result, &before, sizeof(result)) != 0) {
        (void)snprintf(message, message_size,
                       "opcode-10 selector coordinate transaction failed");
        return false;
    }
    return true;
}

static bool workspace_test_opcode16(char *message, size_t message_size)
{
    TecmoGameplayCpuOpcode16WorkspaceInput input;
    TecmoGameplayCpuOpcode16WorkspaceResult result;
    TecmoGameplayCpuOpcode16WorkspaceResult before;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE16_WORKSPACE_INPUT_TAG;
    input.actor_position.x = 0;
    input.actor_position.y = 0;
    if (!tecmo_gameplay_cpu_opcode16_workspace_harness(&input, &result) ||
        result.workspace_036e != 0x00A0U || result.workspace_0370 != 0x0094U) {
        (void)snprintf(message, message_size,
                       "opcode-16 left-hoop workspace failed");
        return false;
    }
    input.orientation_035a = 1U;
    input.actor_position.x = 0x02A0;
    input.actor_position.y = 0x00A0;
    if (!tecmo_gameplay_cpu_opcode16_workspace_harness(&input, &result) ||
        result.workspace_036e != 0x0040U || result.workspace_0370 != 0x000CU) {
        (void)snprintf(message, message_size,
                       "opcode-16 right-hoop workspace failed");
        return false;
    }
    before = result;
    input.actor_position.x = 768;
    if (tecmo_gameplay_cpu_opcode16_workspace_harness(&input, &result) ||
        memcmp(&result, &before, sizeof(result)) != 0) {
        (void)snprintf(message, message_size,
                       "opcode-16 malformed transaction failed");
        return false;
    }
    if (tecmo_gameplay_cpu_opcode_workspace_ba_low_bits(0x80U) != 0U ||
        tecmo_gameplay_cpu_opcode_workspace_ba_low_bits(0x83U) != 3U ||
        tecmo_gameplay_cpu_opcode_workspace_ba_low_bits(0x56U) != 2U) {
        (void)snprintf(message, message_size,
                       "BA low-bit gate failed");
        return false;
    }
    return true;
}

bool tecmo_gameplay_cpu_opcode_workspace_self_test(char *message,
                                                   size_t message_size)
{
    if (message == NULL || message_size == 0U) return false;
    message[0] = '\0';
    if (!workspace_test_assessment(message, message_size) ||
        !workspace_test_opcode10(message, message_size) ||
        !workspace_test_opcode10_selector(message, message_size) ||
        !workspace_test_opcode16(message, message_size)) {
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGAI-3 opcode workspace harness: opcode7=defer "
                   "opcode10=exact-harness opcode16=exact-harness "
                   "ba=external-lifecycle");
    return true;
}
