#ifndef TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_H
#define TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_INPUT_TAG 0x49385654U
#define TECMO_GAMEPLAY_CPU_A8E9_VELOCITY_RESULT_TAG 0x52385654U

typedef struct TecmoGameplayCpuA8e9VelocityInput {
    uint32_t contract_tag;
    uint16_t raw_vx_04f1_04fc;
    uint16_t raw_vz_0507_0512;
    uint8_t raw_006a;
    uint8_t orientation_035a;
} TecmoGameplayCpuA8e9VelocityInput;

typedef struct TecmoGameplayCpuA8e9VelocityResult {
    uint32_t contract_tag;
    uint16_t raw_vx_04f1_04fc;
    uint16_t raw_vz_0507_0512;
    uint16_t pre_orientation_vx;
    uint8_t clamp_magnitude;
    bool negative_vz_branch;
    bool clamp_applied;
    bool pre_clamp_vx_negative;
    bool orientation_negated_vx;
} TecmoGameplayCpuA8e9VelocityResult;

/* `$AA87/$AA93` arithmetic shift: preserve bit 15 and shift every other raw
 * bit right by one. This is defined independently of host signed shifts. */
uint16_t tecmo_gameplay_cpu_a8e9_raw_asr1(uint16_t raw_bits);

/* Wrapping 6502 two's-complement negate. */
uint16_t tecmo_gameplay_cpu_a8e9_raw_negate(uint16_t raw_bits);

/* Pure `$A8E9-$A976` velocity transaction. Input and output must be disjoint;
 * malformed or aliased calls leave the output byte-identical. */
bool tecmo_gameplay_cpu_a8e9_velocity_normalize(
    const TecmoGameplayCpuA8e9VelocityInput *input,
    TecmoGameplayCpuA8e9VelocityResult *result_out);

bool tecmo_gameplay_cpu_a8e9_velocity_self_test(char *message,
                                                size_t message_size);

#endif
