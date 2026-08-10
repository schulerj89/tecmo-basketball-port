#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_actor_command_assignment.h"

#include "asset_pack/tecmo_asset_pack_gameplay_actor_command_assignment.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LIFECYCLE_TAG 0x41414341U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0U] |
                      ((uint16_t)bytes[1U] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0U] | ((uint32_t)bytes[1U] << 8U) |
           ((uint32_t)bytes[2U] << 16U) |
           ((uint32_t)bytes[3U] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    unsigned index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    if (bytes == NULL) return false;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload != NULL &&
           payload_size ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE &&
           memcmp(payload, "TGCA", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32 &&
           read_u32(payload + 32U) == 393232U &&
           read_u32(payload + 36U) == 0x0650F5B0U &&
           bytes_are_zero(payload + 72U,
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_HEADER_SIZE - 72U);
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    size_t index;
    size_t expected_payload_offset =
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET;
    for (index = 0U;
         index < TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayActorCommandAssignmentExpectedSource *expected =
            &tecmo_gameplay_actor_command_assignment_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
            expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->payload_offset ||
            read_u32(record + 16U) != expected->fingerprint_fnv1a32 ||
            read_u64(record + 20U) != expected->fingerprint_fnv1a64 ||
            read_u16(record + 28U) != (uint16_t)index ||
            !bytes_are_zero(record + 30U, 2U) ||
            expected->payload_offset != expected_payload_offset ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint_fnv1a32 ||
            fnv1a64(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint_fnv1a64) {
            return false;
        }
        expected_payload_offset += expected->byte_count;
    }
    return expected_payload_offset ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_OFFSET &&
           bytes_are_zero(
               payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_OFFSET,
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_PADDING_SIZE) &&
           fnv1a32(
               payload + TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET,
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_SIZE) ==
               TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_FNV1A32;
}

static bool reject(TecmoGameplayActorCommandAssignmentAssets *candidate,
                   TecmoGameplayActorCommandAssignmentAssets *assets,
                   const char *message)
{
    if (candidate != NULL) {
        free(candidate->storage);
        candidate->storage = NULL;
    }
    if (assets != NULL) {
        free(assets->storage);
        tecmo_gameplay_actor_command_assignment_assets_init(assets);
        (void)snprintf(assets->status, sizeof(assets->status), "%s",
                       message != NULL ? message : "TGCA-1 rejected");
    }
    return false;
}

void tecmo_gameplay_actor_command_assignment_assets_init(
    TecmoGameplayActorCommandAssignmentAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LIFECYCLE_TAG;
}

void tecmo_gameplay_actor_command_assignment_assets_destroy(
    TecmoGameplayActorCommandAssignmentAssets *assets)
{
    if (assets == NULL || assets->lifecycle_tag !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_actor_command_assignment_assets_init(assets);
}

bool tecmo_gameplay_actor_command_assignment_assets_parse(
    TecmoGameplayActorCommandAssignmentAssets *assets,
    const uint8_t *payload,
    size_t payload_size)
{
    TecmoGameplayActorCommandAssignmentAssets candidate;
    TecmoGameplayActorCommandAssignmentAssets previous;
    uint8_t *storage;
    size_t index;
    if (assets == NULL || assets->lifecycle_tag !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_actor_command_assignment_assets_init(&candidate);
    if (!validate_header(payload, payload_size) ||
        fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_FNV1A32 ||
        !validate_source_records(payload, payload_size)) {
        return reject(&candidate, assets,
            "TGCA-1 header/source descriptor contract rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) {
        return reject(&candidate, assets, "TGCA-1 allocation failed");
    }
    memcpy(storage, payload, payload_size);
    candidate.storage = storage;
    candidate.storage_size = payload_size;
    for (index = 0U;
         index < TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayActorCommandAssignmentExpectedSource *expected =
            &tecmo_gameplay_actor_command_assignment_expected_sources[index];
        TecmoGameplayActorCommandAssignmentSourceSpan *source =
            &candidate.sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint_fnv1a32 = expected->fingerprint_fnv1a32;
        source->fingerprint_fnv1a64 = expected->fingerprint_fnv1a64;
        source->bytes = storage + expected->payload_offset;
    }
    candidate.available = true;
    (void)snprintf(candidate.status, sizeof(candidate.status),
                   "TGCA-1 actor-command-assignment evidence assetpack");
    previous = *assets;
    *assets = candidate;
    free(previous.storage);
    return true;
}

bool tecmo_gameplay_actor_command_assignment_assets_load(
    TecmoGameplayActorCommandAssignmentAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint64_t payload_size = 0U;
    bool loaded;
    if (assets == NULL || assets->lifecycle_tag !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_LIFECYCLE_TAG) {
        return false;
    }
    if (asset_pack_path == NULL || tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE,
            &payload, &payload_size) != 0) {
        if (!assets->available) {
            (void)snprintf(assets->status, sizeof(assets->status),
                           "TGCA-1 entry missing or wrong-sized");
        }
        return false;
    }
    loaded = tecmo_gameplay_actor_command_assignment_assets_parse(
        assets, payload, (size_t)payload_size);
    tecmo_asset_pack_free(payload);
    return loaded;
}

const TecmoGameplayActorCommandAssignmentSourceSpan *
tecmo_gameplay_actor_command_assignment_find_source(
    const TecmoGameplayActorCommandAssignmentAssets *assets,
    TecmoGameplayActorCommandAssignmentSourceKind kind)
{
    size_t index;
    if (assets == NULL || !assets->available) return NULL;
    for (index = 0U;
         index < TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCE_COUNT;
         ++index) {
        if (assets->sources[index].kind == kind) return &assets->sources[index];
    }
    return NULL;
}

static uint16_t source_abs16(int16_t left, int16_t right)
{
    uint16_t delta = (uint16_t)((uint16_t)left - (uint16_t)right);
    return (delta & 0x8000U) != 0U ? (uint16_t)(0U - delta) : delta;
}

static uint8_t source_abs8(int16_t left, int16_t right)
{
    uint8_t delta = (uint8_t)((uint8_t)left - (uint8_t)right);
    return (delta & 0x80U) != 0U ? (uint8_t)(0U - delta) : delta;
}

static void scan_side(const TecmoGameplayLiveFoundation *foundation,
                      const TecmoGameplayCourtCoordinate *target,
                      uint8_t required_flag,
                      TecmoGameplayActorCommandAssignmentScan *scan,
                      TecmoGameplayLiveFoundation *candidate)
{
    int actor;
    uint16_t best = 0x0505U;
    uint8_t winner = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    memset(scan, 0, sizeof(*scan));
    scan->executed = true;
    scan->required_04b0_bit10 = required_flag;
    scan->winner_actor = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    scan->winner_score = 0x0505U;
    scan->excluded_primary_actor = foundation->primary_actor;
    scan->excluded_defender_actor = foundation->defender_actor;
    for (actor = (int)TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1;
         actor >= 0; --actor) {
        uint16_t score;
        if ((uint8_t)actor == foundation->primary_actor ||
            (uint8_t)actor == foundation->defender_actor ||
            (foundation->actor_selector_flags[actor] & 0x10U) !=
                required_flag) {
            continue;
        }
        score = (uint16_t)(
            source_abs16(foundation->actor_position[actor].x, target->x) +
            source_abs8(foundation->actor_position[actor].y, target->y));
        /* $A046 subtracts candidate from best and accepts carry, so equality
           replaces the prior winner. Descending X therefore leaves the
           lowest-index tied actor selected. */
        if (score <= best) {
            best = score;
            winner = (uint8_t)actor;
        }
    }
    if (winner == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
        scan->no_candidate = true;
        return;
    }
    scan->winner_actor = winner;
    scan->winner_score = best;
    candidate->play_state.stream_offset[winner] =
        required_flag != 0U ? 0x0019U : 0x000AU;
    candidate->last_step_offset[winner] =
        candidate->play_state.stream_offset[winner];
    candidate->play_state.actor_state[winner] = 0x04U;
}

static bool caller_gates_valid(
    const TecmoGameplayActorCommandAssignmentInput *input)
{
    if (input == NULL) return false;
    switch (input->caller) {
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE10_B73A:
        /* $B6E5 -> $B73A. $0499 == 0 also satisfies the preceding
           (< $46 || negative $04AF) gate, but it remains represented here
           rather than silently omitting that source branch. */
        return input->raw_object_state == 0x10U &&
            (input->raw_ba & 0x03U) == 0U &&
            input->raw_05a1 == 0U &&
            (input->raw_0499 < 0x46U ||
             (input->raw_04af & 0x80U) != 0U) &&
            (input->raw_0588 & 0x80U) != 0U &&
            input->raw_0499 == 0U &&
            (input->raw_0067 != 0U || input->raw_0068 != 0U);
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE17_B783:
        return input->raw_object_state == 0x17U && input->raw_0499 < 0x04U &&
            (input->raw_0588 & 0x20U) != 0U;
    case TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE18_B7B6:
        return input->raw_object_state == 0x18U && input->raw_0499 == 0U;
    default:
        return false;
    }
}

bool tecmo_gameplay_actor_command_assignment_apply(
    const TecmoGameplayActorCommandAssignmentAssets *assignment_assets,
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    const TecmoGameplayActorCommandAssignmentInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayActorCommandAssignmentResult *result_out)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayActorCommandAssignmentResult result;
    uint8_t primary;
    uint8_t defender;
    if (assignment_assets == NULL || !assignment_assets->available ||
        steering_assets == NULL || !steering_assets->available ||
        input == NULL || foundation_io == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_INPUT_TAG ||
        !tecmo_gameplay_live_foundation_valid(steering_assets, foundation_io)) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RESULT_TAG;
    result.caller = input->caller;
    if (input->caller ==
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_INTERACTION_9FE2) {
        result.noop_reason =
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_INTERACTION_UNOWNED;
        *result_out = result;
        return true;
    }
    if (!caller_gates_valid(input)) {
        result.noop_reason = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_CALLER_GATES;
        *result_out = result;
        return true;
    }
    if (!input->object10_target_valid ||
        !tecmo_gameplay_court_coordinate_valid(&input->object10_target)) {
        result.noop_reason = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_TARGET_UNOWNED;
        *result_out = result;
        return true;
    }
    candidate = *foundation_io;
    primary = candidate.primary_actor;
    defender = candidate.defender_actor;
    result.primary_stream_before = candidate.play_state.stream_offset[primary];
    result.primary_state_before = candidate.play_state.actor_state[primary];
    result.defender_stream_before = candidate.play_state.stream_offset[defender];
    result.defender_state_before = candidate.play_state.actor_state[defender];
    result.primary_automatic = candidate.control_mode[candidate.offense_side] != 0U;
    result.defender_automatic = candidate.control_mode[candidate.defense_side] != 0U;
    if (result.primary_automatic) {
        candidate.play_state.stream_offset[primary] = 0x000AU;
        candidate.last_step_offset[primary] = 0x000AU;
        candidate.play_state.actor_state[primary] = 0x04U;
        result.unsupported_primary_046e18_observed = true;
        result.unsupported_clear_0484_048f_observed = true;
    }
    if (result.defender_automatic) {
        candidate.play_state.stream_offset[defender] = 0x0019U;
        candidate.last_step_offset[defender] = 0x0019U;
        candidate.play_state.actor_state[defender] = 0x04U;
        result.unsupported_clear_0484_048f_observed = true;
    }
    scan_side(&candidate, &input->object10_target, 0x10U,
              &result.side10_scan, &candidate);
    scan_side(&candidate, &input->object10_target, 0x00U,
              &result.side00_scan, &candidate);
    result.unsupported_c711_action1d_observed =
        result.side10_scan.winner_actor != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        result.side00_scan.winner_actor != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    result.unsupported_terminal_9df6_scratch_observed = true;
    result.primary_stream_after = candidate.play_state.stream_offset[primary];
    result.primary_state_after = candidate.play_state.actor_state[primary];
    result.defender_stream_after = candidate.play_state.stream_offset[defender];
    result.defender_state_after = candidate.play_state.actor_state[defender];
    if (!tecmo_gameplay_live_foundation_valid(steering_assets, &candidate)) {
        return false;
    }
    result.applied = true;
    result.noop_reason = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_NONE;
    *foundation_io = candidate;
    *result_out = result;
    return true;
}

static bool build_self_test_foundation(
    const TecmoGameplayCpuSteeringAssets *steering_assets,
    TecmoGameplayLiveFoundation *foundation)
{
    TecmoGameplayCourtCoordinate positions[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    uint8_t controller_team[TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT] =
        {TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR,
         TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR};
    uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT] =
        {TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR,
         TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR};
    size_t actor;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        positions[actor].x = (int16_t)(80 + actor * 24U);
        positions[actor].y = (int16_t)(70 + actor * 7U);
        actor_team[actor] = actor < 5U ? 0U : 1U;
    }
    if (!tecmo_gameplay_live_foundation_initialize(
            steering_assets, positions, 0U, 0U, 0U, actor_team,
            controller_team, controlled_actor, foundation) ||
        !tecmo_gameplay_live_foundation_synchronize(
            steering_assets, positions, 0U, 0U, 0U, actor_team,
            controller_team, controlled_actor, foundation)) {
        return false;
    }
    return true;
}

bool tecmo_gameplay_actor_command_assignment_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayActorCommandAssignmentAssets assignment_assets;
    TecmoGameplayCpuSteeringAssets steering_assets;
    TecmoGameplayLiveFoundation baseline;
    TecmoGameplayLiveFoundation foundation;
    TecmoGameplayLiveFoundation before;
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayActorCommandAssignmentInput input;
    TecmoGameplayActorCommandAssignmentResult result;
    TecmoGameplayActorCommandAssignmentResult result_before;
    uint8_t *payload = NULL;
    uint8_t *mutated_payload = NULL;
    uint64_t payload_size = 0U;
    uint8_t copied_first_source_byte;
    uint16_t human_primary_stream;
    uint16_t human_defender_stream;
    bool ok = false;
    tecmo_gameplay_actor_command_assignment_assets_init(&assignment_assets);
    tecmo_gameplay_cpu_steering_assets_init(&steering_assets);
    if (!tecmo_gameplay_actor_command_assignment_assets_load(
            &assignment_assets, asset_pack_path) ||
        !tecmo_gameplay_cpu_steering_assets_load(
            &steering_assets, asset_pack_path) ||
        !build_self_test_foundation(&steering_assets, &baseline)) {
        goto cleanup;
    }
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_INPUT_TAG;
    input.caller =
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE10_B73A;
    input.raw_object_state = 0x10U;
    input.raw_0588 = 0x80U;
    input.raw_0067 = 1U;
    input.object10_target_valid = true;
    input.object10_target.x = 152;
    input.object10_target.y = 104;

    /* $A0A6's two selected slots use independent automatic predicates.  The
       initialized side-0 fixture is automatic on both sides, so this checks
       $030C/$030D-shaped source writes without making them a scene event. */
    foundation = baseline;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || !result.applied ||
        result.side10_scan.required_04b0_bit10 != 0x10U ||
        result.side00_scan.required_04b0_bit10 != 0U ||
        !result.primary_automatic || !result.defender_automatic ||
        result.primary_stream_after != 0x000AU ||
        result.defender_stream_after != 0x0019U ||
        result.primary_state_after != 0x04U ||
        result.defender_state_after != 0x04U) {
        goto cleanup;
    }

    /* The same gates close with the possession-side roles reversed.  This
       proves the source's $030C/$030D indexing is by side, not by a fixed
       local/offense slot. */
    foundation = baseline;
    if (!tecmo_gameplay_live_foundation_synchronize(
            &steering_assets, foundation.actor_position, 0U, 1U, 5U,
            foundation.actor_team, foundation.controller_team,
            foundation.last_controlled_actor, &foundation) ||
        foundation.offense_side != 1U || foundation.defense_side != 0U ||
        foundation.primary_actor != 5U || foundation.defender_actor != 0U) {
        goto cleanup;
    }
    input.object10_target = foundation.actor_position[5U];
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || !result.applied || !result.primary_automatic ||
        !result.defender_automatic || result.primary_stream_after != 0x000AU ||
        result.defender_stream_after != 0x0019U) {
        goto cleanup;
    }

    /* Human modes leave A0A6's selected slots alone; $A046 still scans the
       two selector sides.  The selected primary/defender are deliberately
       closest to the target and must nevertheless be excluded. */
    foundation = baseline;
    foundation.control_mode[foundation.offense_side] = 0U;
    foundation.control_mode[foundation.defense_side] = 0U;
    input.object10_target.x = 128;
    input.object10_target.y = 96;
    foundation.actor_position[0U] = input.object10_target;
    foundation.actor_position[5U] = input.object10_target;
    foundation.actor_position[1U].x = 129;
    foundation.actor_position[1U].y = 96;
    foundation.actor_position[6U].x = 129;
    foundation.actor_position[6U].y = 96;
    before = foundation;
    human_primary_stream = before.play_state.stream_offset[before.primary_actor];
    human_defender_stream = before.play_state.stream_offset[before.defender_actor];
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || result.primary_automatic || result.defender_automatic ||
        result.primary_stream_before != result.primary_stream_after ||
        result.defender_stream_before != result.defender_stream_after ||
        foundation.play_state.stream_offset[before.primary_actor] !=
            human_primary_stream ||
        foundation.play_state.stream_offset[before.defender_actor] !=
            human_defender_stream ||
        result.side00_scan.winner_actor == before.primary_actor ||
        result.side10_scan.winner_actor == before.defender_actor ||
        result.side00_scan.winner_actor == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        result.side10_scan.winner_actor == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
        goto cleanup;
    }

    /* Exact descending/equal replacement: slots 8 and 6 tie; 6 wins because
       it is encountered later in the $09..$00 scan. */
    foundation = baseline;
    foundation.actor_position[6U].x = 200;
    foundation.actor_position[6U].y = 100;
    foundation.actor_position[8U].x = 200;
    foundation.actor_position[8U].y = 100;
    input.object10_target.x = 200;
    input.object10_target.y = 100;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || result.side10_scan.winner_actor != 6U ||
        result.side10_scan.winner_score != 0U) {
        goto cleanup;
    }

    /* A046's no-candidate exit is separately testable only with a synthetic
       selector fixture: a valid LIVE foundation always has five actors on
       each selector side.  This directly verifies the scan's no-write exit
       without pretending it is an organic production condition. */
    foundation = baseline;
    memset(foundation.actor_selector_flags, 0x20,
           sizeof(foundation.actor_selector_flags));
    candidate = foundation;
    scan_side(&foundation, &input.object10_target, 0x10U,
              &result.side10_scan, &candidate);
    if (!result.side10_scan.executed || !result.side10_scan.no_candidate ||
        result.side10_scan.winner_actor != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
        memcmp(&candidate, &foundation, sizeof(foundation)) != 0) {
        goto cleanup;
    }

    /* The remaining two source-complete object dispatch callers are fixture
       tests only.  They establish distinct gate families before $A023. */
    foundation = baseline;
    input.caller =
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE17_B783;
    input.raw_object_state = 0x17U;
    input.raw_0499 = 0U;
    input.raw_0588 = 0x20U;
    input.raw_0067 = 0U;
    input.raw_0068 = 0U;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || !result.applied) {
        goto cleanup;
    }
    foundation = baseline;
    input.caller =
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE18_B7B6;
    input.raw_object_state = 0x18U;
    input.raw_0499 = 0U;
    input.raw_0588 = 0U;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || !result.applied) {
        goto cleanup;
    }

    /* Gate and interaction rejections are analytical no-ops; malformed tags
       remain fully transactional with an untouched result output. */
    foundation = before;
    input.caller =
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_OBJECT_STATE10_B73A;
    input.raw_object_state = 0x10U;
    input.raw_0499 = 0U;
    input.raw_0588 = 0U;
    input.raw_0067 = 1U;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || result.applied || result.noop_reason !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_CALLER_GATES ||
        memcmp(&foundation, &before, sizeof(foundation)) != 0) {
        goto cleanup;
    }
    input.caller =
        TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_CALLER_INTERACTION_9FE2;
    if (!tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || result.applied || result.noop_reason !=
            TECMO_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_NOOP_INTERACTION_UNOWNED ||
        memcmp(&foundation, &before, sizeof(foundation)) != 0) {
        goto cleanup;
    }
    memset(&result_before, 0xA5, sizeof(result_before));
    result = result_before;
    input.contract_tag = 0U;
    if (tecmo_gameplay_actor_command_assignment_apply(
            &assignment_assets, &steering_assets, &input, &foundation,
            &result) || memcmp(&foundation, &before, sizeof(foundation)) != 0 ||
        memcmp(&result, &result_before, sizeof(result)) != 0) {
        goto cleanup;
    }
    /* Parser tests distinguish its owned copy from the caller payload, then
       independently reject a copied source byte, descriptor byte, and whole
       payload/header corruption. */
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path,
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE,
            &payload, &payload_size) != 0 ||
        payload == NULL || payload_size !=
            TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SIZE) {
        goto cleanup;
    }
    mutated_payload = (uint8_t *)malloc((size_t)payload_size);
    if (mutated_payload == NULL ||
        !tecmo_gameplay_actor_command_assignment_assets_parse(
            &assignment_assets, payload, (size_t)payload_size) ||
        assignment_assets.sources[0U].bytes == NULL) {
        goto cleanup;
    }
    copied_first_source_byte = assignment_assets.sources[0U].bytes[0U];
    payload[TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET] ^=
        0x01U;
    if (assignment_assets.sources[0U].bytes[0U] != copied_first_source_byte ||
        tecmo_gameplay_actor_command_assignment_assets_parse(
            &assignment_assets, payload, (size_t)payload_size)) {
        goto cleanup;
    }
    payload[TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_RAW_OFFSET] ^=
        0x01U;
    memcpy(mutated_payload, payload, (size_t)payload_size);
    mutated_payload[
        TECMO_ASSET_PACK_GAMEPLAY_ACTOR_COMMAND_ASSIGNMENT_SOURCES_OFFSET +
        16U] ^= 0x01U;
    if (tecmo_gameplay_actor_command_assignment_assets_parse(
            &assignment_assets, mutated_payload, (size_t)payload_size)) {
        goto cleanup;
    }
    memcpy(mutated_payload, payload, (size_t)payload_size);
    mutated_payload[0U] ^= 0x01U;
    if (tecmo_gameplay_actor_command_assignment_assets_parse(
            &assignment_assets, mutated_payload, (size_t)payload_size)) {
        goto cleanup;
    }
    ok = true;

cleanup:
    free(mutated_payload);
    tecmo_asset_pack_free(payload);
    tecmo_gameplay_cpu_steering_assets_destroy(&steering_assets);
    tecmo_gameplay_actor_command_assignment_assets_destroy(&assignment_assets);
    if (!ok) {
        (void)snprintf(message, message_size,
                       "TGCA-1 focused command-assignment contract failed.");
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGCA-1 focused command-assignment contract passed.");
    return true;
}
