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

bool tecmo_gameplay_cpu_opcode10_live_projection(
    uint8_t actor,
    uint8_t primary_actor,
    uint8_t actor_selector_04b0,
    uint8_t dynamic_link_06cb,
    uint8_t orientation_035a,
    const TecmoGameplayCourtCoordinate actor_position[
        TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT],
    TecmoGameplayCpuOpcode10LiveProjection *projection_out)
{
    TecmoGameplayCpuOpcode10LiveProjection projection;
    TecmoGameplayCpuOpcode10WorkspaceInput input;
    TecmoGameplayCpuOpcode10WorkspaceResult workspace;
    size_t index;
    if (!workspace_actor_valid(actor) || !workspace_actor_valid(primary_actor) ||
        !workspace_actor_valid(dynamic_link_06cb) || orientation_035a > 1U ||
        actor_position == NULL || projection_out == NULL) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT;
         ++index) {
        if (!workspace_coordinate_valid(&actor_position[index]) ||
            actor_position[index].y > UINT8_MAX) {
            return false;
        }
    }
    memset(&projection, 0, sizeof(projection));
    projection.contract_tag =
        TECMO_GAMEPLAY_CPU_OPCODE10_LIVE_PROJECTION_TAG;
    projection.linked_actor = 0xFFU;

    /* Bank02 $BF87-$BF8C admits only bit-$10 candidates before $BFD5 can
       write $07DF. Its no-candidate exits write/retain the $FF sentinel.
       A bit-clear actor therefore cannot equal a live $07DF actor. */
    if ((actor_selector_04b0 & 0x10U) != 0U) {
        *projection_out = projection;
        return true;
    }
    projection.branch_context_available = true;
    projection.linked_actor = dynamic_link_06cb;

    /* $8DBC selects the timer-dependent primary scaling branch by comparing
       the resolved link with $0308. LIVE has no exact $0798/$075F/$006A/$0760
       owner, so preserve branch ownership but defer the relative workspace. */
    if (dynamic_link_06cb == primary_actor) {
        *projection_out = projection;
        return true;
    }

    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_OPCODE10_WORKSPACE_INPUT_TAG;
    input.actor_index = actor;
    input.special_actor_07df = 0xFFU;
    input.primary_actor_0308 = primary_actor;
    input.dynamic_link_06cb = dynamic_link_06cb;
    input.orientation_035a = orientation_035a;
    input.linked_target_x = (uint16_t)actor_position[dynamic_link_06cb].x;
    input.linked_target_depth =
        (uint8_t)actor_position[dynamic_link_06cb].y;
    /* These bytes are not read on the proven non-primary $8DDD branch. */
    input.rate_index_075f = 0U;
    if (!tecmo_gameplay_cpu_opcode10_workspace_harness(&input, &workspace) ||
        workspace.linked_actor != dynamic_link_06cb) {
        return false;
    }
    projection.relative_workspace_available = true;
    projection.linked_relative_x = workspace.linked_relative_x;
    projection.linked_relative_depth = workspace.linked_relative_depth;
    *projection_out = projection;
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
       $0370/$0371 is abs($F3-$94). This is arithmetic proof only; the Bank05
       caller cadence remains unowned by LIVE. */
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

static bool workspace_test_opcode10_live_projection(char *message,
                                                     size_t message_size)
{
    TecmoGameplayCourtCoordinate position[
        TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT];
    TecmoGameplayCpuOpcode10LiveProjection projection;
    TecmoGameplayCpuOpcode10LiveProjection before;
    size_t actor;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_OPCODE_WORKSPACE_ACTOR_COUNT;
         ++actor) {
        position[actor].x = (int16_t)(80 + actor * 40);
        position[actor].y = (int16_t)(80 + actor * 4);
    }
    /* A bit-clear ordinary actor cannot be Bank02's $07DF candidate. Its
       non-primary dynamic link therefore owns both the branch and workspace. */
    if (!tecmo_gameplay_cpu_opcode10_live_projection(
            1U, 4U, 0x00U, 7U, 0U, position, &projection) ||
        projection.contract_tag !=
            TECMO_GAMEPLAY_CPU_OPCODE10_LIVE_PROJECTION_TAG ||
        !projection.branch_context_available ||
        !projection.relative_workspace_available ||
        projection.linked_actor != 7U ||
        projection.linked_relative_x != -25 ||
        projection.linked_relative_depth != 5) {
        (void)snprintf(message, message_size,
                       "opcode-10 LIVE dynamic-link projection failed");
        return false;
    }
    /* A bit-$10 actor may be $07DF, so both owners remain unavailable. */
    if (!tecmo_gameplay_cpu_opcode10_live_projection(
            6U, 4U, 0x10U, 1U, 0U, position, &projection) ||
        projection.branch_context_available ||
        projection.relative_workspace_available ||
        projection.linked_actor != 0xFFU) {
        (void)snprintf(message, message_size,
                       "opcode-10 LIVE special-actor defer failed");
        return false;
    }
    /* The bit-clear branch is resolved, but a $0308 link needs unowned timer
       inputs for the $8DC1-$8DDA primary scaling path. */
    if (!tecmo_gameplay_cpu_opcode10_live_projection(
            1U, 4U, 0x00U, 4U, 0U, position, &projection) ||
        !projection.branch_context_available ||
        projection.relative_workspace_available ||
        projection.linked_actor != 4U) {
        (void)snprintf(message, message_size,
                       "opcode-10 LIVE primary-timer defer failed");
        return false;
    }
    before = projection;
    if (tecmo_gameplay_cpu_opcode10_live_projection(
            1U, 4U, 0x00U, 7U, 2U, position, &projection) ||
        memcmp(&before, &projection, sizeof(projection)) != 0) {
        (void)snprintf(message, message_size,
                       "opcode-10 LIVE malformed transaction failed");
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
        !workspace_test_opcode10_live_projection(message, message_size) ||
        !workspace_test_opcode16(message, message_size)) {
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGAI-3 opcode workspace harness: opcode7=defer "
                   "opcode10=exact-harness opcode16=exact-harness "
                   "ba=external-lifecycle");
    return true;
}
