#include "tecmo_gameplay_live_foundation.h"

#include <limits.h>
#include <string.h>

static const uint8_t live_fixed_link[
    TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT] = {
    5U, 6U, 7U, 8U, 9U, 0U, 1U, 2U, 3U, 4U
};

/* Bank05 $B98B: receiver/candidate remap used by $B95A/$B96D. */
static const uint8_t live_b87c_candidate_remap[
    TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {
    1U, 2U, 3U, 4U, 0U, 6U, 7U, 8U, 9U, 5U
};

/* These adapter observation counters are not gameplay timers.  They use an
   explicit modulo-2^32 contract so a long-running session does not acquire a
   periodic failure at UINT32_MAX.  The accepted CPU play state separately
   retains its source uint16_t step_serial wrap behavior. */
static uint32_t live_serial_next(uint32_t serial)
{
    return serial == UINT32_MAX ? 0U : serial + 1U;
}

static bool live_actor_positions_valid(
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT])
{
    size_t actor;
    if (actor_position == NULL) return false;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (!tecmo_gameplay_court_coordinate_valid(&actor_position[actor])) {
            return false;
        }
    }
    return true;
}

static bool live_actor_team_valid(
    const uint8_t actor_team[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT])
{
    size_t actor;
    if (actor_team == NULL) return false;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        uint8_t expected = actor <
                TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT
            ? 0U : 1U;
        if (actor_team[actor] != expected) return false;
    }
    return true;
}

/* LIVE owns only Bank05's $04B0 bit-$10 selector predicate. Rejecting any
 * other bit keeps the typed mirror from pretending it carries the rest of
 * the opaque object flag byte. The generic synchronizer establishes the
 * current-side/other-side relationship before a claimant transaction. */
static bool live_selector_flags_valid(
    const TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    if (foundation == NULL || foundation->last_possession >=
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        uint8_t expected = foundation->actor_team[actor] ==
                foundation->last_possession
            ? 0U : 0x10U;
        if (foundation->actor_selector_flags[actor] != expected) {
            return false;
        }
    }
    return true;
}

static bool live_controller_routing_valid(
    const uint8_t actor_team[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT])
{
    size_t controller;
    if (actor_team == NULL || controller_team == NULL ||
        controlled_actor == NULL) {
        return false;
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT;
         ++controller) {
        uint8_t controlled = controlled_actor[controller];
        uint8_t team = controller_team[controller];
        if (team != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
            team >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) {
            return false;
        }
        if (team != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            for (size_t previous = 0U; previous < controller; ++previous) {
                if (controller_team[previous] == team) return false;
            }
        }
        if (team == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
            if (controlled != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
                return false;
            }
        } else if (controlled >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
                   actor_team[controlled] != team) {
            return false;
        }
    }
    return true;
}

static bool live_selection_valid(
    uint8_t orientation,
    uint8_t possession,
    uint8_t ball_holder,
    const uint8_t actor_team[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT])
{
    if (orientation >= TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT ||
        possession >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        ball_holder >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        !live_actor_team_valid(actor_team) || controller_team == NULL ||
        controlled_actor == NULL) {
        return false;
    }
    if (actor_team[ball_holder] != possession) return false;
    return live_controller_routing_valid(
        actor_team, controller_team, controlled_actor);
}

static bool live_stream_offset_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint16_t offset)
{
    TecmoGameplayCpuSteeringCommand command;
    return assets != NULL &&
           tecmo_gameplay_cpu_steering_decode_command(
               assets, offset, &command);
}

static bool live_opcode15_trace_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayLiveOpcode15Trace *trace)
{
    TecmoGameplayCpuSteeringCommand command;
    if (assets == NULL || trace == NULL ||
        trace->contract_tag != TECMO_GAMEPLAY_LIVE_OPCODE15_TRACE_TAG) {
        return false;
    }
    if (!trace->observed) {
        return trace->branch ==
                   TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_NONE &&
               trace->missing_raw_mask == 0U && trace->opcode == 0U &&
               !trace->raw_0499_available && !trace->raw_04b0_available &&
               !trace->raw_007e_available &&
               !trace->raw_06d5_06d6_available &&
               !trace->raw_0479_available &&
               !trace->raw_0442_044d_available &&
               !trace->raw_059e_available &&
               !trace->raw_actor_lifecycle_available;
    }
    if (trace->opcode != 15U ||
        trace->branch !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW ||
        trace->missing_raw_mask !=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK ||
        trace->actor_x >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        trace->raw_0308_before >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        trace->raw_0308_after >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        trace->raw_0309_before >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        trace->raw_0309_after >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        trace->raw_0308_before != trace->raw_0308_after ||
        trace->raw_0309_before != trace->raw_0309_after ||
        trace->actor_stream_before != trace->actor_stream_after ||
        trace->actor_state_before != trace->actor_state_after ||
        trace->raw_0499_available || trace->raw_04b0_available ||
        trace->raw_007e_available || trace->raw_06d5_06d6_available ||
        trace->raw_0479_available || trace->raw_0442_044d_available ||
        trace->raw_059e_available || trace->raw_actor_lifecycle_available ||
        !tecmo_gameplay_cpu_steering_decode_command(
            assets, trace->command_record_offset, &command) ||
        command.opcode != 15U || command.handler_cpu != 0x9172U) {
        return false;
    }
    return true;
}

static bool live_formation_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t formation_index,
    const TecmoGameplayCpuSteeringPlayState *play_state)
{
    TecmoGameplayCpuSteeringFormationResult formation;
    size_t actor;
    if (assets == NULL || play_state == NULL ||
        formation_index >=
            TECMO_GAMEPLAY_LIVE_FOUNDATION_FORMATION_PINNED_LIMIT ||
        assets->formation_source_pinned_count !=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT ||
        !tecmo_gameplay_cpu_steering_formation_select(
            assets, formation_index, &formation) ||
        formation.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_RESULT_TAG ||
        !formation.source_pinned ||
        formation.actor_count != TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (!live_stream_offset_valid(
                assets, play_state->stream_offset[actor])) {
            return false;
        }
    }
    return true;
}

