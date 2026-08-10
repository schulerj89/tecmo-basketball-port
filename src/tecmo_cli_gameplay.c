#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h"
#include "asset_pack/tecmo_asset_pack_gameplay_hud.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_free_throw_projection_test.h"
#include "tecmo_gameplay_hud.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_pretip.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_scene_test_internal.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_music.h"
#include "tecmo_team_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

static int tecmo_cli_run_gameplay_movement_harness(int argc,
                                                   char **argv,
                                                   int index)
{
    const char *pack_path;
    uint32_t team;
    uint32_t roster;
    uint32_t x;
    uint32_t y;
    uint32_t speed;
    uint32_t possession;
    uint32_t orientation;
    uint32_t frames;
    uint8_t held_input;
    TecmoGameplayMovementAssets assets;
    TecmoTeamDataAsset *team_data;
    TecmoGameplayCourtCoordinate position;
    TecmoGameplayMovementState state;
    TecmoGameplayMovementStepInput input;
    const TecmoTeamDataPlayer *player;

    if (index + 9 >= argc) {
        printf("Movement harness requires PACK TEAM ROSTER X Y SPEED POSSESSION ORIENTATION INPUT FRAMES\n");
        return 2;
    }
    pack_path = argv[index];
    if (!tecmo_cli_parse_u32_argument(argv[index + 1], 26U, &team) ||
        !tecmo_cli_parse_u32_argument(argv[index + 2], 11U, &roster) ||
        !tecmo_cli_parse_u32_argument(argv[index + 3],
                            TECMO_GAMEPLAY_COURT_WORLD_MAX_X, &x) ||
        !tecmo_cli_parse_u32_argument(argv[index + 4],
                            TECMO_GAMEPLAY_COURT_WORLD_MAX_Y, &y) ||
        !tecmo_cli_parse_u32_argument(argv[index + 5], 2U, &speed) ||
        !tecmo_cli_parse_u32_argument(argv[index + 6], 1U, &possession) ||
        !tecmo_cli_parse_u32_argument(argv[index + 7], 1U, &orientation) ||
        !tecmo_cli_parse_movement_input(argv[index + 8], &held_input) ||
        !tecmo_cli_parse_u32_argument(argv[index + 9], 4096U, &frames)) {
        printf("Movement harness argument rejected\n");
        return 2;
    }

    tecmo_gameplay_movement_assets_init(&assets);
    team_data = (TecmoTeamDataAsset *)malloc(sizeof(*team_data));
    if (team_data == NULL ||
        !tecmo_gameplay_movement_assets_load(&assets, pack_path) ||
        !tecmo_team_data_asset_load_from_pack(team_data, pack_path)) {
        printf("Movement harness load failed: %s\n",
               team_data == NULL ? "allocation failed" :
               !assets.available ? assets.status : team_data->status);
        free(team_data);
        tecmo_gameplay_movement_assets_destroy(&assets);
        return 1;
    }
    player = &team_data->players[team][roster];
    position.x = (int16_t)x;
    position.y = (int16_t)y;
    if (!tecmo_gameplay_movement_state_initialize(
            &assets, &state, &position,
            orientation == 0U ? 0U : 1U)) {
        printf("Movement harness initial state rejected\n");
        free(team_data);
        tecmo_gameplay_movement_assets_destroy(&assets);
        return 1;
    }
    memset(&input, 0, sizeof(input));
    input.held_direction_bits = held_input;
    input.player_movement_rating = player->profile[0];
    input.condition = player->condition_seed;
    input.speed_value = (uint8_t)speed;
    input.primary_selected_actor = true;

    printf("TGMO-1 harness team=%u roster=%u player=\"%s\" rating=%u condition=%u speed=%u possession=%u orientation=%u input=%s frames=%u\n",
           (unsigned)team, (unsigned)roster, player->name,
           (unsigned)input.player_movement_rating,
           (unsigned)input.condition, (unsigned)speed,
           (unsigned)possession, (unsigned)orientation,
           tecmo_cli_movement_input_name(held_input), (unsigned)frames);
    printf("frame=0 x=%d y=%d action=%u direction=%u fraction=%u animation=%02X boundary=%u\n",
           state.position.x, state.position.y,
           (unsigned)state.action_state, (unsigned)state.direction,
           (unsigned)state.fractional_accumulator,
           (unsigned)state.animation_phase,
           state.boundary_violation_latched ? 1U : 0U);
    for (uint32_t frame = 1U; frame <= frames; ++frame) {
        if (!tecmo_gameplay_movement_step(&assets, &state, &input)) {
            printf("Movement harness step %u rejected\n", (unsigned)frame);
            free(team_data);
            tecmo_gameplay_movement_assets_destroy(&assets);
            return 1;
        }
        printf("frame=%u x=%d y=%d action=%u direction=%u fraction=%u animation=%02X boundary=%u\n",
               (unsigned)frame, state.position.x, state.position.y,
               (unsigned)state.action_state, (unsigned)state.direction,
               (unsigned)state.fractional_accumulator,
               (unsigned)state.animation_phase,
               state.boundary_violation_latched ? 1U : 0U);
    }
    free(team_data);
    tecmo_gameplay_movement_assets_destroy(&assets);
    return 0;
}

