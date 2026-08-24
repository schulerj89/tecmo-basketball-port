#include "tecmo_gameplay_cpu_a0f3_launch.h"

#include "tecmo_gameplay_jump_shots.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool ranges_overlap(const void *left, size_t left_size,
                           const void *right, size_t right_size)
{
    uintptr_t l;
    uintptr_t r;
    if (left == NULL || right == NULL) return false;
    l = (uintptr_t)left;
    r = (uintptr_t)right;
    return l < r + right_size && r < l + left_size;
}

void tecmo_gameplay_cpu_a0f3_assets_init(TecmoGameplayCpuA0f3Assets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->contract_tag = TECMO_GAMEPLAY_CPU_A0F3_ASSETS_TAG;
}

bool tecmo_gameplay_cpu_a0f3_assets_load(TecmoGameplayCpuA0f3Assets *assets,
                                         const char *asset_pack_path)
{
    TecmoGameplayJumpShotAssets jump_assets;
    const TecmoGameplayJumpShotSourceSpan *source;
    TecmoGameplayCpuA0f3Assets candidate;
    if (assets == NULL || asset_pack_path == NULL || asset_pack_path[0] == '\0' ||
        assets->contract_tag != TECMO_GAMEPLAY_CPU_A0F3_ASSETS_TAG ||
        assets->available) return false;
    tecmo_gameplay_jump_shots_init(&jump_assets);
    if (!tecmo_gameplay_jump_shots_load(&jump_assets, asset_pack_path)) {
        tecmo_gameplay_jump_shots_destroy(&jump_assets);
        return false;
    }
    source = tecmo_gameplay_jump_shots_find_source(
        &jump_assets, TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_DISTANCE_TABLE);
    if (source == NULL || source->bank != 5U || source->fixed_bank ||
        source->cpu_start != 0xBDF7U || source->cpu_end != 0xBEF6U ||
        source->byte_count != 256U || source->fingerprint != 0x93FCF6CBU ||
        source->fingerprint_fnv1a64 != 0x8407D4DA9578D56BULL ||
        source->bytes == NULL) {
        tecmo_gameplay_jump_shots_destroy(&jump_assets);
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_ASSETS_TAG;
    candidate.available = true;
    memcpy(candidate.lift_by_depth, source->bytes,
           sizeof(candidate.lift_by_depth));
    tecmo_gameplay_jump_shots_destroy(&jump_assets);
    *assets = candidate;
    return true;
}

static uint16_t raw_abs(uint16_t value)
{
    return (value & 0x8000U) != 0U ? (uint16_t)(0U - value) : value;
}

uint16_t tecmo_gameplay_cpu_a0f3_divide_q6(int32_t numerator_q6,
                                           uint16_t divisor)
{
    bool negative = numerator_q6 < 0;
    uint32_t magnitude = negative
        ? (uint32_t)(-(int64_t)numerator_q6)
        : (uint32_t)numerator_q6;
    uint32_t quotient;
    if (divisor == 0U) {
        quotient = magnitude == 0U ? 0U : 0x7FFFU;
    } else {
        quotient = magnitude / divisor;
        if (quotient > 0x7FFFU) quotient = 0x7FFFU;
    }
    if (negative && quotient != 0U) return (uint16_t)(0U - quotient);
    return (uint16_t)quotient;
}

bool tecmo_gameplay_cpu_a0f3_solve(
    const TecmoGameplayCpuA0f3Assets *assets,
    const TecmoGameplayCpuA0f3Input *input,
    TecmoGameplayCpuA0f3Result *result_out)
{
    static const uint8_t remap[8] = {5U,2U,0U,6U,3U,1U,7U,4U};
    static const uint16_t x_offset[8] = {
        0x0068U,0xFF98U,0U,0x0068U,0xFF98U,0U,0x0068U,0xFF98U
    };
    static const uint16_t depth_offset[8] = {
        0U,0U,0x0018U,0x0018U,0x0018U,0xFFE8U,0xFFE8U,0xFFE8U
    };
    TecmoGameplayCpuA0f3Result result;
    uint16_t duration;
    uint16_t duration_sum;
    uint8_t direction;
    if (assets == NULL || input == NULL || result_out == NULL ||
        assets->contract_tag != TECMO_GAMEPLAY_CPU_A0F3_ASSETS_TAG ||
        !assets->available ||
        input->contract_tag != TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG ||
        input->raw_direction >= 8U ||
        ranges_overlap(assets, sizeof(*assets), result_out, sizeof(*result_out)) ||
        ranges_overlap(input, sizeof(*input), result_out, sizeof(*result_out))) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_RESULT_TAG;
    direction = input->raw_direction;
    if (input->raw_006a >= 0x40U) {
        direction = remap[direction];
        result.direction_remapped = true;
    }
    result.resolved_direction = direction;
    result.target_x_95_94 = (uint16_t)(input->ball_base_x_f2_7d +
                                                x_offset[direction]);
    result.target_depth_97_96 = (uint16_t)(input->ball_base_depth_fd +
                                            depth_offset[direction]);
    result.delta_x_raw = (uint16_t)(result.target_x_95_94 -
                                    input->object10_x_e8_73);
    result.delta_depth_raw = (uint16_t)(result.target_depth_97_96 -
                                        input->object10_depth_f3);
    result.abs_delta_x = raw_abs(result.delta_x_raw);
    result.lift_bdf7 = assets->lift_by_depth[input->object10_depth_f3];
    if ((result.abs_delta_x & 0xFF00U) == 0U &&
        (uint8_t)result.abs_delta_x < 0x20U) {
        duration = result.lift_bdf7;
    } else {
        duration_sum = (uint16_t)(result.abs_delta_x + result.lift_bdf7);
        duration = (uint16_t)(duration_sum >> 1U);
    }
    if ((duration & 0xFF00U) != 0U || (uint8_t)duration >= 0x3DU) {
        duration = 0x003CU;
        result.duration_capped = true;
    }
    result.duration_051e_0513 = duration;
    result.velocity_x_q6 = (uint16_t)(
        tecmo_gameplay_cpu_a0f3_divide_q6(
            (int32_t)(int16_t)result.delta_x_raw * 64, duration) << 1U);
    result.velocity_depth_q6 = (uint16_t)(
        tecmo_gameplay_cpu_a0f3_divide_q6(
            (int32_t)(int16_t)result.delta_depth_raw * 64, duration) << 1U);
    result.accumulator_x_q6 = (uint16_t)(input->object10_x_e8_73 << 6U);
    result.accumulator_depth_q6 = (uint16_t)(
        (uint16_t)input->object10_depth_f3 << 6U);
    *result_out = result;
    return true;
}

bool tecmo_gameplay_cpu_a0f3_motion_begin(
    const TecmoGameplayCpuA0f3Result *launch,
    TecmoGameplayCpuA0f3Motion *motion_out)
{
    TecmoGameplayCpuA0f3Motion motion;
    if (launch == NULL || motion_out == NULL ||
        launch->contract_tag != TECMO_GAMEPLAY_CPU_A0F3_RESULT_TAG ||
        ranges_overlap(launch, sizeof(*launch), motion_out, sizeof(*motion_out)))
        return false;
    memset(&motion, 0, sizeof(motion));
    motion.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_MOTION_TAG;
    motion.accumulator_x_q6 = launch->accumulator_x_q6;
    motion.accumulator_depth_q6 = launch->accumulator_depth_q6;
    motion.velocity_x_q6 = launch->velocity_x_q6;
    motion.velocity_depth_q6 = launch->velocity_depth_q6;
    motion.remaining_ticks = launch->duration_051e_0513;
    *motion_out = motion;
    return true;
}

bool tecmo_gameplay_cpu_a0f3_motion_tick_publish(
    TecmoGameplayCpuA0f3Motion *motion,
    TecmoGameplayCpuA0f3PublishedPosition *position_out)
{
    TecmoGameplayCpuA0f3Motion candidate;
    TecmoGameplayCpuA0f3PublishedPosition position;
    if (motion == NULL || position_out == NULL ||
        motion->contract_tag != TECMO_GAMEPLAY_CPU_A0F3_MOTION_TAG ||
        motion->remaining_ticks == 0U ||
        ranges_overlap(motion, sizeof(*motion), position_out,
                       sizeof(*position_out))) return false;
    candidate = *motion;
    candidate.accumulator_x_q6 = (uint16_t)(candidate.accumulator_x_q6 +
                                             candidate.velocity_x_q6);
    candidate.accumulator_depth_q6 = (uint16_t)(
        candidate.accumulator_depth_q6 + candidate.velocity_depth_q6);
    --candidate.remaining_ticks;
    position.raw_x = (uint16_t)(candidate.accumulator_x_q6 >> 6U);
    position.raw_depth = (uint8_t)(candidate.accumulator_depth_q6 >> 6U);
    position.complete = candidate.remaining_ticks == 0U;
    *motion = candidate;
    *position_out = position;
    return true;
}

static bool fixture(const TecmoGameplayCpuA0f3Assets *assets,
                    uint8_t direction, uint8_t raw_6a,
                    uint16_t velocity_x, uint16_t velocity_depth,
                    uint16_t duration)
{
    TecmoGameplayCpuA0f3Input input;
    TecmoGameplayCpuA0f3Result result;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG;
    input.ball_base_x_f2_7d = 0x00A0U;
    input.ball_base_depth_fd = 0x8FU;
    input.object10_x_e8_73 = 0x00A0U;
    input.object10_depth_f3 = 0x8FU;
    input.raw_direction = direction;
    input.raw_006a = raw_6a;
    return tecmo_gameplay_cpu_a0f3_solve(assets, &input, &result) &&
           result.lift_bdf7 == 0x1AU &&
           result.velocity_x_q6 == velocity_x &&
           result.velocity_depth_q6 == velocity_depth &&
           result.duration_051e_0513 == duration &&
           result.accumulator_x_q6 == 0x2800U &&
           result.accumulator_depth_q6 == 0x23C0U;
}

bool tecmo_gameplay_cpu_a0f3_launch_self_test(const char *asset_pack_path,
                                               char *message,
                                               size_t message_size)
{
    TecmoGameplayCpuA0f3Assets assets;
    TecmoGameplayCpuA0f3Assets assets_before;
    TecmoGameplayCpuA0f3Input input;
    TecmoGameplayCpuA0f3Result result;
    TecmoGameplayCpuA0f3Result result_before;
    TecmoGameplayCpuA0f3Motion motion;
    TecmoGameplayCpuA0f3Motion motion_before;
    TecmoGameplayCpuA0f3PublishedPosition position;
    TecmoGameplayCpuA0f3PublishedPosition position_before;
    if (message == NULL || message_size == 0U) return false;
    tecmo_gameplay_cpu_a0f3_assets_init(&assets);
    if (!tecmo_gameplay_cpu_a0f3_assets_load(&assets, asset_pack_path) ||
        !fixture(&assets, 0U, 0U, 0x00DCU, 0U, 60U) ||
        !fixture(&assets, 1U, 0U, 0xFF24U, 0U, 60U) ||
        !fixture(&assets, 2U, 0U, 0U, 0x0076U, 26U) ||
        !fixture(&assets, 5U, 0U, 0U, 0xFF8AU, 26U) ||
        tecmo_gameplay_cpu_a0f3_divide_q6(0, 0U) != 0U ||
        tecmo_gameplay_cpu_a0f3_divide_q6(1, 0U) != 0x7FFFU ||
        tecmo_gameplay_cpu_a0f3_divide_q6(-1, 0U) != 0x8001U ||
        tecmo_gameplay_cpu_a0f3_divide_q6(-65, 2U) != 0xFFE0U ||
        tecmo_gameplay_cpu_a0f3_divide_q6(INT32_MAX, 1U) != 0x7FFFU ||
        tecmo_gameplay_cpu_a0f3_divide_q6(INT32_MIN, 1U) != 0x8001U)
        goto fail;

    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CPU_A0F3_INPUT_TAG;
    input.ball_base_x_f2_7d = 0x00A0U;
    input.ball_base_depth_fd = 0x8FU;
    input.object10_x_e8_73 = 0x00A0U;
    input.object10_depth_f3 = 0x8FU;
    input.raw_direction = 2U;
    input.raw_006a = 0x40U;
    if (!tecmo_gameplay_cpu_a0f3_solve(&assets, &input, &result) ||
        !result.direction_remapped || result.resolved_direction != 0U ||
        !tecmo_gameplay_cpu_a0f3_motion_begin(&result, &motion) ||
        !tecmo_gameplay_cpu_a0f3_motion_tick_publish(&motion, &position) ||
        position.raw_x != (uint16_t)(motion.accumulator_x_q6 >> 6U) ||
        position.raw_depth != (uint8_t)(motion.accumulator_depth_q6 >> 6U))
        goto fail;

    motion.remaining_ticks = 0U;
    memset(&position, 0x5A, sizeof(position));
    motion_before = motion;
    position_before = position;
    if (tecmo_gameplay_cpu_a0f3_motion_tick_publish(&motion, &position) ||
        memcmp(&motion, &motion_before, sizeof(motion)) != 0 ||
        memcmp(&position, &position_before, sizeof(position)) != 0) goto fail;
    motion.remaining_ticks = 1U;
    motion_before = motion;
    if (tecmo_gameplay_cpu_a0f3_motion_tick_publish(
            &motion,
            (TecmoGameplayCpuA0f3PublishedPosition *)(void *)&motion) ||
        memcmp(&motion, &motion_before, sizeof(motion)) != 0) goto fail;

    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    input.raw_direction = 8U;
    assets_before = assets;
    if (tecmo_gameplay_cpu_a0f3_solve(&assets, &input, &result) ||
        tecmo_gameplay_cpu_a0f3_solve(
            &assets, &input,
            (TecmoGameplayCpuA0f3Result *)(void *)&input) ||
        tecmo_gameplay_cpu_a0f3_solve(
            &assets, &input,
            (TecmoGameplayCpuA0f3Result *)(void *)&assets) ||
        memcmp(&assets, &assets_before, sizeof(assets)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0 ||
        tecmo_gameplay_cpu_a0f3_assets_load(&assets, asset_pack_path)) goto fail;

    (void)snprintf(message, message_size,
                   "TGLS-1 A0F3 launch: raw direction/LUT duration divide Q6 tick live=unbound");
    return true;
fail:
    (void)snprintf(message, message_size,
                   "TGLS-1 A0F3 launch transaction failed");
    return false;
}