static bool live_target_fields_valid(
    const TecmoGameplayLiveFoundation *foundation,
    size_t actor)
{
    uint8_t target_object;
    TecmoGameplayCourtCoordinate target;
    if (foundation == NULL || actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    target_object = foundation->play_state.target_object[actor];
    if (target_object != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
        target_object >= TECMO_GAMEPLAY_CPU_STEERING_OBJECT_COUNT) {
        return false;
    }
    target.x = foundation->play_state.target_x[actor];
    target.y = foundation->play_state.target_depth[actor];
    if (foundation->source_target_valid[actor] &&
        foundation->source_raw_target_valid[actor]) {
        return false;
    }
    if (foundation->source_raw_target_valid[actor]) {
        /* `$038D-$0390` is a pair of raw 16-bit latch words. Preserve the
           exact stored bits, but never expose them as an in-court target. */
        return target_object == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    }
    if (foundation->source_target_valid[actor]) {
        /* A source object-target write carries both a referenced object slot
           and a source-recorded coordinate. Slots 0..9 follow the current
           referenced player; slot 10 follows the typed canonical ball
           coordinate on the immutable post-human snapshot/tick. Original
           Bank05 dynamic retarget/matchup semantics remain incomplete. */
        return tecmo_gameplay_court_coordinate_valid(&target);
    }
    /* Zero is the accepted uninitialized target sentinel. A deferred source
       effect may also preserve a previously validated target, handled above. */
    return target_object == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
           target.x == 0 && target.y == 0;
}

static bool live_play_state_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    if (assets == NULL || foundation == NULL ||
        foundation->contract_tag != TECMO_GAMEPLAY_LIVE_FOUNDATION_TAG ||
        !foundation->state_valid || !foundation->initialized ||
        !foundation->formation_source_pinned ||
        !foundation->fixed_link_projection_active ||
        !foundation->workspace_native_approximation ||
        !foundation->shot_request_native_approximation ||
        foundation->formation_index >=
            TECMO_GAMEPLAY_LIVE_FOUNDATION_FORMATION_PINNED_LIMIT ||
        foundation->orientation >=
            TECMO_GAMEPLAY_CPU_STEERING_ORIENTATION_COUNT ||
        foundation->play_state.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_PLAY_STATE_TAG ||
        foundation->primary_actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation->defender_actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation->prior_selected_actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation->prior_defender_actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation->offense_side >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        foundation->defense_side >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        foundation->offense_side == foundation->defense_side ||
        foundation->offense_side != foundation->last_possession ||
        foundation->defense_side !=
            (uint8_t)(foundation->last_possession ^ 1U) ||
        foundation->static_primary_seed != 4U ||
        foundation->static_defender_seed != 9U ||
        foundation->last_possession >=
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        foundation->initialization_serial == 0U ||
        !live_actor_team_valid(foundation->actor_team) ||
        !live_selector_flags_valid(foundation) ||
        !live_actor_positions_valid(foundation->actor_position) ||
        !live_opcode15_trace_valid(assets, &foundation->opcode15_trace) ||
        foundation->play_state.matchup_seed[0U] != 2U ||
        foundation->play_state.matchup_seed[1U] != 7U ||
        foundation->play_state.primary_actor != foundation->primary_actor ||
        foundation->play_state.defender_actor != foundation->defender_actor ||
        !live_formation_valid(assets, foundation->formation_index,
                              &foundation->play_state)) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        TecmoGameplayCpuSteeringFormationResult formation;
        if (!tecmo_gameplay_cpu_steering_formation_select(
                assets, foundation->formation_index, &formation) ||
            foundation->formation_start_offset[actor] !=
                formation.stream_offset[actor] ||
            !live_stream_offset_valid(
                assets, foundation->formation_start_offset[actor])) {
            return false;
        }
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT;
         ++actor) {
        if (foundation->selected_actor_by_side[actor] >=
                TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
            foundation->candidate_actor_by_side[actor] >=
                TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
            foundation->actor_team[foundation->selected_actor_by_side[actor]] !=
                actor ||
            foundation->actor_team[foundation->candidate_actor_by_side[actor]] !=
                actor ||
            foundation->candidate_sector_by_side[actor] > 10U) {
            return false;
        }
    }
    if (memcmp(foundation->play_state.fixed_link, live_fixed_link,
               sizeof(live_fixed_link)) != 0 ||
        memcmp(assets->fixed_link, live_fixed_link,
               sizeof(live_fixed_link)) != 0) {
        return false;
    }
    if (foundation->first_sync_pending) {
        if (foundation->last_ball_holder !=
                TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
            foundation->primary_actor != foundation->static_primary_seed ||
            foundation->defender_actor != foundation->static_defender_seed) {
            return false;
        }
    } else if (foundation->last_ball_holder >=
                   TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
               foundation->actor_team[foundation->last_ball_holder] !=
                   foundation->last_possession ||
               foundation->primary_actor != foundation->last_ball_holder ||
               foundation->actor_team[foundation->defender_actor] ==
                   foundation->last_possession) {
        return false;
    }
    if (foundation->last_shot_actor !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
        foundation->last_shot_actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    if (foundation->last_shot_request &&
        foundation->last_shot_actor ==
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR) {
        return false;
    }
    if (!live_controller_routing_valid(
            foundation->actor_team, foundation->controller_team,
            foundation->last_controlled_actor)) {
        return false;
    }
    if ((foundation->last_shot_deferred &&
         (!foundation->last_shot_request ||
          foundation->last_shot_playback_supported)) ||
        (foundation->last_shot_playback_supported &&
         (!foundation->last_shot_request || foundation->last_shot_deferred)) ||
        (!foundation->last_shot_request &&
         (foundation->last_shot_deferred ||
          foundation->last_shot_playback_supported))) {
        return false;
    }
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (foundation->dynamic_link[actor] >=
                TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
            return false;
        }
        if ((foundation->play_state.actor_state[actor] == 0x05U) !=
                foundation->play_state.route_motion[actor].active ||
            (foundation->play_state.route_motion[actor].active &&
             !foundation->source_target_valid[actor]) ||
            !live_target_fields_valid(foundation, actor) ||
            foundation->last_step_offset[actor] !=
                foundation->play_state.stream_offset[actor] ||
            foundation->play_state.fixed_link_target[actor] !=
                foundation->play_state.fixed_link[actor] ||
            (!foundation->source_direction_valid[actor] &&
             (foundation->source_direction[actor] !=
                  TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION ||
              foundation->play_state.direction[actor] !=
                  TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION)) ||
            (foundation->source_direction_valid[actor] &&
             (foundation->source_direction[actor] >=
                  TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT ||
              foundation->play_state.direction[actor] !=
                  foundation->source_direction[actor])) ||
            (foundation->deferred[actor] &&
             (foundation->deferred_reason[actor] ==
                  TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
              foundation->deferred_reason[actor] >=
                  TECMO_GAMEPLAY_CPU_STEERING_DEFER_REASON_COUNT)) ||
            (!foundation->deferred[actor] &&
             foundation->deferred_reason[actor] !=
                 TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE) ||
            foundation->last_effect[actor] >=
                TECMO_GAMEPLAY_CPU_STEERING_EFFECT_COUNT ||
            !live_stream_offset_valid(assets,
                                      foundation->last_step_offset[actor])) {
            return false;
        }
    }
    return true;
}