static int tecmo_cli_run_gameplay_cpu_steering_inspect(int argc,
                                                        char **argv,
                                                        int index)
{
    TecmoGameplayCpuSteeringAssets assets;
    TecmoGameplayCpuSteeringCommand command;
    const char *pack_path;
    uint32_t stream_offset;
    int16_t horizontal_delta;
    int16_t depth_delta;
    uint8_t direction;
    if (index + 3 >= argc) {
        printf("CPU steering inspect requires PACK OFFSET DX DY\n");
        return 2;
    }
    pack_path = argv[index];
    if (!tecmo_cli_parse_u32_argument(
            argv[index + 1],
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE -
                TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE,
            &stream_offset) ||
        !tecmo_cli_parse_i16_argument(argv[index + 2], &horizontal_delta) ||
        !tecmo_cli_parse_i16_argument(argv[index + 3], &depth_delta)) {
        printf("CPU steering inspect argument rejected\n");
        return 2;
    }
    tecmo_gameplay_cpu_steering_assets_init(&assets);
    if (!tecmo_gameplay_cpu_steering_assets_load(&assets, pack_path)) {
        printf("CPU steering inspect load failed: %s\n", assets.status);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }
    if (!tecmo_gameplay_cpu_steering_decode_command(
            &assets, (uint16_t)stream_offset, &command)) {
        printf("CPU steering command offset rejected\n");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }
    if (!tecmo_gameplay_cpu_steering_direction_for_delta(
            &assets, horizontal_delta, depth_delta, &direction)) {
        printf("CPU steering zero/invalid direction vector rejected\n");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }
    printf("TGAI-2 offset=$%04X cpu=$%04X opcode=%u args=%02X,%02X,%02X,%02X handler=$%04X kind=%s\n",
           (unsigned)command.stream_offset,
           (unsigned)command.cpu_address,
           (unsigned)command.opcode,
           (unsigned)command.arguments[0],
           (unsigned)command.arguments[1],
           (unsigned)command.arguments[2],
           (unsigned)command.arguments[3],
           (unsigned)command.handler_cpu,
           tecmo_gameplay_cpu_steering_command_kind_name(command.kind));
    printf("delta=(%d,%d) direction=%u name=%s normal_flow=0\n",
           horizontal_delta, depth_delta, (unsigned)direction,
           tecmo_gameplay_cpu_steering_direction_name(direction));
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return 0;
}

static void tecmo_cli_opcode15_raw_fixture(
    TecmoGameplayCpuSteeringOpcode15RawInput *input)
{
    memset(input, 0, sizeof(*input));
    input->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_INPUT_TAG;
    input->observed_mask =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK;
    input->command_record_offset =
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET;
    input->actor_x = 6U;
    input->raw_0499_slot10 = 0x46U;
    input->raw_04b0_actor_x = 0x10U;
    input->raw_0308_primary = 4U;
    input->raw_0309_defender = 9U;
    input->raw_030a_offense_side = 0U;
    input->raw_030b_defense_side = 1U;
    input->raw_000e_000f_selected_actor[0U] = 4U;
    input->raw_000e_000f_selected_actor[1U] = 9U;
    input->raw_06d5 = 6U;
    input->raw_06d6 = 2U;
    input->raw_059e = 5U;
    for (uint8_t actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        input->actor[actor].raw_0547_0551_stream_offset =
            (uint16_t)(0x0100U + actor * 5U);
        input->actor[actor].raw_057c_state = 0x04U;
        input->actor[actor].raw_046e_timer = (uint8_t)(0x20U + actor);
        input->actor[actor].raw_0463_direction =
            (uint8_t)(actor % TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT);
        input->actor[actor].raw_0442_pose_low = 0xAAU;
        input->actor[actor].raw_044d_pose_high = 0xBBU;
        input->actor[actor].raw_0479_sprite_flags = 0x40U;
        input->actor[actor].raw_0458_action = 0x50U;
    }
    input->actor[9U].raw_0547_0551_stream_offset = 0x1234U;
    input->actor[9U].raw_057c_state = 0x08U;
    input->actor[9U].raw_046e_timer = 0xC3U;
    input->actor[9U].raw_0463_direction = 5U;
}

