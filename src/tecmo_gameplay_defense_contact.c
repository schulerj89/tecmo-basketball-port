#include "tecmo_gameplay_defense_contact.h"

#include <limits.h>
#include <string.h>

static bool span_is_valid(const void *bytes, size_t count)
{
    uintptr_t start;

    if (bytes == NULL || count == 0U) return false;
    start = (uintptr_t)bytes;
    return (uintptr_t)count <= UINTPTR_MAX - start;
}

static bool regions_overlap(const void *first,
                            size_t first_size,
                            const void *second,
                            size_t second_size)
{
    uintptr_t first_start;
    uintptr_t second_start;
    uintptr_t first_end;
    uintptr_t second_end;

    if (first == NULL || second == NULL || first_size == 0U ||
        second_size == 0U) {
        return false;
    }
    first_start = (uintptr_t)first;
    second_start = (uintptr_t)second;
    if ((uintptr_t)first_size > UINTPTR_MAX - first_start ||
        (uintptr_t)second_size > UINTPTR_MAX - second_start) {
        return true;
    }
    first_end = first_start + (uintptr_t)first_size;
    second_end = second_start + (uintptr_t)second_size;
    return first_start < second_end && second_start < first_end;
}

static uint16_t raw_x_wrapped_absolute(uint16_t reference,
                                       uint16_t entry)
{
    uint16_t wrapped;

    wrapped = (uint16_t)((uint32_t)reference - (uint32_t)entry);
    if (reference < entry) {
        wrapped = (uint16_t)(0U - (uint32_t)wrapped);
    }
    return wrapped;
}

static uint8_t raw_depth_absolute(uint8_t reference, uint8_t entry)
{
    if (reference >= entry) {
        return (uint8_t)(reference - entry);
    }
    return (uint8_t)(entry - reference);
}

static uint16_t b06_weighted_metric(uint16_t raw_x_delta,
                                    uint8_t raw_depth_delta)
{
    uint16_t maximum;
    uint16_t minimum;

    if (raw_x_delta >= (uint16_t)raw_depth_delta) {
        maximum = raw_x_delta;
        minimum = (uint16_t)raw_depth_delta;
    } else {
        maximum = (uint16_t)raw_depth_delta;
        minimum = raw_x_delta;
    }
    return (uint16_t)((uint32_t)maximum +
                      ((uint32_t)minimum >> 1U));
}

static void b06_metric_from_coordinates(uint16_t raw_x_reference,
                                         uint16_t raw_x_entry,
                                         uint8_t raw_depth_reference,
                                         uint8_t raw_depth_entry,
                                         uint8_t *raw_0094_out,
                                         uint8_t *raw_0095_out)
{
    uint16_t raw_x_delta;
    uint8_t raw_depth_delta;
    uint16_t metric;

    raw_x_delta = raw_x_wrapped_absolute(raw_x_reference, raw_x_entry);
    raw_depth_delta = raw_depth_absolute(raw_depth_reference,
                                         raw_depth_entry);
    metric = b06_weighted_metric(raw_x_delta, raw_depth_delta);
    *raw_0094_out = (uint8_t)(metric & 0x00FFU);
    *raw_0095_out = (uint8_t)(metric >> 8U);
}

bool tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
    const TecmoGameplayDefenseContactB06MetricInput *input,
    TecmoGameplayDefenseContactB06MetricResult *result_out)
{
    TecmoGameplayDefenseContactB06MetricResult result;

    if (input == NULL || result_out == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_INPUT_TAG ||
        input->routine_cpu !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU ||
        regions_overlap(input, sizeof(*input), result_out,
                        sizeof(*result_out))) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_RESULT_TAG;
    result.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    result.raw_0094 = 0U;
    result.raw_0095 = 0U;
    b06_metric_from_coordinates(
        input->raw_x_reference, input->raw_x_entry,
        input->raw_depth_reference, input->raw_depth_entry,
        &result.raw_0094, &result.raw_0095);
    result.raw_metric = (uint16_t)result.raw_0094 |
                        (uint16_t)((uint16_t)result.raw_0095 << 8U);
    *result_out = result;
    return true;
}

