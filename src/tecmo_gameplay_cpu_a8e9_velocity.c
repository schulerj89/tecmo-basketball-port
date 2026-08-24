#include "tecmo_gameplay_cpu_a8e9_velocity.h"

#include <stdio.h>
#include <string.h>

uint16_t tecmo_gameplay_cpu_a8e9_raw_asr1(uint16_t raw_bits)
{
    return (uint16_t)((raw_bits >> 1U) | (raw_bits & 0x8000U));
}

uint16_t tecmo_gameplay_cpu_a8e9_raw_negate(uint16_t raw_bits)
{
    return (uint16_t)(0U - raw_bits);
}

bool tecmo_gameplay_cpu_a8e9_velocity_normalize(
    const TecmoGameplayCpuA8e9VelocityInput *input,
    TecmoGameplayCpuA8e9VelocityResult *result_out)
{
    TecmoGameplayCpuA8e9VelocityResult result;
    uint16_t vx;
    uint16_t vz;
    if (input == NULL || result_out == NULL ||
        (const void *)input == (const void *)result_out ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG ||
        input->orientation_035a > 1U) return false;

    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_RESULT_TAG;
    vx = input->raw_vx_04f1_04fc;
    vz = input->raw_vz_0507_0512;
    if ((vz & 0x8000U) != 0U) {
        uint16_t magnitude;
        result.negative_vz_branch = true;
        vz = tecmo_gameplay_cpu_a8e9_raw_asr1(
            tecmo_gameplay_cpu_a8e9_raw_asr1(vz));
        vx = tecmo_gameplay_cpu_a8e9_raw_asr1(
            tecmo_gameplay_cpu_a8e9_raw_asr1(vx));
        result.pre_clamp_vx_negative = (vx & 0x8000U) != 0U;
        magnitude = result.pre_clamp_vx_negative
            ? tecmo_gameplay_cpu_a8e9_raw_negate(vx)
            : vx;
        if ((magnitude & 0xFF00U) == 0U &&
            (uint8_t)magnitude < 0x30U) {
            result.clamp_magnitude = (uint8_t)(
                0x30U + (input->raw_006a & 0x0FU));
            vx = result.clamp_magnitude;
            if (result.pre_clamp_vx_negative) {
                vx = tecmo_gameplay_cpu_a8e9_raw_negate(vx);
            }
            result.clamp_applied = true;
        }
    } else {
        vx = tecmo_gameplay_cpu_a8e9_raw_asr1(
            tecmo_gameplay_cpu_a8e9_raw_asr1(vx));
    }

    result.pre_orientation_vx = vx;
    if ((input->orientation_035a == 0U && (vx & 0x8000U) != 0U) ||
        (input->orientation_035a == 1U && (vx & 0x8000U) == 0U)) {
        vx = tecmo_gameplay_cpu_a8e9_raw_negate(vx);
        result.orientation_negated_vx = true;
    }
    result.raw_vx_04f1_04fc = vx;
    result.raw_vz_0507_0512 = vz;
    *result_out = result;
    return true;
}

static bool run_case(uint16_t vx,
                     uint16_t vz,
                     uint8_t raw_6a,
                     uint8_t orientation,
                     uint16_t expected_vx,
                     uint16_t expected_vz,
                     bool expected_clamp)
{
    TecmoGameplayCpuA8e9VelocityInput input;
    TecmoGameplayCpuA8e9VelocityResult result;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG;
    input.raw_vx_04f1_04fc = vx;
    input.raw_vz_0507_0512 = vz;
    input.raw_006a = raw_6a;
    input.orientation_035a = orientation;
    return tecmo_gameplay_cpu_a8e9_velocity_normalize(&input, &result) &&
           result.raw_vx_04f1_04fc == expected_vx &&
           result.raw_vz_0507_0512 == expected_vz &&
           result.clamp_applied == expected_clamp;
}

bool tecmo_gameplay_cpu_a8e9_velocity_self_test(char *message,
                                                size_t message_size)
{
    TecmoGameplayCpuA8e9VelocityInput input;
    TecmoGameplayCpuA8e9VelocityInput input_before;
    TecmoGameplayCpuA8e9VelocityResult result;
    TecmoGameplayCpuA8e9VelocityResult result_before;
    if (message == NULL || message_size == 0U) return false;
    if (tecmo_gameplay_cpu_a8e9_raw_asr1(0U) != 0U ||
        tecmo_gameplay_cpu_a8e9_raw_asr1(0x8000U) != 0xC000U ||
        tecmo_gameplay_cpu_a8e9_raw_asr1(0xFFFFU) != 0xFFFFU ||
        tecmo_gameplay_cpu_a8e9_raw_negate(0U) != 0U ||
        tecmo_gameplay_cpu_a8e9_raw_negate(0x8000U) != 0x8000U ||
        !run_case(0U, 0U, 0U, 0U, 0U, 0U, false) ||
        !run_case(4U, 1U, 0U, 0U, 1U, 1U, false) ||
        !run_case(0x8000U, 0U, 0U, 0U, 0x2000U, 0U, false) ||
        !run_case(0x8000U, 0U, 0U, 1U, 0xE000U, 0U, false) ||
        !run_case(0xFF44U, 0xFFFFU, 0U, 0U, 0x0030U, 0xFFFFU, true) ||
        !run_case(0xFF40U, 0xFFFFU, 0U, 0U, 0x0030U, 0xFFFFU, false) ||
        !run_case(0U, 0xFFFFU, 0xAEU, 1U, 0xFFC2U, 0xFFFFU, true) ||
        !run_case(0xFED7U, 0xFFEFU, 0U, 0U, 0x004BU, 0xFFFBU, false) ||
        !run_case(0xFF65U, 0xFFC3U, 0xAEU, 0U, 0x003EU, 0xFFF0U, true) ||
        !run_case(0xFF65U, 0xFFC3U, 0xAEU, 1U, 0xFFC2U, 0xFFF0U, true)) {
        goto fail;
    }

    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG;
    input.orientation_035a = 2U;
    memset(&result, 0xA5, sizeof(result));
    input_before = input;
    result_before = result;
    if (tecmo_gameplay_cpu_a8e9_velocity_normalize(&input, &result) ||
        tecmo_gameplay_cpu_a8e9_velocity_normalize(
            (const TecmoGameplayCpuA8e9VelocityInput *)(const void *)&result,
            &result) ||
        tecmo_gameplay_cpu_a8e9_velocity_normalize(
            &input,
            (TecmoGameplayCpuA8e9VelocityResult *)(void *)&input) ||
        memcmp(&input, &input_before, sizeof(input)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) goto fail;

    (void)snprintf(message, message_size,
                   "TGVN-1 A8E9 velocity: raw16 asr/negate clamp/orientation "
                   "live=unbound");
    return true;
fail:
    (void)snprintf(message, message_size,
                   "TGVN-1 A8E9 velocity transaction failed");
    return false;
}