/* A deterministic raw-contract proof, deliberately separate from LIVE. It
   captures the exact resolver's transactional state but neither derives raw
   RAM from native scene state nor executes Bank07 $C711. */
static int tecmo_cli_run_gameplay_cpu_steering_opcode15_harness(
    int argc,
    char **argv,
    int index)
{
    TecmoGameplayCpuSteeringAssets assets;
    TecmoGameplayCpuSteeringOpcode15RawInput input;
    TecmoGameplayCpuSteeringOpcode15RawInput output;
    TecmoGameplayCpuSteeringOpcode15RawResult gate;
    TecmoGameplayCpuSteeringOpcode15RawResult retry;
    TecmoGameplayCpuSteeringOpcode15RawResult primary_swap;
    TecmoGameplayCpuSteeringOpcode15RawResult mark_other;
    TecmoGameplayCpuSteeringOpcode15RawResult selected;
    const char *pack_path;

    if (argc - index != 1) {
        printf("Opcode-15 raw harness requires PACK\n");
        return 2;
    }
    pack_path = argv[index];
    tecmo_gameplay_cpu_steering_assets_init(&assets);
    if (!tecmo_gameplay_cpu_steering_assets_load(&assets, pack_path)) {
        printf("Opcode-15 raw harness load failed: %s\n", assets.status);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }

    tecmo_cli_opcode15_raw_fixture(&input);
    input.observed_mask =
        TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_SLOT10_0499;
    input.raw_0499_slot10 = 0x45U;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            &assets, &input, &output, &gate)) goto rejected;
    tecmo_cli_opcode15_raw_fixture(&input);
    input.raw_04b0_actor_x = 0U;
    input.raw_007e = 0x04U;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            &assets, &input, &output, &retry)) goto rejected;
    input.raw_007e = 0U;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            &assets, &input, &output, &primary_swap)) goto rejected;
    input.raw_04b0_actor_x = 0x10U;
    input.raw_007e = 0x08U;
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            &assets, &input, &output, &mark_other)) goto rejected;
    tecmo_cli_opcode15_raw_fixture(&input);
    if (!tecmo_gameplay_cpu_steering_opcode15_resolve_raw(
            &assets, &input, &output, &selected)) goto rejected;

    printf("{\"schema\":\"tecmo.gameplay-cpu-steering/opcode15-raw-harness/TGAI-2\","
           "\"mode\":\"harness-only\","
           "\"canonical_records\":[\"0037\",\"004B\"],"
           "\"branches\":{\"gate_noop\":\"%s\","
           "\"primary_retry\":\"%s\",\"primary_swap\":\"%s\","
           "\"mark_other\":\"%s\",\"selected_defender\":\"%s\"},"
           "\"selected_defender\":{\"committed\":%s,"
           "\"raw_0308\":[%u,%u],\"raw_0309\":[%u,%u],"
           "\"old_defender_stream\":[%u,%u],"
           "\"old_defender_state\":[%u,%u],"
           "\"new_actor_state\":[%u,%u],\"raw_059E\":[%u,%u],"
           "\"selection_06D5\":[%u,%u],\"selection_06D6\":[%u,%u],"
           "\"c711\":{\"selector\":%u,\"x\":%u,\"y\":%u,"
           "\"observed_unexecuted\":%s}}}\n",
           tecmo_gameplay_cpu_steering_opcode15_branch_name(gate.branch),
           tecmo_gameplay_cpu_steering_opcode15_branch_name(retry.branch),
           tecmo_gameplay_cpu_steering_opcode15_branch_name(primary_swap.branch),
           tecmo_gameplay_cpu_steering_opcode15_branch_name(mark_other.branch),
           tecmo_gameplay_cpu_steering_opcode15_branch_name(selected.branch),
           selected.committed ? "true" : "false",
           (unsigned)selected.raw_0308_before,
           (unsigned)selected.raw_0308_after,
           (unsigned)selected.raw_0309_before,
           (unsigned)selected.raw_0309_after,
           (unsigned)selected.defender_stream_before,
           (unsigned)selected.defender_stream_after,
           (unsigned)selected.defender_state_before,
           (unsigned)selected.defender_state_after,
           (unsigned)selected.new_actor_state_before,
           (unsigned)selected.new_actor_state_after,
           (unsigned)selected.raw_059e_before,
           (unsigned)selected.raw_059e_after,
           (unsigned)selected.raw_06d5_before,
           (unsigned)selected.raw_06d5_after,
           (unsigned)selected.raw_06d6_before,
           (unsigned)selected.raw_06d6_after,
           (unsigned)selected.c711_selector,
           (unsigned)selected.c711_x_actor,
           (unsigned)selected.c711_y_actor,
           selected.c711_selector_observed_unexecuted ? "true" : "false");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return 0;