static bool b06_scan_input_valid(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    const TecmoGameplayDefenseContactB06ScanResult *result_out)
{
    if (input == NULL || result_out == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_INPUT_TAG ||
        input->routine_cpu !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU ||
        input->raw_0309 >=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        input->raw_030b >=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        !span_is_valid(input->raw_0073_low,
                       input->raw_0073_low_count) ||
        !span_is_valid(input->raw_00e8_high,
                       input->raw_00e8_high_count) ||
        !span_is_valid(input->raw_00f3_depth,
                       input->raw_00f3_depth_count) ||
        !span_is_valid(input->raw_04b0_by_slot,
                       input->raw_04b0_by_slot_count) ||
        input->raw_0073_low_count !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        input->raw_00e8_high_count !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        input->raw_00f3_depth_count !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        input->raw_04b0_by_slot_count !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT ||
        regions_overlap(input, sizeof(*input), result_out,
                        sizeof(*result_out)) ||
        regions_overlap(result_out, sizeof(*result_out),
                        input->raw_0073_low,
                        input->raw_0073_low_count) ||
        regions_overlap(result_out, sizeof(*result_out),
                        input->raw_00e8_high,
                        input->raw_00e8_high_count) ||
        regions_overlap(result_out, sizeof(*result_out),
                        input->raw_00f3_depth,
                        input->raw_00f3_depth_count) ||
        regions_overlap(result_out, sizeof(*result_out),
                        input->raw_04b0_by_slot,
                        input->raw_04b0_by_slot_count)) {
        return false;
    }
    return true;
}

static void b06_scan_pass(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    TecmoGameplayDefenseContactB06ScanResult *result)
{
    uint8_t cursor = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;

    while (cursor > 0U) {
        uint8_t candidate;
        uint16_t reference_x;
        uint16_t candidate_x;
        uint16_t metric;
        uint16_t threshold;

        --cursor;
        candidate = cursor;
        if (candidate == input->raw_0309 ||
            (input->raw_04b0_by_slot[candidate] &
             TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_GATE_MASK) == 0U) {
            continue;
        }
        reference_x = (uint16_t)input->raw_007d |
                      (uint16_t)((uint16_t)input->raw_00f2 << 8U);
        candidate_x = (uint16_t)input->raw_0073_low[candidate] |
                      (uint16_t)((uint16_t)input->raw_00e8_high[candidate]
                                 << 8U);
        b06_metric_from_coordinates(
            reference_x, candidate_x, input->raw_00fd,
            input->raw_00f3_depth[candidate], &result->raw_0094,
            &result->raw_0095);
        metric = (uint16_t)result->raw_0094 |
                 (uint16_t)((uint16_t)result->raw_0095 << 8U);
        threshold = (uint16_t)result->raw_06d7 |
                    (uint16_t)((uint16_t)result->raw_06d8 << 8U);
        if (metric < threshold) {
            result->raw_06d7 = result->raw_0094;
            result->raw_06d8 = result->raw_0095;
            result->raw_06d5 = candidate;
            result->raw_037f_at_030b = candidate;
            result->raw_flags = (uint8_t)(result->raw_flags |
                                           TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_RESULT_FLAG_IMPROVEMENT);
        }
    }
}

bool tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    TecmoGameplayDefenseContactB06ScanResult *result_out)
{
    TecmoGameplayDefenseContactB06ScanResult result;

    if (!b06_scan_input_valid(input, result_out)) return false;
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_RESULT_TAG;
    result.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    result.raw_030b = input->raw_030b;
    result.raw_0094 = input->raw_0094;
    result.raw_0095 = input->raw_0095;
    result.raw_06d5 = input->raw_06d5;
    result.raw_06d7 = input->raw_06d7;
    result.raw_06d8 =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_THRESHOLD_HIGH;
    result.raw_037f_at_030b = input->raw_037f_at_030b;
    b06_scan_pass(input, &result);
    *result_out = result;
    return true;
}