void tecmo_gameplay_live_foundation_init(
    TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    if (foundation == NULL) return;
    memset(foundation, 0, sizeof(*foundation));
    foundation->contract_tag = TECMO_GAMEPLAY_LIVE_FOUNDATION_TAG;
    foundation->opcode15_trace.contract_tag =
        TECMO_GAMEPLAY_LIVE_OPCODE15_TRACE_TAG;
    foundation->first_sync_pending = true;
    foundation->last_ball_holder =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    foundation->last_shot_actor =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    foundation->primary_actor =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    foundation->defender_actor =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    foundation->prior_selected_actor = 4U;
    foundation->prior_defender_actor = 9U;
    foundation->offense_side = 0U;
    foundation->defense_side = 1U;
    foundation->selected_actor_by_side[0U] = 4U;
    foundation->selected_actor_by_side[1U] = 9U;
    foundation->candidate_actor_by_side[0U] = 0U;
    foundation->candidate_actor_by_side[1U] = 5U;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        foundation->last_step_offset[actor] = 0U;
        foundation->last_effect[actor] = 0U;
        foundation->source_direction[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        foundation->deferred_reason[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        foundation->actor_team[actor] = actor <
                TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT
            ? 0U : 1U;
        foundation->defender_eligible[actor] = true;
        foundation->dynamic_link[actor] = live_fixed_link[actor];
        foundation->actor_selector_flags[actor] = actor >= 5U ? 0x10U : 0U;
    }
    for (size_t controller = 0U;
         controller < TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT;
         ++controller) {
        foundation->controller_team[controller] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        foundation->last_controlled_actor[controller] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    }
}

bool tecmo_gameplay_live_foundation_valid(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayLiveFoundation *foundation)
{
    return live_play_state_valid(assets, foundation);
}

bool tecmo_gameplay_live_foundation_formation_index_for_coordinate(
    const TecmoGameplayCourtCoordinate *coordinate,
    uint8_t *formation_index_out)
{
    uint8_t x_bucket;
    uint8_t depth_row;
    uint16_t index;
    if (coordinate == NULL || formation_index_out == NULL ||
        !tecmo_gameplay_court_coordinate_valid(coordinate)) {
        return false;
    }
    x_bucket = (uint8_t)((uint16_t)coordinate->x >> 6U);
    depth_row = (uint8_t)((uint16_t)coordinate->y >> 6U);
    if (x_bucket >= 12U || depth_row >= 4U) return false;
    index = (uint16_t)depth_row * 12U + x_bucket;
    if (index >=
            TECMO_GAMEPLAY_CPU_STEERING_FORMATION_SOURCE_PINNED_COUNT) {
        return false;
    }
    *formation_index_out = (uint8_t)index;
    return true;
}

static void live_seed_fixed_link_projection(
    TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    /* Native adapter only: fixed_link is not live $037F/$06CB ownership. */
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        foundation->play_state.fixed_link_target[actor] =
            foundation->play_state.fixed_link[actor];
    }
}