rejected:
    printf("Opcode-15 raw harness resolver rejected canonical fixture\n");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return 1;
}

static int tecmo_cli_run_gameplay_cpu_steering_harness(int argc,
                                                        char **argv,
                                                        int index)
{
    const int required_argument_count =
        7 + (int)TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    const char *pack_path;
    uint32_t actor;
    uint32_t possession;
    uint32_t orientation;
    uint32_t ball_holder;
    uint32_t matchup_actor;
    uint32_t difficulty;
    TecmoGameplayCpuSteeringAssets assets;
    TecmoGameplayCpuSteeringHarnessInput input;
    TecmoGameplayCpuSteeringHarnessResult result;
    size_t position_index;
    if (argc - index != required_argument_count) {
        printf("CPU steering harness requires PACK ACTOR POSSESSION ORIENTATION HOLDER MATCHUP DIFFICULTY X0,Y0 ... X9,Y9\n");
        return 2;
    }
    pack_path = argv[index];
    memset(&input, 0, sizeof(input));
    input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    if (!tecmo_cli_parse_u32_argument(
            argv[index + 1],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U, &actor) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 2],
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT - 1U, &possession) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 3],
            TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT - 1U,
            &orientation) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 4],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U, &ball_holder) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 5],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U, &matchup_actor) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 6],
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT - 1U,
            &difficulty)) {
        printf("CPU steering harness argument rejected\n");
        return 2;
    }
    for (position_index = 0U;
         position_index < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++position_index) {
        if (!tecmo_cli_parse_court_coordinate_argument(
                argv[index + 7 + (int)position_index],
                &input.actor_position[position_index])) {
            printf("CPU steering harness argument rejected\n");
            return 2;
        }
    }
    input.actor = (uint8_t)actor;
    input.possession = (uint8_t)possession;
    input.orientation = (uint8_t)orientation;
    input.ball_holder = (uint8_t)ball_holder;
    input.matchup_actor = (uint8_t)matchup_actor;
    input.difficulty = (uint8_t)difficulty;

    tecmo_gameplay_cpu_steering_assets_init(&assets);
    if (!tecmo_gameplay_cpu_steering_assets_load(&assets, pack_path)) {
        printf("CPU steering harness load failed: %s\n", assets.status);
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }
    if (!tecmo_gameplay_cpu_steering_harness_evaluate(
            &assets, &input, &result)) {
        printf("CPU steering harness state rejected\n");
        tecmo_gameplay_cpu_steering_assets_destroy(&assets);
        return 1;
    }
    printf("TGAI-2 harness actor=%u team=%u possession=%u orientation=%u holder=%u matchup=%u difficulty=%u snapshot=%08X normal_flow=0\n",
           (unsigned)result.actor, (unsigned)result.actor_team,
           (unsigned)result.possession, (unsigned)result.orientation,
           (unsigned)result.ball_holder, (unsigned)result.matchup_actor,
           (unsigned)result.difficulty,
           (unsigned)result.input_fingerprint);
    printf("court=");
    for (position_index = 0U;
         position_index < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++position_index) {
        printf("%s%u:(%d,%d)", position_index == 0U ? "" : ";",
               (unsigned)position_index,
               input.actor_position[position_index].x,
               input.actor_position[position_index].y);
    }
    printf("\n");
    printf("target=%s target_actor=",
           tecmo_gameplay_cpu_steering_harness_target_kind_name(
               result.target_kind));
    if (result.target_actor == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
        printf("none");
    } else {
        printf("%u", (unsigned)result.target_actor);
    }
    printf(" from=(%d,%d) to=(%d,%d) delta=(%d,%d) ",
           result.actor_position.x, result.actor_position.y,
           result.target_position.x, result.target_position.y,
           result.horizontal_delta, result.depth_delta);
    if (result.writes_direction) {
        printf("direction=%u name=%s write=1 ",
               (unsigned)result.direction,
               tecmo_gameplay_cpu_steering_direction_name(
                   result.direction));
    } else {
        printf("direction=keep name=keep write=0 ");
    }
    printf("target_policy=native-harness quantizer=rom-exact scene_adapter=1 normal_flow=0\n");
    tecmo_gameplay_cpu_steering_assets_destroy(&assets);
    return 0;
}