bool tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
    const TecmoGameplayDefenseContactB05GeometryInput *input,
    TecmoGameplayDefenseContactB05GeometryResult *result_out)
{
    TecmoGameplayDefenseContactB05GeometryResult result;
    uint16_t raw_x_delta;
    uint8_t raw_depth_delta;
    bool raw_x_borrow;
    bool raw_depth_borrow;
    bool raw_x_gate;
    bool raw_depth_gate;

    if (input == NULL || result_out == NULL ||
        input->contract_tag !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_INPUT_TAG ||
        input->routine_cpu !=
            TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU ||
        regions_overlap(input, sizeof(*input), result_out,
                        sizeof(*result_out))) {
        return false;
    }
    raw_x_borrow = input->raw_x_candidate < input->raw_x_reference;
    if (raw_x_borrow) {
        raw_x_delta = (uint16_t)(0x10000U -
                                 ((uint32_t)input->raw_x_reference -
                                  (uint32_t)input->raw_x_candidate));
    } else {
        raw_x_delta = (uint16_t)(input->raw_x_candidate -
                                 input->raw_x_reference);
    }
    raw_depth_borrow = input->raw_depth_candidate <
                       input->raw_depth_reference;
    if (raw_depth_borrow) {
        raw_depth_delta = (uint8_t)(0x100U -
                                    ((uint16_t)input->raw_depth_reference -
                                     (uint16_t)input->raw_depth_candidate));
    } else {
        raw_depth_delta = (uint8_t)(input->raw_depth_candidate -
                                    input->raw_depth_reference);
    }
    raw_x_gate = (!raw_x_borrow && raw_x_delta <=
                  TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_X_LOW_MAX) ||
                 (raw_x_borrow && raw_x_delta >=
                  TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_X_HIGH_MIN);
    raw_depth_gate = (!raw_depth_borrow && raw_depth_delta <=
                      TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_DEPTH_LOW_MAX) ||
                     (raw_depth_borrow && raw_depth_delta >=
                      TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_DEPTH_HIGH_MIN);
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_TAG;
    result.routine_cpu =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU;
    result.raw_x_delta = raw_x_delta;
    result.raw_depth_delta = raw_depth_delta;
    result.raw_flags = (uint8_t)(raw_x_borrow
                                     ? TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_X_BORROW
                                     : 0U);
    result.raw_flags = (uint8_t)(result.raw_flags |
                                 (raw_depth_borrow
                                      ? TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_DEPTH_BORROW
                                      : 0U));
    result.raw_gate = (uint8_t)(raw_x_gate && raw_depth_gate
                                    ? TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_PASS
                                    : 0U);
    *result_out = result;
    return true;
}

static bool b05_state17_input_valid(
    const TecmoGameplayDefenseContactB05State17Input *input,
    const TecmoGameplayDefenseContactB05State17Result *result_out)
{
    return input != NULL && result_out != NULL &&
           input->contract_tag ==
               TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_INPUT_TAG &&
           input->routine_cpu ==
               TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU &&
           input->raw_route_established == 1U &&
           input->raw_030b <
               TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT &&
           span_is_valid(input->raw_0754_by_slot,
                         input->raw_0754_by_slot_count) &&
           input->raw_0754_by_slot_count ==
               TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT &&
           !regions_overlap(input, sizeof(*input), result_out,
                            sizeof(*result_out)) &&
           !regions_overlap(result_out, sizeof(*result_out),
                            input->raw_0754_by_slot,
                            input->raw_0754_by_slot_count);
}

bool tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
    const TecmoGameplayDefenseContactB05State17Input *input,
    TecmoGameplayDefenseContactB05State17Result *result_out)
{
    TecmoGameplayDefenseContactB05State17Result result;
    uint8_t raw_0754_before;

    if (!b05_state17_input_valid(input, result_out)) return false;
    raw_0754_before = input->raw_0754_by_slot[input->raw_030b];
    memset(&result, 0, sizeof(result));
    result.contract_tag =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_TAG;
    result.routine_cpu =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU;
    result.raw_helper_cpu =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_HELPER_CPU;
    result.raw_helper_x = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_HELPER_X;
    result.raw_state_value =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_VALUE;
    result.raw_flags =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_HELPER;
    result.raw_030b = input->raw_030b;
    result.raw_030c_be = input->raw_030c_be;
    result.raw_0754_before = raw_0754_before;
    result.raw_0754_after = (uint8_t)((uint16_t)raw_0754_before + 1U);
    result.raw_0588_before = input->raw_0588;
    result.raw_0588_after = (uint8_t)((input->raw_0588 & 0xEFU) | 0x60U);
    result.raw_ba_before = input->raw_ba;
    result.raw_ba_after = (uint8_t)(input->raw_ba | 0x80U);
    result.raw_0478_after = 0x17U;
    result.raw_0528_after = 0x17U;
    result.raw_0743_after = 0U;
    result.raw_0458_bf_before = input->raw_0458_bf;
    result.raw_0458_bf_after = input->raw_0458_bf;
    if (input->raw_030c_be != 0U) {
        result.raw_flags = (uint8_t)(result.raw_flags |
                                     TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_NIBBLE);
        result.raw_0458_bf_after =
            (uint8_t)((input->raw_0458_bf & 0xF0U) | 0x05U);
    }
    *result_out = result;
    return true;
}
