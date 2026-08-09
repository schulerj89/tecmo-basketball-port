#include "tecmo_gameplay_live_foundation.h"

#include <limits.h>
#include <string.h>

static const uint8_t live_fixed_link[
    TECMO_GAMEPLAY_CPU_STEERING_FIXED_LINK_COUNT] = {
    5U, 6U, 7U, 8U, 9U, 0U, 1U, 2U, 3U, 4U
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
    uint8_t target_actor;
    TecmoGameplayCourtCoordinate target;
    if (foundation == NULL || actor >=
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    target_actor = foundation->play_state.target_actor[actor];
    if (target_actor != TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
        target_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    target.x = foundation->play_state.target_x[actor];
    target.y = foundation->play_state.target_depth[actor];
    if (foundation->source_target_valid[actor]) {
        /* A source actor-target write carries both the referenced slot and a
           source-recorded coordinate. The coordinate is required evidence;
           the native adapter follows the current referenced actor on every
           immutable post-human snapshot/tick. Original Bank05 dynamic
           retarget/matchup semantics remain incomplete/unproven. */
        return tecmo_gameplay_court_coordinate_valid(&target);
    }
    /* Zero is the accepted uninitialized target sentinel. A deferred source
       effect may also preserve a previously validated target, handled above. */
    return target_actor == TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
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
        !foundation->native_matchup_inferred ||
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
        foundation->static_primary_seed != 4U ||
        foundation->static_defender_seed != 9U ||
        foundation->last_possession >=
            TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT ||
        foundation->initialization_serial == 0U ||
        !live_actor_team_valid(foundation->actor_team) ||
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
        if (!live_target_fields_valid(foundation, actor) ||
            foundation->last_step_offset[actor] !=
                foundation->play_state.stream_offset[actor] ||
            foundation->play_state.native_matchup_actor[actor] !=
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
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        foundation->last_step_offset[actor] = 0U;
        foundation->last_effect[actor] = 0U;
        foundation->source_direction[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        foundation->actor_team[actor] = actor <
                TECMO_GAMEPLAY_CPU_STEERING_TEAM_ACTOR_COUNT
            ? 0U : 1U;
        foundation->defender_eligible[actor] = true;
        foundation->dynamic_link[actor] = live_fixed_link[actor];
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

static void live_seed_native_matchup(
    TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        foundation->play_state.native_matchup_actor[actor] =
            foundation->play_state.fixed_link[actor];
    }
}

static void live_invalidate_source_metadata(
    TecmoGameplayLiveFoundation *foundation)
{
    size_t actor;
    if (foundation == NULL) return;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        foundation->play_state.target_actor[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        foundation->play_state.target_x[actor] = 0;
        foundation->play_state.target_depth[actor] = 0;
        foundation->play_state.direction[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        foundation->source_target_valid[actor] = false;
        foundation->source_direction_valid[actor] = false;
        foundation->source_direction[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        foundation->deferred[actor] = false;
    }
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
            &actor_position[4U], &formation_index) ||
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
    candidate.native_matchup_inferred = true;
    candidate.workspace_native_approximation = true;
    candidate.shot_request_native_approximation = true;
    candidate.formation_index = formation_index;
    candidate.orientation = orientation;
    candidate.static_primary_seed = candidate.play_state.primary_actor;
    candidate.static_defender_seed = candidate.play_state.defender_actor;
    candidate.primary_actor = candidate.static_primary_seed;
    candidate.defender_actor = candidate.static_defender_seed;
    candidate.last_possession = possession;
    candidate.control_mode[0U] = 1U;
    candidate.control_mode[1U] = 1U;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        candidate.actor_team[actor] = actor_team[actor];
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
    candidate.initialization_serial = 1U;
    live_seed_native_matchup(&candidate);
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
    changed = candidate.first_sync_pending ||
        candidate.orientation != orientation ||
        candidate.last_possession != possession ||
        candidate.last_ball_holder != ball_holder;
    for (actor = 0U;
         actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT; ++actor) {
        if (candidate.actor_team[actor] != actor_team[actor]) changed = true;
        candidate.actor_team[actor] = actor_team[actor];
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
            candidate.selected_defender_handoff_active = false;
        }
        candidate.play_state.defender_actor = candidate.defender_actor;
        candidate.first_sync_pending = false;
        /* Holder/orientation/controller changes invalidate command-derived
           targets and directions. Bank05 reset/swap semantics are incomplete;
           retaining old writes across a real role transition would fabricate
           continuity. This is a native-faithful safety policy. */
        live_invalidate_source_metadata(&candidate);
    }
    live_seed_native_matchup(&candidate);
    candidate.native_matchup_inferred = true;
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

    /* $B24F selects/initializes the receiver. $B27B restores the former
       selected actor to Bank06 state 4 at command $9F2E+$0B63=$AA91. */
    candidate.prior_selected_actor = old_selected;
    candidate.primary_actor = new_selected_actor;
    candidate.play_state.primary_actor = new_selected_actor;
    candidate.last_ball_holder = new_selected_actor;
    candidate.play_state.actor_state[new_selected_actor] = 0U;
    candidate.play_state.timer[new_selected_actor] = 0U;
    candidate.play_state.actor_state[old_selected] = 4U;
    candidate.play_state.timer[old_selected] = 0U;
    candidate.play_state.stream_offset[old_selected] = 0x0B63U;
    candidate.last_step_offset[old_selected] = 0x0B63U;

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
        candidate.selected_defender_handoff_active = true;
        candidate.play_state.timer[found] = 0U;
        candidate.play_state.timer[old_defender] = 0U;
    } else {
        candidate.selected_defender_handoff_active = false;
    }
    live_invalidate_source_metadata(&candidate);
    live_seed_native_matchup(&candidate);
    candidate.sync_serial = live_serial_next(candidate.sync_serial);
    if (!live_play_state_valid(assets, &candidate)) return false;
    *foundation_io = candidate;
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
    if (result.fetched && !result.deferred &&
        (result.command.opcode == 0U || result.command.opcode == 2U ||
         result.command.opcode == 4U)) {
        TecmoGameplayCourtCoordinate target = {
            next_state.target_x[actor], next_state.target_depth[actor]};
        if (result.command.opcode == 4U) {
            validated_target_write =
                next_state.target_actor[actor] <
                    TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT &&
                tecmo_gameplay_court_coordinate_valid(&target);
        } else {
            validated_target_write =
                next_state.target_actor[actor] ==
                    TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR &&
                tecmo_gameplay_court_coordinate_valid(&target);
        }
    }
    if (validated_target_write) candidate.source_target_valid[actor] = true;
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