static int tecmo_cli_run_gameplay_cpu_steering_movement_harness(
    int argc,
    char **argv,
    int index)
{
    const int required_argument_count =
        11 + (int)TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
    const char *pack_path;
    uint32_t actor;
    uint32_t possession;
    uint32_t orientation;
    uint32_t ball_holder;
    uint32_t matchup_actor;
    uint32_t difficulty;
    uint32_t rating;
    uint32_t condition;
    uint32_t speed;
    uint32_t frames;
    TecmoGameplayCpuSteeringAssets steering_assets;
    TecmoGameplayMovementAssets movement_assets;
    TecmoGameplayCpuSteeringMovementInput input;
    TecmoGameplayCpuSteeringMovementResult result;
    size_t position_index;
    if (argc - index != required_argument_count) {
        printf("CPU steering movement harness requires PACK ACTOR POSSESSION ORIENTATION HOLDER MATCHUP DIFFICULTY RATING CONDITION SPEED FRAMES X0,Y0 ... X9,Y9\n");
        return 2;
    }
    pack_path = argv[index];
    memset(&input, 0, sizeof(input));
    input.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_MOVEMENT_INPUT_TAG;
    input.steering.contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_INPUT_TAG;
    if (!tecmo_cli_parse_u32_argument(
            argv[index + 1],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U, &actor) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 2],
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT - 1U, &possession) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 3],
            TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT - 1U,
            &orientation) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 4],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U,
            &ball_holder) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 5],
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U,
            &matchup_actor) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 6],
            TECMO_GAMEPLAY_CPU_STEERING_DIFFICULTY_COUNT - 1U,
            &difficulty) ||
        !tecmo_cli_parse_u32_argument(argv[index + 7], 0xFFU, &rating) ||
        !tecmo_cli_parse_u32_argument(argv[index + 8], 100U, &condition) ||
        !tecmo_cli_parse_u32_argument(
            argv[index + 9],
            TECMO_GAMEPLAY_MOVEMENT_SPEED_COUNT - 1U, &speed) ||
        !tecmo_cli_parse_u32_argument(argv[index + 10], 4096U, &frames)) {
        printf("CPU steering movement harness argument rejected\n");
        return 2;
    }
    for (position_index = 0U;
         position_index < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++position_index) {
        if (!tecmo_cli_parse_court_coordinate_argument(
                argv[index + 11 + (int)position_index],
                &input.steering.actor_position[position_index])) {
            printf("CPU steering movement harness argument rejected\n");
            return 2;
        }
    }
    input.steering.actor = (uint8_t)actor;
    input.steering.possession = (uint8_t)possession;
    input.steering.orientation = (uint8_t)orientation;
    input.steering.ball_holder = (uint8_t)ball_holder;
    input.steering.matchup_actor = (uint8_t)matchup_actor;
    input.steering.difficulty = (uint8_t)difficulty;
    input.player_movement_rating = (uint8_t)rating;
    input.condition = (uint8_t)condition;
    input.speed_value = (uint8_t)speed;
    input.primary_selected_actor = actor == ball_holder;

    tecmo_gameplay_cpu_steering_assets_init(&steering_assets);
    tecmo_gameplay_movement_assets_init(&movement_assets);
    if (!tecmo_gameplay_cpu_steering_assets_load(
            &steering_assets, pack_path) ||
        !tecmo_gameplay_movement_assets_load(
            &movement_assets, pack_path)) {
        printf("CPU steering movement harness load failed: %s\n",
               !steering_assets.available
                   ? steering_assets.status
                   : movement_assets.status);
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        tecmo_gameplay_cpu_steering_assets_destroy(&steering_assets);
        return 1;
    }
    if (!tecmo_gameplay_movement_state_initialize(
            &movement_assets, &input.movement,
            &input.steering.actor_position[input.steering.actor],
            orientation == 0U ? 1U : 0U)) {
        printf("CPU steering movement harness initial state rejected\n");
        tecmo_gameplay_movement_assets_destroy(&movement_assets);
        tecmo_gameplay_cpu_steering_assets_destroy(&steering_assets);
        return 1;
    }

    printf("TGAI-TGMO harness actor=%u possession=%u orientation=%u holder=%u matchup=%u difficulty=%u rating=%u condition=%u speed=%u frames=%u target_policy=native-harness zero_input=native-neutral quantizer=rom-exact movement=rom-exact primary=%u secondary=%u scene_adapter=1 normal_flow=0\n",
           (unsigned)actor, (unsigned)possession,
           (unsigned)orientation, (unsigned)ball_holder,
           (unsigned)matchup_actor, (unsigned)difficulty,
           (unsigned)rating, (unsigned)condition, (unsigned)speed,
           (unsigned)frames,
           input.primary_selected_actor ? 1U : 0U,
           input.primary_selected_actor ? 0U : 1U);
    printf("frame=0 x=%d y=%d action=%u direction=%u fraction=%u animation=%02X boundary=%u\n",
           input.movement.position.x, input.movement.position.y,
           (unsigned)input.movement.action_state,
           (unsigned)input.movement.direction,
           (unsigned)input.movement.fractional_accumulator,
           (unsigned)input.movement.animation_phase,
           input.movement.boundary_violation_latched ? 1U : 0U);
    for (uint32_t frame = 1U; frame <= frames; ++frame) {
        if (!tecmo_gameplay_cpu_steering_movement_step(
                &steering_assets, &movement_assets, &input, &result)) {
            printf("CPU steering movement harness step %u rejected\n",
                   (unsigned)frame);
            tecmo_gameplay_movement_assets_destroy(&movement_assets);
            tecmo_gameplay_cpu_steering_assets_destroy(&steering_assets);
            return 1;
        }
        printf("frame=%u snapshot=%08X from=(%d,%d) target=(%d,%d) steering=%s write=%u held=%s x=%d y=%d action=%u direction=%u fraction=%u animation=%02X boundary=%u\n",
               (unsigned)frame,
               (unsigned)result.steering.input_fingerprint,
               result.steering.actor_position.x,
               result.steering.actor_position.y,
               result.steering.target_position.x,
               result.steering.target_position.y,
               result.steering.writes_direction
                   ? tecmo_gameplay_cpu_steering_direction_name(
                         result.steering.direction)
                   : "keep",
               result.steering.writes_direction ? 1U : 0U,
               tecmo_cli_movement_input_name(result.held_direction_bits),
               result.movement.position.x,
               result.movement.position.y,
               (unsigned)result.movement.action_state,
               (unsigned)result.movement.direction,
               (unsigned)result.movement.fractional_accumulator,
               (unsigned)result.movement.animation_phase,
               result.movement.boundary_violation_latched ? 1U : 0U);
        input.movement = result.movement;
        input.steering.actor_position[input.steering.actor] =
            result.movement.position;
    }
    tecmo_gameplay_movement_assets_destroy(&movement_assets);
    tecmo_gameplay_cpu_steering_assets_destroy(&steering_assets);
    return 0;
}