static void live_invalidate_source_metadata_actor(
    TecmoGameplayLiveFoundation *foundation,
    size_t actor)
{
    if (foundation == NULL || actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return;
    }
    /* A state-5 route owns a frozen source target. Any lifecycle transition
       that invalidates that target must cancel the route transaction too;
       retaining its Q6 state would either resume stale motion later or make
       the next route tick fail after integrating against a missing target. */
    if (foundation->play_state.route_motion[actor].active ||
        foundation->play_state.actor_state[actor] == 0x05U) {
        memset(&foundation->play_state.route_motion[actor], 0,
               sizeof(foundation->play_state.route_motion[actor]));
        foundation->play_state.route_motion[actor].contract_tag =
            TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
        foundation->play_state.actor_state[actor] = 0x04U;
    }
    foundation->play_state.target_object[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    foundation->play_state.target_x[actor] = 0;
    foundation->play_state.target_depth[actor] = 0;
    foundation->play_state.direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    foundation->source_target_valid[actor] = false;
    foundation->source_raw_target_valid[actor] = false;
    foundation->source_direction_valid[actor] = false;
    foundation->source_direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    foundation->deferred[actor] = false;
    foundation->deferred_reason[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
}

static void live_invalidate_source_metadata(
    TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    if (foundation == NULL) return;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        live_invalidate_source_metadata_actor(foundation, actor);
    }
}

bool tecmo_gameplay_live_foundation_refresh_formation(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate actor_position[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayCpuSteeringFormationResult formation;
    bool reloaded[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT] = {false};
    uint8_t formation_index;
    size_t actor;
    if (assets == NULL || !assets->available || actor_position == NULL ||
        foundation_io == NULL || !live_play_state_valid(assets, foundation_io) ||
        foundation_io->primary_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        !live_actor_positions_valid(actor_position) ||
        !tecmo_gameplay_live_foundation_formation_index_for_coordinate(
            &actor_position[foundation_io->primary_actor], &formation_index)) {
        return false;
    }
    if (formation_index == foundation_io->formation_index) return true;
    if (!tecmo_gameplay_cpu_steering_formation_select(
            assets, formation_index, &formation) || !formation.source_pinned) {
        return false;
    }
    candidate = *foundation_io;
    candidate.formation_index = formation_index;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        bool postcatch_prior_cursor =
            actor == candidate.prior_selected_actor &&
            candidate.play_state.actor_state[actor] == 0x04U &&
            candidate.play_state.stream_offset[actor] == 0x0B63U;
        candidate.formation_start_offset[actor] = formation.stream_offset[actor];
        /* $944D excludes $0308 and $9452 excludes bit-$10 actors. The
           ordinary actor loop separately excludes $0309, so retain both
           selected actors' current command lifecycle. */
        if (actor != candidate.primary_actor &&
            actor != candidate.defender_actor &&
            !postcatch_prior_cursor &&
            (candidate.actor_selector_flags[actor] & 0x10U) == 0U) {
            candidate.play_state.stream_offset[actor] =
                formation.stream_offset[actor];
            candidate.play_state.actor_state[actor] = 0x04U;
            candidate.last_step_offset[actor] = formation.stream_offset[actor];
            reloaded[actor] = true;
        }
    }
    /* Bank05 $B27B writes the former selected actor's exact `$0B63` cursor
       before the next Bank06 dispatch. A holder-driven formation bucket
       change must not erase that same-call catch result before its first
       eligible opcode-2 step. Only this typed prior-selected/state/cursor
       catch tuple is exempt; once opcode 2 advances to an opcode-8 record,
       ordinary formation-refresh behavior applies again.

       Bank06 $944D does not reload $0308, and $9452 excludes bit-$10
       actors.  Those slots keep their existing stream cursor and their
       previously written $055B/$0566/$0571 movement state; clearing every
       native target here made a selected CPU ball-handler stop at the first
       64-pixel formation boundary.  Reset source metadata only for the
       slots whose command stream this refresh actually replaces. */
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        if (reloaded[actor]) {
            live_invalidate_source_metadata_actor(&candidate, actor);
        }
    }
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_initialize(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    uint8_t orientation,
    uint8_t possession,
    uint8_t ball_holder,
    const uint8_t actor_team[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    TecmoGameplayLiveFoundation *foundation_out)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayCpuSteeringFormationResult formation;
    uint8_t formation_index;
    size_t actor;
    size_t controller;
    tecmo_gameplay_live_foundation_init(&candidate);
    if (assets == NULL || !assets->available || foundation_out == NULL ||
        !live_actor_positions_valid(actor_position) ||
        !live_selection_valid(orientation, possession, ball_holder,
                              actor_team, controller_team,
                              controlled_actor) ||
        !tecmo_gameplay_live_foundation_formation_index_for_coordinate(
            &actor_position[ball_holder], &formation_index) ||
        formation_index >=
            TECMO_GAMEPLAY_LIVE_FOUNDATION_FORMATION_PINNED_LIMIT ||
        !tecmo_gameplay_cpu_steering_formation_select(
            assets, formation_index, &formation) ||
        !formation.source_pinned) {
        return false;
    }
    if (!tecmo_gameplay_cpu_steering_play_state_initialize(
            assets, formation_index, &candidate.play_state)) {
        return false;
    }
    candidate.state_valid = true;
    candidate.initialized = true;
    candidate.formation_source_pinned = true;
    candidate.fixed_link_projection_active = true;
    candidate.workspace_native_approximation = true;
    candidate.shot_request_native_approximation = true;
    candidate.formation_index = formation_index;
    candidate.orientation = orientation;
    candidate.static_primary_seed = candidate.play_state.primary_actor;
    candidate.static_defender_seed = candidate.play_state.defender_actor;
    candidate.primary_actor = candidate.static_primary_seed;
    candidate.defender_actor = candidate.static_defender_seed;
    candidate.last_possession = possession;
    candidate.offense_side = possession;
    candidate.defense_side = (uint8_t)(possession ^ 1U);
    candidate.control_mode[0U] = 1U;
    candidate.control_mode[1U] = 1U;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        candidate.actor_team[actor] = actor_team[actor];
        candidate.actor_selector_flags[actor] =
            actor_team[actor] == possession ? 0U : 0x10U;
        candidate.actor_position[actor] = actor_position[actor];
        candidate.formation_start_offset[actor] =
            formation.stream_offset[actor];
        candidate.last_step_offset[actor] =
            candidate.play_state.stream_offset[actor];
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT;
         ++controller) {
        candidate.last_controlled_actor[controller] =
            controlled_actor[controller];
        candidate.controller_team[controller] = controller_team[controller];
        if (controller_team[controller] <
                TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT) {
            candidate.control_mode[controller_team[controller]] = 0U;
        }
    }
    candidate.selected_defender_handoff_active =
        candidate.control_mode[candidate.defense_side] != 0U;
    candidate.initialization_serial = 1U;
    live_seed_fixed_link_projection(&candidate);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_out = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_synchronize(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate
        actor_position[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    uint8_t orientation,
    uint8_t possession,
    uint8_t ball_holder,
    const uint8_t actor_team[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT],
    const uint8_t controller_team[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    const uint8_t controlled_actor[
        TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT],
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    bool changed;
    bool possession_changed;
    bool holder_changed;
    size_t actor;
    size_t controller;
    if (assets == NULL || !assets->available || foundation_io == NULL ||
        !live_play_state_valid(assets, foundation_io) ||
        !live_actor_positions_valid(actor_position) ||
        !live_selection_valid(orientation, possession, ball_holder,
                              actor_team, controller_team,
                              controlled_actor)) {
        return false;
    }
    candidate = *foundation_io;
    possession_changed = candidate.last_possession != possession;
    holder_changed = candidate.last_ball_holder != ball_holder;
    changed = candidate.first_sync_pending ||
        candidate.orientation != orientation ||
        candidate.last_possession != possession ||
        candidate.last_ball_holder != ball_holder;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (candidate.actor_team[actor] != actor_team[actor]) changed = true;
        candidate.actor_team[actor] = actor_team[actor];
        candidate.actor_selector_flags[actor] =
            actor_team[actor] == possession ? 0U : 0x10U;
        candidate.actor_position[actor] = actor_position[actor];
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CPU_STEERING_CONTROLLER_SLOT_COUNT;
         ++controller) {
        if (candidate.last_controlled_actor[controller] !=
                controlled_actor[controller] ||
            candidate.controller_team[controller] !=
                controller_team[controller]) {
            changed = true;
        }
        candidate.last_controlled_actor[controller] =
            controlled_actor[controller];
        candidate.controller_team[controller] = controller_team[controller];
    }
    candidate.orientation = orientation;
    candidate.last_possession = possession;
    candidate.last_ball_holder = ball_holder;
    if (changed) {
        bool seed_selection = candidate.first_sync_pending ||
            possession_changed;
        candidate.sync_serial = live_serial_next(candidate.sync_serial);
        candidate.play_state.primary_actor = ball_holder;
        candidate.primary_actor = ball_holder;
        if (seed_selection) {
            candidate.defender_actor =
                candidate.play_state.fixed_link[ball_holder];
            candidate.play_state.defender_actor =
                candidate.defender_actor;
            candidate.selected_defender_handoff_active =
                candidate.control_mode[candidate.defense_side] != 0U;
            candidate.offense_side = possession;
            candidate.defense_side = (uint8_t)(possession ^ 1U);
            candidate.selected_actor_by_side[candidate.offense_side] =
                ball_holder;
            candidate.selected_actor_by_side[candidate.defense_side] =
                candidate.defender_actor;
            candidate.candidate_actor_by_side[candidate.offense_side] =
                (uint8_t)(possession * 5U +
                    ((ball_holder % 5U + 1U) % 5U));
            candidate.candidate_actor_by_side[candidate.defense_side] =
                (uint8_t)(candidate.defense_side * 5U +
                    ((candidate.defender_actor % 5U + 1U) % 5U));
        } else if (holder_changed) {
            candidate.selected_actor_by_side[candidate.offense_side] =
                ball_holder;
            candidate.candidate_actor_by_side[candidate.offense_side] =
                (uint8_t)(candidate.offense_side * 5U +
                    ((ball_holder % 5U + 1U) % 5U));
        }
        candidate.play_state.defender_actor = candidate.defender_actor;
        candidate.first_sync_pending = false;
        /* Holder/orientation/controller changes invalidate command-derived
           targets and directions. Bank05 reset/swap semantics are incomplete;
           retaining old writes across a real role transition would fabricate
           continuity. This is a native-faithful safety policy. */
        live_invalidate_source_metadata(&candidate);
    }
    live_seed_fixed_link_projection(&candidate);
    candidate.fixed_link_projection_active = true;
    if (!tecmo_gameplay_live_foundation_refresh_formation(
            assets, actor_position, &candidate)) {
        return false;
    }
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_score_restart_transition(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t resulting_possession,
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    uint8_t old_primary;
    uint8_t old_defender;
    uint8_t old_offense;
    uint8_t old_defense;
    size_t actor;
    if (assets == NULL || !assets->available || foundation_io == NULL ||
        !live_play_state_valid(assets, foundation_io) ||
        resulting_possession >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        resulting_possession == foundation_io->last_possession ||
        resulting_possession != foundation_io->defense_side) {
        return false;
    }
    candidate = *foundation_io;
    old_primary = candidate.primary_actor;
    old_defender = candidate.defender_actor;
    old_offense = candidate.offense_side;
    old_defense = candidate.defense_side;
    if (old_primary >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        old_defender >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        old_primary == old_defender ||
        candidate.actor_team[old_primary] != old_offense ||
        candidate.actor_team[old_defender] != old_defense) {
        return false;
    }

    /* Accepted $8FB9-$8FE7 swaps $030A/$030B and $0308/$0309. $8FE8-
       $902D then clears $057C/$046E and restores packed $0458=$30 for both
       selected actors. Route/target metadata has no independent scene owner;
       cancel it as the typed consequence of clearing an active selected
       lifecycle, rather than allowing a C-only state-5 route to survive a
       native state-zero reset. */
    live_invalidate_source_metadata_actor(&candidate, old_primary);
    live_invalidate_source_metadata_actor(&candidate, old_defender);
    candidate.offense_side = old_defense;
    candidate.defense_side = old_offense;
    candidate.primary_actor = old_defender;
    candidate.play_state.primary_actor = old_defender;
    candidate.defender_actor = old_primary;
    candidate.play_state.defender_actor = old_primary;
    candidate.last_possession = resulting_possession;
    candidate.last_ball_holder = old_defender;
    candidate.first_sync_pending = false;
    candidate.prior_selected_actor = old_primary;
    candidate.prior_defender_actor = old_defender;
    candidate.play_state.actor_state[old_primary] = 0U;
    candidate.play_state.actor_state[old_defender] = 0U;
    candidate.play_state.action_state_046e[old_primary] = 0U;
    candidate.play_state.action_state_046e[old_defender] = 0U;
    candidate.play_state.action[old_primary] = 0x30U;
    candidate.play_state.action[old_defender] = 0x30U;
    candidate.play_state.wait_counter[old_primary] = 0U;
    candidate.play_state.wait_counter[old_defender] = 0U;

    /* $9042-$9053 toggles $04B0 bit $10 for all slots 9..0. The loop order
       has no observable alias in this typed array, but the complete mask
       result is exact. */
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        candidate.actor_selector_flags[actor] ^= 0x10U;
    }
    candidate.selected_actor_by_side[candidate.offense_side] = old_defender;
    candidate.candidate_actor_by_side[candidate.offense_side] =
        live_b87c_candidate_remap[old_defender];
    candidate.selected_actor_by_side[candidate.defense_side] = old_primary;
    candidate.selected_defender_handoff_active =
        candidate.control_mode[candidate.defense_side] != 0U;

    /* Bank07's score restart reaches Bank06 $9621 before ordinary live AI.
       Its leading exact writes clear only the aggregation count/mask; retain
       the threshold byte until a later command replaces it. */
    candidate.play_state.aggregation_06df = 0U;
    candidate.play_state.aggregation_06e1 = 0U;
    candidate.play_state.global_0790 = 0U;
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    live_seed_fixed_link_projection(&candidate);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_normalize_automatic_primary(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t selected_actor,
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    if (assets == NULL || !assets->available || foundation_io == NULL ||
        !live_play_state_valid(assets, foundation_io) ||
        foundation_io->first_sync_pending ||
        selected_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation_io->primary_actor != selected_actor ||
        foundation_io->last_ball_holder != selected_actor ||
        foundation_io->actor_team[selected_actor] !=
            foundation_io->offense_side ||
        foundation_io->control_mode[foundation_io->offense_side] == 0U) {
        return false;
    }
    candidate = *foundation_io;
    live_invalidate_source_metadata_actor(&candidate, selected_actor);

    /* Static source proves that every closed automatic promotion path seeds
       a selected command stream/state instead of retaining an ordinary
       formation cursor. The generic no-claimant miss path is not one of
       those closed callers. Use the source-valid $007D/state4/action18 tuple
       solely as a bounded compatibility normalization; this is explicitly
       not a fabricated $B87C claimant transaction. */
    candidate.play_state.stream_offset[selected_actor] = 0x007DU;
    candidate.last_step_offset[selected_actor] = 0x007DU;
    candidate.play_state.actor_state[selected_actor] = 0x04U;
    candidate.play_state.action_state_046e[selected_actor] = 0x18U;
    candidate.play_state.wait_counter[selected_actor] = 0U;
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_pass_handoff(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t new_selected_actor,
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    uint8_t old_selected;
    uint8_t old_defender;
    uint8_t opposing_team;
    int actor;
    if (assets == NULL || foundation_io == NULL ||
        !live_play_state_valid(assets, foundation_io) ||
        new_selected_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        foundation_io->actor_team[new_selected_actor] !=
            foundation_io->last_possession) {
        return false;
    }
    candidate = *foundation_io;
    old_selected = candidate.primary_actor;
    old_defender = candidate.defender_actor;
    opposing_team = (uint8_t)(candidate.last_possession ^ 1U);

    /* The catch changes selected roles, so no pre-catch source target or
       active state-5 route may survive it.  Do this before installing the
       exact catch states below: source invalidation normalizes an active
       route to state 4, while Bank05 $B24F explicitly clears the receiver to
       state 0 before its same-call continuation. */
    live_invalidate_source_metadata(&candidate);

    /* $B24F selects/initializes the receiver. $B27B restores the former
       selected actor to Bank06 state 4 at command $9F2E+$0B63=$AA91. */
    candidate.prior_selected_actor = old_selected;
    candidate.primary_actor = new_selected_actor;
    candidate.play_state.primary_actor = new_selected_actor;
    candidate.last_ball_holder = new_selected_actor;
    candidate.selected_actor_by_side[candidate.offense_side] =
        new_selected_actor;
    candidate.candidate_actor_by_side[candidate.offense_side] = old_selected;
    candidate.play_state.actor_state[new_selected_actor] = 0U;
    candidate.play_state.wait_counter[new_selected_actor] = 0U;
    candidate.play_state.action[new_selected_actor] = 0U;
    candidate.play_state.action_state_046e[new_selected_actor] = 0U;
    candidate.play_state.actor_state[old_selected] = 4U;
    candidate.play_state.action_state_046e[old_selected] = 0U;
    candidate.play_state.stream_offset[old_selected] = 0x0B63U;
    candidate.last_step_offset[old_selected] = 0x0B63U;

    /* Bank05 $B2EC jumps directly to $96B6 after the catch work above.
       Nonzero raw $030C[$030A] (automatic offense) must leave $96B6-$9708
       with the new holder in state 4 and one of the two source-pinned streams
       $007D/$00D7. LIVE owns that automatic-vs-human distinction through
       typed control_mode, but it does not own the same-call $0373/$0095/$0094
       inputs that select between those streams. Choose the source-valid long
       route as a justified native approximation: unlike the short route's
       wait/gate sequence, its first command supplies the exact absolute
       movement target without fabricating missing gate workspaces. Human
       catches correctly retain the exact state-0 endpoint above. */
    if (candidate.control_mode[candidate.offense_side] != 0U) {
        candidate.play_state.stream_offset[new_selected_actor] = 0x00D7U;
        candidate.last_step_offset[new_selected_actor] = 0x00D7U;
        candidate.play_state.actor_state[new_selected_actor] = 0x04U;
        /* $96B6 writes selected action $18 before the route branch. */
        candidate.play_state.action_state_046e[new_selected_actor] = 0x18U;
    }

    if (candidate.control_mode[opposing_team] != 0U) {
        uint8_t found = TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        /* $B317 scans X=9..0 and requires both $04B0 bit $10 and $06CB. */
        for (actor = 9; actor >= 0; --actor) {
            if (candidate.defender_eligible[actor] &&
                candidate.dynamic_link[actor] == new_selected_actor) {
                found = (uint8_t)actor;
                break;
            }
        }
        if (found == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR ||
            candidate.actor_team[found] != opposing_team) {
            return false;
        }
        candidate.prior_defender_actor = old_defender;
        candidate.defender_actor = found;
        candidate.play_state.defender_actor = found;
        candidate.selected_actor_by_side[candidate.defense_side] = found;
        candidate.selected_defender_handoff_active = true;
        candidate.play_state.action_state_046e[found] = 0U;
        candidate.play_state.action_state_046e[old_defender] = 0U;
    } else {
        candidate.selected_defender_handoff_active = false;
    }
    live_seed_fixed_link_projection(&candidate);
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_pass_launch_lock(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t receiver_actor,
    TecmoGameplayLiveFoundation *foundation_io)
{
    TecmoGameplayLiveFoundation candidate;
    uint8_t offense;
    uint8_t passer;
    if (assets == NULL || foundation_io == NULL ||
        !live_play_state_valid(assets, foundation_io) ||
        foundation_io->first_sync_pending ||
        receiver_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    candidate = *foundation_io;
    offense = candidate.offense_side;
    passer = candidate.primary_actor;
    if (candidate.last_ball_holder != passer ||
        candidate.selected_actor_by_side[offense] != passer ||
        candidate.candidate_actor_by_side[offense] != receiver_actor ||
        receiver_actor == passer ||
        candidate.actor_team[receiver_actor] != offense) {
        return false;
    }

    /* Bank05 $B0ED-$B0FA, inside the shared $B074 launch path, swaps
       $000E[$030A] with $037F[$030A]. The source does not write $0308 here:
       keep primary_actor/last_ball_holder on the passer until $B24F. */
    candidate.selected_actor_by_side[offense] = receiver_actor;
    candidate.candidate_actor_by_side[offense] = passer;
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    return true;
}

bool tecmo_gameplay_live_foundation_claimant_settlement(
    const TecmoGameplayCpuSteeringAssets *assets,
    uint8_t selected_claimant,
    uint8_t resulting_possession,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayLiveClaimantSettlement *result_out)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayLiveClaimantSettlement result;
    uint8_t old_primary;
    uint8_t old_defender;
    bool side_cross;
    int actor;

    if (assets == NULL || !assets->available || foundation_io == NULL ||
        result_out == NULL ||
        selected_claimant >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        resulting_possession >= TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        !live_play_state_valid(assets, foundation_io) ||
        foundation_io->first_sync_pending ||
        foundation_io->actor_team[selected_claimant] !=
            resulting_possession) {
        return false;
    }

    candidate = *foundation_io;
    old_primary = candidate.primary_actor;
    old_defender = candidate.defender_actor;
    side_cross = selected_claimant != old_primary &&
        (candidate.actor_selector_flags[selected_claimant] & 0x10U) != 0U;

    /* A no-toggle branch must retain the currently selected side. This is the
       typed caller requirement corresponding to $B8C1-$B8C9 rather than an
       inference from a generic possession reset. */
    if ((!side_cross && resulting_possession != candidate.last_possession) ||
        (side_cross && resulting_possession == candidate.last_possession)) {
        return false;
    }

    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG;
    result.raw_0308_before = old_primary;
    result.raw_0309_before = old_defender;
    result.raw_030a_before = candidate.offense_side;
    result.raw_030b_before = candidate.defense_side;
    result.candidate_replaced_primary = selected_claimant != old_primary;

    /* $B87C saves $0308/$0309 before promoting $9C to $0308. LIVE's
       prior_* fields are typed observations of those saved selections; the
       later $1D action dispatch itself has no faithful LIVE owner. */
    candidate.prior_selected_actor = old_primary;
    candidate.prior_defender_actor = old_defender;
    candidate.primary_actor = selected_claimant;
    candidate.play_state.primary_actor = selected_claimant;
    candidate.last_ball_holder = selected_claimant;

    /* Exact $B8C1-$B8F5 predicate/order: only a different candidate whose
       current $04B0 bit-$10 is set turns over side context. $9042 then loops
       X=9..0 and EORs bit-$10 for every slot. $035A->$035B/EOR #1 is observed
       at $B8D8-$B8E0 but has no typed C owner, so it is recorded only. */
    if (side_cross) {
        uint8_t saved_offense = candidate.offense_side;
        candidate.defender_actor = old_primary;
        candidate.play_state.defender_actor = old_primary;
        candidate.last_possession = resulting_possession;
        candidate.offense_side = candidate.defense_side;
        candidate.defense_side = saved_offense;
        for (actor = 0;
             actor < (int)TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
            candidate.actor_selector_flags[actor] ^= 0x10U;
        }
        result.side_context_swapped = true;
        result.raw_04b0_bit10_toggled = true;
        result.raw_035a_save_and_toggle_observed = true;
    }

    /* $B8F6-$B918 runs only when $030C[$030B] is nonzero. The exact scan is
       descending 9..0 and tests *only* $04B0 bit-$10 plus $06CB==$0308;
       defender_eligible is a separate legacy adapter and is intentionally not
       consulted here. No match leaves the existing $0309 selection intact. */
    if (candidate.control_mode[candidate.defense_side] != 0U) {
        result.automatic_defender_scan_ran = true;
        for (actor = (int)TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1;
             actor >= 0; --actor) {
            if ((candidate.actor_selector_flags[actor] & 0x10U) != 0U &&
                candidate.dynamic_link[actor] == selected_claimant) {
                candidate.defender_actor = (uint8_t)actor;
                candidate.play_state.defender_actor = (uint8_t)actor;
                result.automatic_defender_match_found = true;
                break;
            }
        }
    }

    /* $B928/$B94F update the selected side and $B95A uses the exact $B98B
       remap. $B93B-$B95E writes stream $007D and state $04 only for automatic
       offense. No actor state/stream reset is invented for human offense. */
    candidate.selected_actor_by_side[candidate.offense_side] =
        selected_claimant;
    candidate.candidate_actor_by_side[candidate.offense_side] =
        live_b87c_candidate_remap[selected_claimant];
    if (candidate.control_mode[candidate.offense_side] != 0U) {
        candidate.play_state.stream_offset[selected_claimant] = 0x007DU;
        candidate.play_state.actor_state[selected_claimant] = 0x04U;
        candidate.last_step_offset[selected_claimant] = 0x007DU;
    }
    candidate.selected_actor_by_side[candidate.defense_side] =
        candidate.defender_actor;
    candidate.selected_defender_handoff_active =
        candidate.control_mode[candidate.defense_side] != 0U;

    /* sync_serial is a LIVE adapter observation counter, not a source timer.
       It gives opt-in diagnostics a stable transaction edge without changing
       gameplay timing or claiming a $B87C byte owner for the counter. */
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    result.raw_0308_after = candidate.primary_actor;
    result.raw_0309_after = candidate.defender_actor;
    result.raw_030a_after = candidate.offense_side;
    result.raw_030b_after = candidate.defense_side;

    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_live_foundation_play_step(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringPlayInput *input,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayCpuSteeringPlayResult *result_out)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayCpuSteeringPlayState next_state;
    TecmoGameplayCpuSteeringPlayResult result;
    uint8_t actor;
    bool validated_target_write = false;
    if (assets == NULL || !assets->available || input == NULL ||
        foundation_io == NULL || result_out == NULL ||
        input->step_budget != 1U ||
        !live_play_state_valid(assets, foundation_io) ||
        input->actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        !tecmo_gameplay_cpu_steering_play_step(
            assets, &foundation_io->play_state, input, &next_state,
            &result) || result.contract_tag !=
            TECMO_GAMEPLAY_CPU_STEERING_PLAY_RESULT_TAG ||
        result.actor != input->actor || result.step_budget != 1U ||
        result.steps_executed > 1U ||
        !live_stream_offset_valid(assets, result.next_offset)) {
        return false;
    }
    candidate = *foundation_io;
    actor = input->actor;
    candidate.tick_serial = live_serial_next(candidate.tick_serial);
    candidate.play_state = next_state;
    candidate.last_step_offset[actor] = result.next_offset;
    candidate.last_effect[actor] = (uint8_t)result.effect;
    candidate.deferred[actor] = result.deferred;
    candidate.deferred_reason[actor] = result.deferred_reason;
    if (result.fetched && result.command.opcode == 15U) {
        TecmoGameplayLiveOpcode15Trace *trace =
            &candidate.opcode15_trace;
        /* LIVE deliberately has no faithful raw owner for the Bank06 gate
           plane or its post-swap registers. Record unavailable inputs rather
           than treating typed scene values or caller constants as RAM. */
        memset(trace, 0, sizeof(*trace));
        trace->contract_tag = TECMO_GAMEPLAY_LIVE_OPCODE15_TRACE_TAG;
        trace->observed = true;
        trace->opcode = result.command.opcode;
        trace->actor_x = actor;
        trace->command_record_offset = result.command.stream_offset;
        trace->branch =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_BRANCH_DEFERRED_MISSING_RAW;
        trace->missing_raw_mask =
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE15_RAW_KNOWN_MASK;
        trace->raw_0308_before = foundation_io->primary_actor;
        trace->raw_0308_after = candidate.primary_actor;
        trace->raw_0309_before = foundation_io->defender_actor;
        trace->raw_0309_after = candidate.defender_actor;
        trace->actor_stream_before =
            foundation_io->play_state.stream_offset[actor];
        trace->actor_stream_after = next_state.stream_offset[actor];
        trace->actor_state_before =
            foundation_io->play_state.actor_state[actor];
        trace->actor_state_after = next_state.actor_state[actor];
    }
    if (result.fetched && !result.deferred &&
        (result.command.opcode == 0U || result.command.opcode == 2U ||
         result.command.opcode == 4U || result.command.opcode == 10U ||
         result.command.opcode == 13U || result.command.opcode == 16U)) {
        TecmoGameplayCourtCoordinate target = {
            next_state.target_x[actor], next_state.target_depth[actor]};
        candidate.source_target_valid[actor] = false;
        candidate.source_raw_target_valid[actor] = false;
        if (result.command.opcode == 13U) {
            validated_target_write =
                result.raw_target_valid &&
                next_state.target_object[actor] ==
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
                (uint16_t)next_state.target_x[actor] == result.raw_target_x &&
                (uint16_t)next_state.target_depth[actor] ==
                    result.raw_target_depth;
            candidate.source_raw_target_valid[actor] =
                validated_target_write;
        } else if (result.command.opcode == 4U ||
            result.command.opcode == 10U ||
            result.command.opcode == 16U) {
            validated_target_write =
                next_state.target_object[actor] <
                    TECMO_GAMEPLAY_CPU_STEERING_OBJECT_COUNT &&
                tecmo_gameplay_court_coordinate_valid(&target);
        } else {
            validated_target_write =
                next_state.target_object[actor] ==
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
                tecmo_gameplay_court_coordinate_valid(&target);
        }
    }
    if (validated_target_write && result.command.opcode != 13U) {
        candidate.source_target_valid[actor] = true;
    }
    if (next_state.direction[actor] !=
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION) {
        candidate.source_direction_valid[actor] = true;
        candidate.source_direction[actor] = next_state.direction[actor];
    }
    candidate.state_valid = true;
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    *result_out = result;
    return true;
}

bool tecmo_gameplay_live_foundation_shot_request(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCpuSteeringShotInput *input,
    uint8_t actor,
    TecmoGameplayLiveFoundation *foundation_io,
    TecmoGameplayCpuSteeringShotResult *result_out)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayCpuSteeringShotResult result;
    if (assets == NULL || !assets->available || input == NULL ||
        foundation_io == NULL || result_out == NULL ||
        actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT ||
        !live_play_state_valid(assets, foundation_io) ||
        !tecmo_gameplay_cpu_steering_shot_request(
            assets, input, &result)) {
        return false;
    }
    candidate = *foundation_io;
    candidate.last_shot_request = result.request;
    candidate.last_shot_actor = actor;
    candidate.last_shot_deferred = false;
    candidate.last_shot_playback_supported = false;
    candidate.shot_request_native_approximation = true;
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
    *result_out = result;
    return true;
}
