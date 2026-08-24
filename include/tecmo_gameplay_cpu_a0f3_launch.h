#ifndef TECMO_GAMEPLAY_CPU_A0F3_LAUNCH_H
#define TECMO_GAMEPLAY_CPU_A0F3_LAUNCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_A0F3_ASSETS_TAG 0x41304654U
#define TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG 0x49304654U
#define TECMO_GAMEPLAY_CPU_A0F3_RESULT_TAG 0x52304654U
#define TECMO_GAMEPLAY_CPU_A0F3_MOTION_TAG 0x4D304654U

typedef struct TecmoGameplayCpuA0f3Assets {
    uint32_t contract_tag;
    bool available;
    uint8_t lift_by_depth[256];
} TecmoGameplayCpuA0f3Assets;

typedef struct TecmoGameplayCpuA0f3Input {
    uint32_t contract_tag;
    uint16_t ball_base_x_f2_7d;
    uint8_t ball_base_depth_fd;
    uint16_t object10_x_e8_73;
    uint8_t object10_depth_f3;
    uint8_t raw_direction;
    uint8_t raw_006a;
} TecmoGameplayCpuA0f3Input;

typedef struct TecmoGameplayCpuA0f3Result {
    uint32_t contract_tag;
    uint16_t target_x_95_94;
    uint16_t target_depth_97_96;
    uint16_t delta_x_raw;
    uint16_t delta_depth_raw;
    uint16_t abs_delta_x;
    uint16_t duration_051e_0513;
    uint16_t velocity_x_q6;
    uint16_t velocity_depth_q6;
    uint16_t accumulator_x_q6;
    uint16_t accumulator_depth_q6;
    uint8_t resolved_direction;
    uint8_t lift_bdf7;
    bool direction_remapped;
    bool duration_capped;
} TecmoGameplayCpuA0f3Result;

typedef struct TecmoGameplayCpuA0f3Motion {
    uint32_t contract_tag;
    uint16_t accumulator_x_q6;
    uint16_t accumulator_depth_q6;
    uint16_t velocity_x_q6;
    uint16_t velocity_depth_q6;
    uint16_t remaining_ticks;
} TecmoGameplayCpuA0f3Motion;

typedef struct TecmoGameplayCpuA0f3PublishedPosition {
    uint16_t raw_x;
    uint8_t raw_depth;
    bool complete;
} TecmoGameplayCpuA0f3PublishedPosition;

void tecmo_gameplay_cpu_a0f3_assets_init(
    TecmoGameplayCpuA0f3Assets *assets);
bool tecmo_gameplay_cpu_a0f3_assets_load(
    TecmoGameplayCpuA0f3Assets *assets,
    const char *asset_pack_path);

/* Exact `$80A9-$815A` signed-numerator/unsigned-divisor result, before
 * A0F3's final wrapping left shift. Ordinary division retains the complete
 * 16-bit quotient; divisor zero alone selects the 7FFF/8001 sentinel. */
uint16_t tecmo_gameplay_cpu_a0f3_divide_q6(int32_t numerator_q6,
                                           uint16_t divisor);

/* Pure `$A0F3/$B32C/$BCF4` launch transaction. Rejected calls preserve the
 * destination byte-for-byte; raw direction is intentionally not a scene enum. */
bool tecmo_gameplay_cpu_a0f3_solve(
    const TecmoGameplayCpuA0f3Assets *assets,
    const TecmoGameplayCpuA0f3Input *input,
    TecmoGameplayCpuA0f3Result *result_out);

bool tecmo_gameplay_cpu_a0f3_motion_begin(
    const TecmoGameplayCpuA0f3Result *launch,
    TecmoGameplayCpuA0f3Motion *motion_out);
bool tecmo_gameplay_cpu_a0f3_motion_tick_publish(
    TecmoGameplayCpuA0f3Motion *motion,
    TecmoGameplayCpuA0f3PublishedPosition *position_out);

bool tecmo_gameplay_cpu_a0f3_launch_self_test(const char *asset_pack_path,
                                               char *message,
                                               size_t message_size);

#endif