int tecmo_cli_run_gameplay_commands(const TecmoCliContext *context)
{
    const char *command;
    const char *root;
    int argc;
    char **argv;
    int index;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    root = context->root;
    argc = context->argc;
    argv = context->argv;
    index = context->index;
    if (strcmp(command, "--gameplay-movement-harness") == 0) {
        return tecmo_cli_run_gameplay_movement_harness(argc, argv, index);
    }

    if (strcmp(command, "--gameplay-cpu-steering-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_cpu_steering_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay CPU steering test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-hud-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_hud_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay HUD asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-cpu-steering-inspect") == 0) {
        return tecmo_cli_run_gameplay_cpu_steering_inspect(argc, argv, index);
    }

    if (strcmp(command, "--gameplay-cpu-steering-harness") == 0) {
        return tecmo_cli_run_gameplay_cpu_steering_harness(argc, argv, index);
    }

    if (strcmp(command, "--gameplay-cpu-steering-opcode15-harness") == 0) {
        return tecmo_cli_run_gameplay_cpu_steering_opcode15_harness(
            argc, argv, index);
    }

    if (strcmp(
            command,
            "--gameplay-cpu-steering-movement-harness") == 0) {
        return tecmo_cli_run_gameplay_cpu_steering_movement_harness(
            argc, argv, index);
    }

    if (strcmp(
            command,
            "--gameplay-free-throw-projection-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_free_throw_projection_self_test(
                pack_path, message, sizeof(message))) {
            printf("Free-throw projection test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    /*
     * Developer-only direct importer path used by the focused TGCP ROM
     * mutation suite. Normal builds continue through --build-assetpack.
     */
    if (strcmp(
            command,
            "--gameplay-camera-projection-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (tecmo_asset_pack_gameplay_camera_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay camera source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    /* Developer-only isolated importer gate for the TGMO mutation suite. */
    if (strcmp(command, "--gameplay-movement-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (tecmo_asset_pack_gameplay_movement_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay movement source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    /* Developer-only isolated importer gate for the TGAI mutation suite. */
    if (strcmp(command, "--gameplay-cpu-steering-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (tecmo_asset_pack_gameplay_cpu_steering_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay CPU steering source test failed: %s\n",
                   message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    /* Developer-only isolated importer gate for the THUD mutation suite. */
    if (strcmp(command, "--gameplay-hud-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (tecmo_asset_pack_gameplay_hud_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay HUD source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    /*
     * Developer-only direct importer path used by the focused TGFL ROM
     * mutation suite. Normal builds continue through --build-assetpack.
     */
    if (strcmp(
            command,
            "--gameplay-free-throw-lineup-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (tecmo_asset_pack_gameplay_free_throw_lineup_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Free-throw lineup source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-scene-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoMusicAsset music_asset;
        TecmoMusicPlayer music_player;
        char message[256];
        bool passed;
        memset(&music_asset, 0, sizeof(music_asset));
        if (pack_path == NULL ||
            !tecmo_music_asset_load_from_pack(&music_asset, pack_path)) {
            printf("Gameplay scene test failed: %s\n",
                   pack_path == NULL ? "PACK path required"
                                     : music_asset.status);
            tecmo_music_asset_shutdown(&music_asset);
            return 1;
        }
        tecmo_music_player_init(&music_player, &music_asset);
        passed = tecmo_gameplay_scene_self_test(
            root, pack_path, &music_player, message, sizeof(message));
        tecmo_music_asset_shutdown(&music_asset);
        if (!passed) {
            printf("Gameplay scene test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--tipoff-regression-trace") == 0) {
        const char *pack_path = index < argc ? argv[index++] : NULL;
        const char *output_directory = index < argc ? argv[index] : NULL;
        if (pack_path == NULL || output_directory == NULL) {
            printf("Tip-off regression trace requires PACK DIR\n");
            return 2;
        }
        return tecmo_cli_run_tipoff_regression_trace(
            root, pack_path, output_directory);
    }

    if (strcmp(command, "--gameplay-pretip-human-checkpoint") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoMusicAsset music_asset;
        TecmoMusicPlayer music_player;
        char message[256];
        bool passed;
        memset(&music_asset, 0, sizeof(music_asset));
        if (pack_path == NULL ||
            !tecmo_music_asset_load_from_pack(&music_asset, pack_path)) {
            printf("Gameplay pre-tip human checkpoint failed: %s\n",
                   pack_path == NULL ? "PACK path required"
                                     : music_asset.status);
            tecmo_music_asset_shutdown(&music_asset);
            return 1;
        }
        tecmo_music_player_init(&music_player, &music_asset);
        passed = tecmo_gameplay_scene_test_pretip_human_checkpoint(
            root, pack_path, &music_player, message, sizeof(message));
        tecmo_music_asset_shutdown(&music_asset);
        if (!passed) {
            printf("Gameplay pre-tip human checkpoint failed: %s\n",
                   message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-pretip-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (pack_path == NULL ||
            !tecmo_gameplay_pretip_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay pre-tip test failed: %s\n",
                   pack_path == NULL ? "PACK path required" : message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--arena-scene-test") == 0) {
        char message[160];
        if (!tecmo_arena_intro_scene_self_test(message, sizeof(message))) {
            printf("Arena intro scene self-test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }
    return TECMO_CLI_NOT_HANDLED;
}
