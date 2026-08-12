#include "tecmo_gameplay_live_proof.h"

#include "png_writer.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_scene_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIVE_PROOF_WIDTH 640
#define LIVE_PROOF_HEIGHT 480
#define LIVE_PROOF_MEMORY_SIZE (16U * 1024U * 1024U)
#define LIVE_PROOF_MAX_PRETIP_UPDATES 2048U
#define LIVE_PROOF_PRIMARY_STREAM_HOLD_UPDATES 96U
#define LIVE_PROOF_FOUL_VISIBLE_FRAME \
    TECMO_GAMEPLAY_VIOLATION_REFEREE_SEQUENCE_START_FRAME

typedef struct LiveProofEventEvidence {
    bool opcode4_ball_target;
    uint16_t opcode4_record_offset;
    uint8_t opcode4_argument_c8;
    uint8_t opcode4_target_object;
    TecmoGameplayCourtCoordinate opcode4_ball_snapshot;
    TecmoGameplayCourtCoordinate opcode4_source_target;
    bool cpu_primary_stream_stepped;
    uint16_t primary_record_offset;
    uint8_t primary_wait_frames;
    uint16_t primary_stream_before;
    uint16_t primary_stream_after;
    uint16_t primary_last_step_before;
    uint16_t primary_last_step_after;
    uint8_t primary_action_before;
    uint8_t primary_action_after;
    uint16_t primary_action_serial_before;
    uint16_t primary_action_serial_after;
    uint8_t foul_referee_group;
    uint16_t foul_visible_phase_frame;
    bool foul_overlay_retained;
    bool claimant_settlement_executed;
    uint8_t claimant_fixture_launch_frame;
    uint16_t claimant_settlement_updates;
    uint8_t claimant_shooting_actor;
    uint8_t claimant_actor;
    TecmoGameplaySceneClaimantSettlementTrace claimant_settlement;
    /* Bank05 $A023-$A0DC remains a source-shaped deferred diagnostic in the
       production scene.  These fields prove the ordinary pretip-to-LIVE
       route did not synthesize its raw object-dispatch caller state. */
    bool actor_command_assignment_deferred;
    bool actor_command_assignment_production_mutated;
    uint8_t actor_command_assignment_observed_jump_ball_state;
    uint8_t actor_command_assignment_observed_ball_target_object;
    uint8_t actor_command_assignment_primary_actor;
    uint8_t actor_command_assignment_defender_actor;
    uint16_t actor_command_assignment_primary_stream_before;
    uint16_t actor_command_assignment_primary_stream_after;
    uint8_t actor_command_assignment_primary_state_before;
    uint8_t actor_command_assignment_primary_state_after;
    uint16_t actor_command_assignment_defender_stream_before;
    uint16_t actor_command_assignment_defender_stream_after;
    uint8_t actor_command_assignment_defender_state_before;
    uint8_t actor_command_assignment_defender_state_after;
    uint32_t actor_command_assignment_scene_frame_before;
    uint32_t actor_command_assignment_scene_frame_after;
    uint32_t actor_command_assignment_sync_serial_before;
    uint32_t actor_command_assignment_sync_serial_after;
} LiveProofEventEvidence;

static void live_proof_error(char *message, size_t message_size,
                             const char *text)
{
    if (message == NULL || message_size == 0U) return;
    (void)snprintf(message, message_size, "%s", text != NULL ? text :
                   "LIVE proof rejected");
}

static bool live_proof_reject(char *message, size_t message_size,
                              const char *text)
{
    live_proof_error(message, message_size, text);
    return false;
}

static bool live_proof_append(char *buffer, size_t capacity, size_t *length,
                              const char *format, ...)
{
    va_list args;
    int written;
    if (buffer == NULL || length == NULL || format == NULL ||
        *length >= capacity) {
        return false;
    }
    va_start(args, format);
    written = vsnprintf(buffer + *length, capacity - *length, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *length) return false;
    *length += (size_t)written;
    return true;
}

static uint32_t live_proof_fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    if (bytes == NULL) return 0U;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static bool live_proof_event_valid(const char *event)
{
    static const char *const names[] = {
        "pretip-start",
        "live-handoff",
        "human-movement",
        "offensive-pass",
        "defensive-switch",
        "cpu-target-deferred",
        "actor-command-assignment-deferred",
        "cpu-primary-stream-step",
        "shot-path",
        "claimant-settlement",
        "defensive-foul-presentation"
    };
    size_t index;
    if (event == NULL || event[0] == '\0') return false;
    for (index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (strcmp(event, names[index]) == 0) return true;
    }
    return false;
}

static void live_proof_launch_init(TecmoGameplaySceneLaunch *launch)
{
    memset(launch, 0, sizeof(*launch));
    launch->source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch->game_index = 0U;
    launch->away_team = 0U;
    launch->home_team = 1U;
    launch->regulation_minutes = 4U;
    launch->difficulty = 0U;
    launch->control_mode = 0U;
    launch->speed_value = 0U;
    launch->controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch->controller_team[1U] = TECMO_GAMEPLAY_TEAM_HOME;
    launch->game_music_enabled = false;
    launch->starter_binding_bound = true;
    /* Deliberately exercises 5/6/10/11 and a non-identity home permutation. */
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][0U] = 5U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][1U] = 6U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][2U] = 10U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][3U] = 11U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_AWAY][4U] = 0U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][0U] = 11U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][1U] = 10U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][2U] = 6U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][3U] = 5U;
    launch->starter_roster_index[TECMO_GAMEPLAY_TEAM_HOME][4U] = 1U;
}

static void live_proof_controls_neutral(TecmoControlFrame *frame)
{
    if (frame == NULL) return;
    memset(frame, 0, sizeof(*frame));
}

static bool live_proof_advance_pretip(TecmoGameplayScene *scene)
{
    TecmoControlFrame away;
    TecmoControlFrame neutral;
    uint8_t saved_home_controller_team;
    bool home_automatic_fixture;
    size_t update;
    live_proof_controls_neutral(&away);
    live_proof_controls_neutral(&neutral);
    saved_home_controller_team = scene->launch.controller_team[1U];
    home_automatic_fixture = saved_home_controller_team ==
        TECMO_GAMEPLAY_TEAM_HOME;
    if (home_automatic_fixture) {
        scene->launch.controller_team[1U] =
            TECMO_GAMEPLAY_SCENE_NO_TEAM;
    }
    for (update = 0U; update < LIVE_PROOF_MAX_PRETIP_UPDATES; ++update) {
        if (!tecmo_gameplay_scene_in_pretip(scene)) {
            if (home_automatic_fixture) {
                scene->launch.controller_team[1U] =
                    saved_home_controller_team;
            }
            return true;
        }
        live_proof_controls_neutral(&away);
        if (scene->pretip_state.phase ==
            TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP) {
            away.held.cancel = true;
        }
        if (!tecmo_gameplay_scene_update(scene, &away, &neutral)) {
            if (home_automatic_fixture) {
                scene->launch.controller_team[1U] =
                    saved_home_controller_team;
            }
            return false;
        }
        if (home_automatic_fixture &&
            scene->pretip_state.claim_resolved) {
            scene->launch.controller_team[1U] =
                saved_home_controller_team;
            home_automatic_fixture = false;
        }
    }
    if (home_automatic_fixture) {
        scene->launch.controller_team[1U] = saved_home_controller_team;
    }
    return !tecmo_gameplay_scene_in_pretip(scene);
}

static bool live_proof_force_possession(TecmoGameplayScene *scene,
                                        TecmoGameplayTeam team,
                                        uint8_t holder)
{
    if (!scene_handoff_possession(scene, team, holder) ||
        !scene_sync_live_foundation(scene)) {
        return false;
    }
    return scene_ownership_valid(scene);
}

static bool live_proof_live_ownership(const TecmoGameplayScene *scene,
                                      char *message, size_t message_size)
{
    if (scene == NULL || tecmo_gameplay_scene_in_pretip(scene) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.first_sync_pending ||
        scene->live_foundation.primary_actor != scene->ball_holder ||
        scene->live_foundation.last_possession !=
            (uint8_t)scene->state.possession ||
        !tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation) ||
        !scene_ownership_valid(scene)) {
        return live_proof_reject(
            message, message_size,
            "LIVE handoff did not produce synchronized LIVE ownership");
    }
    return true;
}

static bool live_proof_find_offset(
    const TecmoGameplayCpuSteeringAssets *assets,
    bool target_command,
    uint16_t *offset_out)
{
    uint16_t offset;
    if (assets == NULL || offset_out == NULL || !assets->available) return false;
    for (offset = 0U;
         offset < (uint16_t)(assets->command_record_count *
                             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE);
         offset = (uint16_t)(offset +
                             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        TecmoGameplayCpuSteeringCommand command;
        if (!tecmo_gameplay_cpu_steering_decode_command(
                assets, offset, &command)) {
            continue;
        }
        if (target_command && command.opcode == 4U) {
            *offset_out = offset;
            return true;
        }
        if (!target_command &&
            (command.opcode == 5U || command.opcode == 6U ||
             command.opcode == 8U || command.opcode == 10U ||
             command.opcode == 12U || command.opcode == 13U ||
             command.opcode == 15U || command.opcode == 16U ||
             command.opcode == 20U || command.opcode == 23U)) {
            *offset_out = offset;
            return true;
        }
    }
    return false;
}

/* Choose the canonical Bank04 opcode-4 ball-object record. The selected-primary
   exclusion fixture parks this otherwise supported record on `$0308` and
   proves the ordinary Bank06 actor loop cannot execute it. */
static bool live_proof_find_opcode4_ball_record(
    const TecmoGameplayScene *scene,
    uint16_t *offset_out,
    uint8_t *wait_out,
    TecmoGameplayCourtCoordinate *target_out)
{
    const TecmoGameplayCpuSteeringAssets *assets;
    uint16_t offset;
    if (scene == NULL || offset_out == NULL || wait_out == NULL ||
        target_out == NULL) {
        return false;
    }
    assets = &scene->cpu_steering_assets;
    if (!assets->available) {
        return false;
    }
    for (offset = 0U;
         offset < assets->command_record_count *
             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE;
         offset = (uint16_t)(offset +
             TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        TecmoGameplayCpuSteeringCommand target_command;
        if (!tecmo_gameplay_cpu_steering_decode_command(
                assets, offset, &target_command) ||
            target_command.opcode != 4U ||
            target_command.arguments[0U] !=
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT) {
            continue;
        }
        *offset_out = offset;
        *wait_out = 0U;
        target_out->x = 0;
        target_out->y = 0;
        return true;
    }
    return false;
}

static bool live_proof_prepare_cpu_fixture(TecmoGameplayScene *scene)
{
    TecmoGameplayLiveFoundation candidate;
    uint16_t target_offset;
    uint16_t deferred_offset;
    if (scene == NULL ||
        !live_proof_find_offset(&scene->cpu_steering_assets, true,
                                &target_offset) ||
        !live_proof_find_offset(&scene->cpu_steering_assets, false,
                                &deferred_offset) ||
        !live_proof_force_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return false;
    }
    /* Deterministic test fixture: CPU-only routing is injected after the
       bound production-style launch. It is not original or normal-policy
       evidence. */
    scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (!live_proof_force_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return false;
    }
    candidate = scene->live_foundation;
    candidate.play_state.stream_offset[1U] = target_offset;
    candidate.last_step_offset[1U] = target_offset;
    candidate.play_state.target_object[1U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.play_state.target_x[1U] = 0;
    candidate.play_state.target_depth[1U] = 0;
    candidate.source_target_valid[1U] = false;
    candidate.play_state.stream_offset[2U] = deferred_offset;
    candidate.last_step_offset[2U] = deferred_offset;
    candidate.play_state.target_object[2U] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.play_state.target_x[2U] = 0;
    candidate.play_state.target_depth[2U] = 0;
    candidate.source_target_valid[2U] = false;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate)) {
        return false;
    }
    scene->live_foundation = candidate;
    return true;
}

/* This fixture changes only an ordinary LIVE pair's court coordinates and
 * pre-existing team-foul count.  The foul itself must be accepted from the
 * normal outer scene update's human defensive-B dispatch; it never calls the
 * state-level request API or writes a phase. */
static bool live_proof_trigger_defensive_foul(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t holder;
    uint8_t defender;
    uint8_t defender_roster;
    uint8_t referee_group;
    uint16_t action_before;
    size_t frame;

    if (scene == NULL || evidence == NULL ||
        !live_proof_force_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
        return live_proof_reject(message, message_size,
                                 "live defensive-foul handoff failed");
    }
    holder = scene->ball_holder;
    defender = scene->controlled_actor[1U];
    if (holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->live_foundation.primary_actor != holder ||
        scene->actors[holder].team != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->actors[defender].team != TECMO_GAMEPLAY_TEAM_HOME) {
        return live_proof_reject(message, message_size,
                                 "live defensive-foul pair was not selected");
    }
    /* The real nonidentity launch begins controller two on a different home
       player than Bank05's selected defender.  Use the ordinary human A
       switch first, then require its selected defender to be the B contact
       actor.  This is a production action sequence, not a direct selector
       or phase mutation. */
    if (scene->live_foundation.defender_actor != defender) {
        live_proof_controls_neutral(&p1);
        live_proof_controls_neutral(&p2);
        p2.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE) {
            return live_proof_reject(
                message, message_size,
                "human defensive A switch rejected before foul proof");
        }
        defender = scene->controlled_actor[1U];
        if (defender >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->live_foundation.defender_actor != defender ||
            scene->actors[defender].team != TECMO_GAMEPLAY_TEAM_HOME) {
            return live_proof_reject(
                message, message_size,
                "human defensive A did not select the B contact actor");
        }
    }
    if (scene->actors[defender].roster_index >= TECMO_GAMEPLAY_PLAYER_COUNT) {
        return live_proof_reject(message, message_size,
                                 "live defensive-foul roster was invalid");
    }
    defender_roster = scene->actors[defender].roster_index;
    scene->actors[defender].position.x =
        (int16_t)(scene->actors[holder].position.x + 1);
    scene->actors[defender].position.y = scene->actors[holder].position.y;
    scene->actors[defender].anchor = scene->actors[defender].position;
    if (!scene_sync_live_foundation(scene) || !scene_ownership_valid(scene)) {
        return live_proof_reject(
            message, message_size,
            "live defensive-foul contact fixture did not synchronize");
    }
    /* One prior team foul below the strict B02 regulation bonus threshold.
       This proves the classifier-derived two-attempt branch rather than a
       scene-owned fixed FTs value. */
    scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] = 4U;
    action_before = scene->action_serial;
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    p2.held.cancel = true;
    p2.pressed.cancel = true;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME] != 5U ||
        scene->state.individual_fouls[TECMO_GAMEPLAY_TEAM_HOME]
            [defender_roster] != 1U ||
        scene->state.free_throws.scoring_team != TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->state.free_throws.attempts_remaining != 2U ||
        scene->action_serial != (uint16_t)(action_before + 1U) ||
        !scene->foul_presentation.valid ||
        scene->foul_presentation.fouling_team != TECMO_GAMEPLAY_TEAM_HOME ||
        scene->foul_presentation.actor_index != defender ||
        scene->foul_presentation.roster_index != defender_roster ||
        scene->foul_presentation.foul_class != TECMO_GAMEPLAY_FOUL_CLASS_PUSHING ||
        scene->foul_presentation.individual_foul_delta != 1U ||
        scene->foul_presentation.team_foul_delta != 1U ||
        scene->foul_presentation.individual_fouls_after != 1U ||
        scene->foul_presentation.team_fouls_after != 5U ||
        scene->foul_presentation.free_throw_attempts != 2U ||
        !scene->foul_presentation.team_in_bonus ||
        scene->foul_presentation.fouled_out) {
        if (message != NULL && message_size != 0U) {
            (void)snprintf(
                message, message_size,
                "human defensive-B did not enter strict live foul presentation (phase=%u serial=%u before=%u holder=%u primary=%u defender=%u controlled=%u possession=%u fouls=%u attempts=%u input=%u/%u controller=%u status=%s)",
                (unsigned)scene->state.phase,
                (unsigned)scene->action_serial, (unsigned)action_before,
                (unsigned)scene->ball_holder,
                (unsigned)scene->live_foundation.primary_actor,
                (unsigned)scene->live_foundation.defender_actor,
                (unsigned)scene->controlled_actor[1U],
                (unsigned)scene->state.possession,
                (unsigned)scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME],
                (unsigned)scene->state.free_throws.attempts_remaining,
                p2.held.cancel ? 1U : 0U, p2.pressed.cancel ? 1U : 0U,
                (unsigned)scene->launch.controller_team[1U], scene->status);
        }
        return false;
    }
    /* $E95E orders Bank02's screen-$22 writer before selector $22 reaches
       the Bank04 referee controller. The loader blackout/fade is only
       capture-bounded, so capture the first current renderer frame that also
       has selector-0's visible group 1 rather than claiming an exact B02 PPU
       completion frame. No release input is supplied. */
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    for (frame = 0U; frame < LIVE_PROOF_FOUL_VISIBLE_FRAME; ++frame) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
            !scene->foul_presentation.valid) {
            return live_proof_reject(
                message, message_size,
                "live defensive-foul overlay did not retain presentation");
        }
    }
    if (scene->state.phase_frame != LIVE_PROOF_FOUL_VISIBLE_FRAME ||
        !tecmo_gameplay_violation_referee_foul_group_for_frame(
            &scene->violation_referee_assets, scene->state.phase_frame,
            &referee_group) || referee_group != 1U) {
        return live_proof_reject(
            message, message_size,
            "live defensive-foul overlay did not reach referee group 1");
    }
    evidence->foul_overlay_retained = true;
    evidence->foul_visible_phase_frame = scene->state.phase_frame;
    evidence->foul_referee_group = referee_group;
    return true;
}

/* Establish a deterministic, ordinary controller-B miss fixture from the
 * actual completed pre-tip live handoff. It never injects a claimant,
 * possession, phase, or finish call: the outer production update launches and
 * settles the shot, and the claimant is selected later by the normal
 * $B73E-derived scene scan. The limited coordinates/frame seed are transparent
 * test-fixture inputs rather than claims about original policy. */
static bool live_proof_trigger_claimant_settlement(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene launched;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoControlFrame neutral;
    TecmoGameplayCourtCoordinate shooter;
    TecmoGameplayCourtCoordinate claimant;
    TecmoGameplayCourtCoordinate far_actor = {576, 192};
    TecmoGameplayTeam shooting_team;
    TecmoGameplayTeam claimant_team;
    uint8_t shooting_actor;
    uint8_t claimant_actor;
    size_t controller;
    uint32_t serial_before;
    uint16_t update;
    uint32_t seed;

    if (scene == NULL || evidence == NULL ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team !=
            (uint8_t)scene->state.possession ||
        scene->orientation_state.offensive_hoop.x <= 48 ||
        scene->orientation_state.offensive_hoop.x >=
            TECMO_GAMEPLAY_COURT_WORLD_MAX_X - 48) {
        return live_proof_reject(message, message_size,
                                 "claimant settlement native pre-tip handoff failed");
    }
    shooting_actor = scene->ball_holder;
    shooting_team = (TecmoGameplayTeam)scene->state.possession;
    claimant_team = scene_other_team(shooting_team);
    claimant_actor = scene_first_actor_for_team(claimant_team);
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == shooting_team &&
            scene->controlled_actor[controller] == shooting_actor) {
            break;
        }
    }
    if (controller == TECMO_GAMEPLAY_CONTROLLER_COUNT ||
        claimant_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        claimant_actor == shooting_actor ||
        scene->actors[claimant_actor].team != (uint8_t)claimant_team) {
        return live_proof_reject(
            message, message_size,
            "claimant settlement native controller/claimant fixture failed");
    }

    /* Bounded low-byte frame search selects a real normal-B miss. The explicit
       valid-coordinate fixture is synchronized through the ordinary LIVE
       boundary before the outer update starts the shot; it does not inject a
       claimant, possession, phase, or terminal handler. */
    for (seed = 0U; seed < 256U; ++seed) {
        launched = *scene;
        shooter.x = (int16_t)(
            launched.orientation_state.attack_direction == 0U
                ? launched.orientation_state.offensive_hoop.x + 48
                : launched.orientation_state.offensive_hoop.x - 48);
        shooter.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
        if (!scene_actor_coordinate_valid(&shooter)) continue;
        for (size_t actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            launched.actors[actor].position = far_actor;
            launched.actors[actor].anchor = far_actor;
        }
        launched.actors[shooting_actor].position = shooter;
        launched.actors[shooting_actor].anchor = shooter;
        launched.frame = seed;
        launched.state.shot_clock = 12U;
        launched.state.clock_divider = 1U;
        if (!scene_sync_live_foundation(&launched) ||
            !scene_attach_ball(&launched)) {
            continue;
        }
        live_proof_controls_neutral(&p1);
        live_proof_controls_neutral(&p2);
        if (controller == 0U) {
            p1.held.cancel = true;
            p1.pressed.cancel = true;
        } else {
            p2.held.cancel = true;
            p2.pressed.cancel = true;
        }
        if (!tecmo_gameplay_scene_update(&launched, &p1, &p2) ||
            launched.shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
            launched.shot_actor != shooting_actor ||
            launched.shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS) {
            continue;
        }
        *scene = launched;
        evidence->claimant_fixture_launch_frame = (uint8_t)seed;
        break;
    }
    if (seed == 256U) {
        return live_proof_reject(
            message, message_size,
            "claimant settlement fixture could not launch normal-B miss");
    }

    claimant.x = (int16_t)(scene->shot_end_position.x_q8 / 256);
    claimant.y = (int16_t)(scene->shot_end_position.y_q8 / 256);
    if (!scene_actor_coordinate_valid(&claimant)) {
        return live_proof_reject(message, message_size,
                                 "claimant settlement endpoint was invalid");
    }
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (actor == shooting_actor) continue;
        scene->actors[actor].position = far_actor;
        scene->actors[actor].anchor = far_actor;
    }
    /* Put the first opposing roster slot at the resolved endpoint and retain
       the other non-shooters at the explicit far fixture. The normal claimant
       selector, not this helper, decides whether that arrangement qualifies.
       This affects only the source-order fixture, never the handoff code. */
    scene->actors[claimant_actor].position = claimant;
    scene->actors[claimant_actor].anchor = claimant;
    serial_before = scene->claimant_settlement_trace.event_serial;
    live_proof_controls_neutral(&neutral);
    for (update = 0U; update < 256U;
         ++update) {
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral)) {
            return live_proof_reject(
                message, message_size,
                "claimant settlement production update failed");
        }
        if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) break;
    }
    if (scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession != claimant_team ||
        scene->ball_holder != claimant_actor ||
        !scene->claimant_settlement_trace.valid ||
        scene->claimant_settlement_trace.contract_tag !=
            TECMO_GAMEPLAY_SCENE_CLAIMANT_TRACE_TAG ||
        scene->claimant_settlement_trace.event_serial == 0U ||
        scene->claimant_settlement_trace.event_serial == serial_before ||
        scene->claimant_settlement_trace.transaction.contract_tag !=
            TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG ||
        !scene->claimant_settlement_trace.transaction.side_context_swapped ||
        !scene->claimant_settlement_trace.transaction.raw_04b0_bit10_toggled ||
        scene->claimant_settlement_trace.before.raw_0308_primary_actor !=
            shooting_actor ||
        scene->claimant_settlement_trace.after.raw_0308_primary_actor !=
            claimant_actor ||
        scene->claimant_settlement_trace.after.semantic_scene_possession !=
            claimant_team ||
        scene->claimant_settlement_trace.after.semantic_ball_holder !=
            claimant_actor ||
        !scene_ownership_valid(scene)) {
        return live_proof_reject(
            message, message_size,
            "claimant settlement did not reach Bank05 typed handoff");
    }
    evidence->claimant_settlement_executed = true;
    evidence->claimant_settlement_updates = (uint16_t)(update + 1U);
    evidence->claimant_shooting_actor = shooting_actor;
    evidence->claimant_actor = claimant_actor;
    evidence->claimant_settlement = scene->claimant_settlement_trace;
    return true;
}

/* There is intentionally no call to the TGCA fixture resolver here.  The
 * normal scene owns a numeric jump-ball state, but it does not retain the
 * Bank05 object-slot-10 state/coordinate nor the raw $BA/$05A1/$0499/$0588/
 * $67/$68/$04AF gates that select B73A/B783/B7B6.  Invoking a fixture with
 * fabricated inputs would make this proof look like a gameplay attachment. */
static bool live_proof_observe_actor_command_assignment_deferred(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayLiveFoundation foundation_before;
    uint32_t frame_before;
    uint8_t phase_before;
    uint8_t possession_before;
    uint8_t holder_before;
    if (scene == NULL || evidence == NULL ||
        !live_proof_live_ownership(scene, message, message_size)) {
        return false;
    }
    foundation_before = scene->live_foundation;
    frame_before = scene->frame;
    phase_before = (uint8_t)scene->state.phase;
    possession_before = (uint8_t)scene->state.possession;
    holder_before = scene->ball_holder;

    /* This event is a deliberately empty production observation.  It must
       stay so until a scene transition supplies a complete raw caller. */
    if (memcmp(&foundation_before, &scene->live_foundation,
               sizeof(foundation_before)) != 0 ||
        frame_before != scene->frame || phase_before != (uint8_t)scene->state.phase ||
        possession_before != (uint8_t)scene->state.possession ||
        holder_before != scene->ball_holder) {
        return live_proof_reject(
            message, message_size,
            "actor-command-assignment deferred observation mutated production");
    }
    evidence->actor_command_assignment_deferred = true;
    evidence->actor_command_assignment_production_mutated = false;
    evidence->actor_command_assignment_observed_jump_ball_state =
        scene->jump_ball_state;
    evidence->actor_command_assignment_observed_ball_target_object =
        holder_before < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
            ? scene->live_foundation.play_state.target_object[holder_before]
            : TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    evidence->actor_command_assignment_primary_actor =
        foundation_before.primary_actor;
    evidence->actor_command_assignment_defender_actor =
        foundation_before.defender_actor;
    evidence->actor_command_assignment_primary_stream_before =
        foundation_before.play_state.stream_offset[
            foundation_before.primary_actor];
    evidence->actor_command_assignment_primary_stream_after =
        scene->live_foundation.play_state.stream_offset[
            scene->live_foundation.primary_actor];
    evidence->actor_command_assignment_primary_state_before =
        foundation_before.play_state.actor_state[
            foundation_before.primary_actor];
    evidence->actor_command_assignment_primary_state_after =
        scene->live_foundation.play_state.actor_state[
            scene->live_foundation.primary_actor];
    evidence->actor_command_assignment_defender_stream_before =
        foundation_before.play_state.stream_offset[
            foundation_before.defender_actor];
    evidence->actor_command_assignment_defender_stream_after =
        scene->live_foundation.play_state.stream_offset[
            scene->live_foundation.defender_actor];
    evidence->actor_command_assignment_defender_state_before =
        foundation_before.play_state.actor_state[
            foundation_before.defender_actor];
    evidence->actor_command_assignment_defender_state_after =
        scene->live_foundation.play_state.actor_state[
            scene->live_foundation.defender_actor];
    evidence->actor_command_assignment_scene_frame_before = frame_before;
    evidence->actor_command_assignment_scene_frame_after = scene->frame;
    evidence->actor_command_assignment_sync_serial_before =
        foundation_before.sync_serial;
    evidence->actor_command_assignment_sync_serial_after =
        scene->live_foundation.sync_serial;
    return true;
}

static bool live_proof_apply_event(TecmoGameplayScene *scene,
                                   const char *event,
                                   LiveProofEventEvidence *evidence,
                                   char *message,
                                   size_t message_size)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    if (scene == NULL || event == NULL || evidence == NULL) {
        return live_proof_reject(message, message_size,
                                 "LIVE proof event context missing");
    }
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    if (strcmp(event, "pretip-start") == 0) {
        if (!tecmo_gameplay_scene_in_pretip(scene) ||
            !tecmo_gameplay_pretip_is_presentation(&scene->pretip_state) ||
            scene->pretip_state.live_handoff ||
            !scene->live_foundation.first_sync_pending ||
            scene->live_foundation.sync_serial != 0U ||
            scene->live_foundation.last_ball_holder !=
                TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
            scene->live_foundation.last_possession !=
                (uint8_t)TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->live_foundation.primary_actor !=
                scene->live_foundation.static_primary_seed ||
            scene->live_foundation.defender_actor !=
                scene->live_foundation.static_defender_seed ||
            scene->live_foundation.static_primary_seed != 4U ||
            scene->live_foundation.static_defender_seed != 9U) {
            return live_proof_reject(
                message, message_size,
                "pretip-start was not an unsynchronized presentation state");
        }
        return true;
    }
    if (!live_proof_advance_pretip(scene)) {
        return live_proof_reject(message, message_size,
                                 "real PRETIP-to-LIVE advancement failed");
    }
    if (strcmp(event, "live-handoff") == 0) {
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "defensive-foul-presentation") == 0) {
        return live_proof_trigger_defensive_foul(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "claimant-settlement") == 0) {
        return live_proof_trigger_claimant_settlement(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "actor-command-assignment-deferred") == 0) {
        return live_proof_observe_actor_command_assignment_deferred(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "human-movement") == 0) {
        int16_t start_x;
        uint8_t roster_index;
        uint8_t condition;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "human movement fixture handoff failed");
        }
        start_x = scene->actors[scene->controlled_actor[0U]].position.x;
        roster_index = scene->actors[0U].roster_index;
        condition = scene->actors[0U].condition;
        p1.held.right = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[0U].position.x != start_x ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[0U].position.x != (int16_t)(start_x + 1) ||
            scene->actors[0U].roster_index != roster_index ||
            scene->actors[0U].condition != condition) {
            return live_proof_reject(
                message, message_size,
                "human movement did not prove one-update TGMO latency");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "offensive-pass") == 0) {
        uint8_t expected_roster;
        uint8_t expected_condition;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "offensive pass fixture handoff failed");
        }
        expected_roster = scene->launch.starter_roster_index
            [TECMO_GAMEPLAY_TEAM_AWAY][1U];
        expected_condition = scene->fatigue_state.condition
            [TECMO_GAMEPLAY_TEAM_AWAY][expected_roster];
        p1.held.shoot = true;
        p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            !scene_pass_active(scene) || scene->ball_holder != 0U ||
            scene->controlled_actor[0U] != 0U ||
            scene->actors[1U].team != TECMO_GAMEPLAY_TEAM_AWAY ||
            scene->actors[1U].roster_index != expected_roster ||
            scene->actors[1U].condition != expected_condition) {
            return live_proof_reject(
                message, message_size,
                "offensive pass did not preserve bound receiver identity");
        }
        for (size_t pass_guard = 0U;
             scene_pass_active(scene) && pass_guard < 40U; ++pass_guard) {
            memset(&p1, 0, sizeof(p1));
            if (!tecmo_gameplay_scene_update(scene, &p1, &p2))
                return live_proof_reject(message, message_size,
                                         "offensive pass flight rejected");
        }
        if (scene_pass_active(scene) || scene->ball_holder != 1U ||
            scene->controlled_actor[0U] != 1U)
            return live_proof_reject(message, message_size,
                                     "offensive pass catch did not settle");
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "defensive-switch") == 0) {
        uint8_t expected_actor;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) {
            return live_proof_reject(message, message_size,
                                     "defensive switch fixture handoff failed");
        }
        expected_actor = scene->live_foundation.candidate_actor_by_side[
            TECMO_GAMEPLAY_TEAM_AWAY];
        p1.pressed.shoot = true;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2) ||
            scene->controlled_actor[0U] != expected_actor ||
            scene->controlled_actor[1U] != 5U ||
            expected_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[expected_actor].team != TECMO_GAMEPLAY_TEAM_AWAY ||
            expected_actor == scene->ball_holder) {
            return live_proof_reject(
                message, message_size,
                "defensive A did not consume the directional candidate");
        }
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "cpu-target-deferred") == 0) {
        TecmoGameplayCpuSteeringCommand opcode4;
        size_t target_count = 0U;
        size_t deferred_count = 0U;
        size_t actor;
        if (!live_proof_prepare_cpu_fixture(scene) ||
            !tecmo_gameplay_cpu_steering_decode_command(
                &scene->cpu_steering_assets,
                scene->live_foundation.play_state.stream_offset[1U],
                &opcode4) || opcode4.opcode != 4U ||
            opcode4.arguments[0U] !=
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
            !tecmo_gameplay_court_coordinate_q8_floor(
                &scene->ball_position, &evidence->opcode4_ball_snapshot) ||
            !tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            return live_proof_reject(message, message_size,
                                     "CPU target/deferred fixture update failed");
        }
        for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            if (scene->live_foundation.source_target_valid[actor]) {
                ++target_count;
            }
            if (scene->live_foundation.deferred[actor]) ++deferred_count;
        }
        if (target_count == 0U || deferred_count == 0U) {
            return live_proof_reject(
                message, message_size,
                "CPU event did not expose both source target and deferred actors");
        }
        evidence->opcode4_record_offset = opcode4.stream_offset;
        evidence->opcode4_argument_c8 = opcode4.arguments[0U];
        evidence->opcode4_target_object =
            scene->live_foundation.play_state.target_object[1U];
        evidence->opcode4_source_target.x =
            scene->live_foundation.play_state.target_x[1U];
        evidence->opcode4_source_target.y =
            scene->live_foundation.play_state.target_depth[1U];
        if (!scene->live_foundation.source_target_valid[1U] ||
            scene->live_foundation.deferred[1U] ||
            evidence->opcode4_target_object !=
                TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
            evidence->opcode4_source_target.x !=
                evidence->opcode4_ball_snapshot.x ||
            evidence->opcode4_source_target.y !=
                evidence->opcode4_ball_snapshot.y ||
            !scene->cpu_actors[1U].target_valid ||
            scene->cpu_actors[1U].target_kind !=
                TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET ||
            scene->cpu_actors[1U].target_position.x !=
                evidence->opcode4_ball_snapshot.x ||
            scene->cpu_actors[1U].target_position.y !=
                evidence->opcode4_ball_snapshot.y) {
            return live_proof_reject(
                message, message_size,
                "CPU opcode-4 canonical ball target was not retained");
        }
        evidence->opcode4_ball_target = true;
        return live_proof_live_ownership(scene, message, message_size);
    }
    if (strcmp(event, "cpu-primary-stream-step") == 0) {
        TecmoGameplayLiveFoundation candidate;
        TecmoGameplayCourtCoordinate unused_target;
        uint16_t source_offset;
        uint8_t source_wait;
        scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !live_proof_find_opcode4_ball_record(
                scene, &source_offset, &source_wait,
                &unused_target)) {
            return live_proof_reject(
                message, message_size,
                "CPU primary-step fixture could not locate Bank04 ball target");
        }
        candidate = scene->live_foundation;
        candidate.play_state.stream_offset[0U] = source_offset;
        candidate.last_step_offset[0U] = source_offset;
        candidate.play_state.actor_state[0U] = 0x04U;
        candidate.play_state.wait_counter[0U] = 0U;
        candidate.play_state.target_object[0U] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
        candidate.play_state.target_x[0U] = 0;
        candidate.play_state.target_depth[0U] = 0;
        candidate.play_state.direction[0U] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        candidate.source_target_valid[0U] = false;
        candidate.source_direction_valid[0U] = false;
        candidate.source_direction[0U] =
            TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
        candidate.deferred[0U] = false;
        candidate.deferred_reason[0U] =
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        /* Bank06 $8374-$83F3 dispatches the automatic selected primary before
           $8286/$8289 skips it in the ordinary loop. Park a valid opcode-4
           record on actor 0 and hold every non-primary actor so one production
           update proves the cursor is consumed exactly once. */
        for (size_t actor = 1U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            candidate.play_state.wait_counter[actor] =
                LIVE_PROOF_PRIMARY_STREAM_HOLD_UPDATES;
            candidate.play_state.actor_state[actor] = 0x06U;
            candidate.deferred[actor] = false;
            candidate.deferred_reason[actor] =
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        }
        if (!tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &candidate)) {
            return live_proof_reject(
                message, message_size,
                "CPU primary-step fixture foundation was rejected");
        }
        scene->live_foundation = candidate;
        if (!scene_attach_ball(scene)) {
            return live_proof_reject(message, message_size,
                                     "CPU primary-step ball attach failed");
        }
        evidence->primary_record_offset = source_offset;
        evidence->primary_wait_frames = source_wait;
        evidence->primary_stream_before = source_offset;
        evidence->primary_last_step_before = source_offset;
        evidence->primary_action_before = candidate.play_state
            .action_state_046e[0U];
        evidence->primary_action_serial_before = scene->action_serial;
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
            return live_proof_reject(
                message, message_size,
                "CPU primary-step production update failed");
        }
        evidence->primary_stream_after = scene->live_foundation.play_state
            .stream_offset[0U];
        evidence->primary_last_step_after =
            scene->live_foundation.last_step_offset[0U];
        evidence->primary_action_after = scene->live_foundation.play_state
            .action_state_046e[0U];
        evidence->primary_action_serial_after = scene->action_serial;
        if (scene->live_foundation.primary_actor != 0U ||
            evidence->primary_stream_after !=
                (uint16_t)(evidence->primary_stream_before +
                           TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
            evidence->primary_last_step_after !=
                (uint16_t)(evidence->primary_last_step_before +
                           TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
            evidence->primary_action_after !=
                evidence->primary_action_before ||
            evidence->primary_action_serial_after !=
                evidence->primary_action_serial_before ||
            !scene->live_foundation.source_target_valid[0U] ||
            !scene->cpu_actors[0U].target_valid ||
            scene->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            return live_proof_reject(
                message, message_size,
                "selected primary did not execute exactly one source step");
        }
        evidence->cpu_primary_stream_stepped = true;
        return true;
    }
    if (strcmp(event, "shot-path") == 0) {
        TecmoGameplayCourtCoordinate close_position;
        TecmoGameplaySceneCpuShotRequest shot_request;
        TecmoControlFrame neutral;
        uint16_t action_before;
        scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
        scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
        /* Deterministic close-position fixture for the supported shots.c
           profile; this is not original source evidence or normal policy. */
        if (!live_proof_force_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
            return live_proof_reject(message, message_size,
                                     "shot fixture handoff failed");
        }
        close_position.x = (int16_t)(
            scene->orientation_state.offensive_hoop.x + 8);
        close_position.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
        if (!scene_actor_coordinate_valid(&close_position)) {
            return live_proof_reject(
                message, message_size,
                "shot close-position fixture was outside TGCT bounds");
        }
        scene->actors[0U].position = close_position;
        scene->actors[0U].anchor = close_position;
        scene->state.shot_clock = 12U;
        scene->state.clock_divider = 1U;
        if (!scene_attach_ball(scene)) return false;
        action_before = scene->action_serial;
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) ||
            !shot_request.requested || !shot_request.playback_supported ||
            shot_request.deferred ||
            scene->action_serial != (uint16_t)(action_before + 1U) ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != 0U) {
            return live_proof_reject(message, message_size,
                                     "shot fixture did not launch exactly once");
        }
        /* Advance the already-started close shot through the production
           outer update. This is playback only: AI is not re-entered, the
           excluded shot path is not called a second time, and action_serial
           must remain at the single launch increment. */
        live_proof_controls_neutral(&neutral);
        if (!tecmo_gameplay_scene_update(scene, &neutral, &neutral) ||
            scene->action_serial != (uint16_t)(action_before + 1U) ||
            scene->shot_frame == 0U ||
            !scene_shot_is_close(scene->shot_kind) ||
            scene->shot_actor != 0U) {
            return live_proof_reject(
                message, message_size,
                "shot fixture visible playback advance rejected");
        }
        return true;
    }
    return live_proof_reject(message, message_size,
                             "LIVE proof event was not implemented");
}

static bool live_proof_render(const TecmoRuntime *runtime,
                              const char *output_png_path,
                              uint32_t *frame_hash_out)
{
    TecmoFramebuffer framebuffer;
    uint32_t *pixels;
    uint8_t *rgba;
    size_t pixel_count;
    size_t index;
    if (runtime == NULL || output_png_path == NULL ||
        output_png_path[0] == '\0' || frame_hash_out == NULL) return false;
    pixel_count = (size_t)LIVE_PROOF_WIDTH * (size_t)LIVE_PROOF_HEIGHT;
    pixels = (uint32_t *)calloc(pixel_count, sizeof(*pixels));
    rgba = (uint8_t *)malloc(pixel_count * 4U);
    if (pixels == NULL || rgba == NULL) {
        free(pixels);
        free(rgba);
        return false;
    }
    framebuffer.pixels = pixels;
    framebuffer.width = LIVE_PROOF_WIDTH;
    framebuffer.height = LIVE_PROOF_HEIGHT;
    framebuffer.pitch_pixels = LIVE_PROOF_WIDTH;
    tecmo_runtime_render(runtime, &framebuffer);
    for (index = 0U; index < pixel_count; ++index) {
        rgba[index * 4U + 0U] = (uint8_t)((pixels[index] >> 16U) & 0xFFU);
        rgba[index * 4U + 1U] = (uint8_t)((pixels[index] >> 8U) & 0xFFU);
        rgba[index * 4U + 2U] = (uint8_t)(pixels[index] & 0xFFU);
        rgba[index * 4U + 3U] = (uint8_t)((pixels[index] >> 24U) & 0xFFU);
    }
    *frame_hash_out = live_proof_fnv1a32(rgba, pixel_count * 4U);
    if (png_write_rgba8(output_png_path, rgba,
                        LIVE_PROOF_WIDTH, LIVE_PROOF_HEIGHT) != 0) {
        free(pixels);
        free(rgba);
        return false;
    }
    free(pixels);
    free(rgba);
    return true;
}

static bool live_proof_append_u8_array(char *buffer, size_t capacity,
                                        size_t *length,
                                        const uint8_t *values, size_t count)
{
    size_t index;
    if (values == NULL || !live_proof_append(
            buffer, capacity, length, "[")) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (!live_proof_append(buffer, capacity, length, "%s%u",
                               index == 0U ? "" : ",",
                               (unsigned)values[index])) {
            return false;
        }
    }
    return live_proof_append(buffer, capacity, length, "]");
}

static bool live_proof_append_u16_array(char *buffer, size_t capacity,
                                         size_t *length,
                                         const uint16_t *values, size_t count)
{
    size_t index;
    if (values == NULL || !live_proof_append(
            buffer, capacity, length, "[")) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (!live_proof_append(buffer, capacity, length, "%s%u",
                               index == 0U ? "" : ",",
                               (unsigned)values[index])) {
            return false;
        }
    }
    return live_proof_append(buffer, capacity, length, "]");
}

static bool live_proof_append_possession_snapshot(
    char *buffer,
    size_t capacity,
    size_t *length,
    const TecmoGameplayScenePossessionTraceSnapshot *snapshot)
{
    if (snapshot == NULL || snapshot->contract_tag !=
            TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG ||
        !live_proof_append(
            buffer, capacity, length,
            "{\"contract\":\"TGPS-1\",\"sync_serial\":%u,"
            "\"raw\":{\"$0308\":%u,\"$0309\":%u,"
            "\"$030A\":%u,\"$030B\":%u,\"$030C_$030D\":",
            (unsigned)snapshot->sync_serial,
            (unsigned)snapshot->raw_0308_primary_actor,
            (unsigned)snapshot->raw_0309_defender_actor,
            (unsigned)snapshot->raw_030a_offense_side,
            (unsigned)snapshot->raw_030b_defense_side) ||
        !live_proof_append_u8_array(
            buffer, capacity, length, snapshot->raw_030c_030d_control_mode,
            TECMO_GAMEPLAY_TEAM_COUNT) ||
        !live_proof_append(buffer, capacity, length,
                           ",\"$000E_$000F\":") ||
        !live_proof_append_u8_array(
            buffer, capacity, length,
            snapshot->raw_000e_000f_selected_actor,
            TECMO_GAMEPLAY_TEAM_COUNT) ||
        !live_proof_append(buffer, capacity, length,
                           ",\"$037F_$0380\":") ||
        !live_proof_append_u8_array(
            buffer, capacity, length,
            snapshot->raw_037f_0380_candidate_actor,
            TECMO_GAMEPLAY_TEAM_COUNT) ||
        !live_proof_append(buffer, capacity, length,
                           ",\"$04B0_bit10_flags\":") ||
        !live_proof_append_u8_array(
            buffer, capacity, length, snapshot->raw_04b0_selector_flags,
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) ||
        !live_proof_append(buffer, capacity, length, ",\"$06CB\":") ||
        !live_proof_append_u8_array(
            buffer, capacity, length, snapshot->raw_06cb_dynamic_link,
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) ||
        !live_proof_append(buffer, capacity, length,
                           ",\"$0547_$0551_stream_offset\":") ||
        !live_proof_append_u16_array(
            buffer, capacity, length,
            snapshot->raw_0547_0551_stream_offset,
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) ||
        !live_proof_append(buffer, capacity, length, ",\"$057C\":") ||
        !live_proof_append_u8_array(
            buffer, capacity, length, snapshot->raw_057c_actor_state,
            TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) ||
        !live_proof_append(
            buffer, capacity, length,
            "},\"opcode15\":{\"observed\":%s,\"opcode\":%u,"
            "\"record_offset\":%u,\"branch\":\"%s\","
            "\"missing_raw_mask\":\"%08X\","
            "\"raw_gate_available\":{\"$0499\":%s,\"$04B0\":%s,"
            "\"$007E\":%s,\"$06D5_$06D6\":%s,\"$0479\":%s,"
            "\"$0442_$044D\":%s,\"$059E\":%s,"
            "\"actor_lifecycle\":%s},\"typed_before_after\":{"
            "\"$0308\":[%u,%u],\"$0309\":[%u,%u],"
            "\"$0547_$0551\":[%u,%u],\"$057C\":[%u,%u]}},"
            "\"semantic\":{\"scene_possession\":%u,"
            "\"ball_holder\":%u,\"live_last_possession\":%u,"
            "\"live_last_ball_holder\":%u,\"live_synchronized\":%s}}",
            snapshot->opcode15_trace.observed ? "true" : "false",
            (unsigned)snapshot->opcode15_trace.opcode,
            (unsigned)snapshot->opcode15_trace.command_record_offset,
            tecmo_gameplay_cpu_steering_opcode15_branch_name(
                snapshot->opcode15_trace.branch),
            (unsigned)snapshot->opcode15_trace.missing_raw_mask,
            snapshot->opcode15_trace.raw_0499_available ? "true" : "false",
            snapshot->opcode15_trace.raw_04b0_available ? "true" : "false",
            snapshot->opcode15_trace.raw_007e_available ? "true" : "false",
            snapshot->opcode15_trace.raw_06d5_06d6_available ? "true" : "false",
            snapshot->opcode15_trace.raw_0479_available ? "true" : "false",
            snapshot->opcode15_trace.raw_0442_044d_available ? "true" : "false",
            snapshot->opcode15_trace.raw_059e_available ? "true" : "false",
            snapshot->opcode15_trace.raw_actor_lifecycle_available ? "true" : "false",
            (unsigned)snapshot->opcode15_trace.raw_0308_before,
            (unsigned)snapshot->opcode15_trace.raw_0308_after,
            (unsigned)snapshot->opcode15_trace.raw_0309_before,
            (unsigned)snapshot->opcode15_trace.raw_0309_after,
            (unsigned)snapshot->opcode15_trace.actor_stream_before,
            (unsigned)snapshot->opcode15_trace.actor_stream_after,
            (unsigned)snapshot->opcode15_trace.actor_state_before,
            (unsigned)snapshot->opcode15_trace.actor_state_after,
            (unsigned)snapshot->semantic_scene_possession,
            (unsigned)snapshot->semantic_ball_holder,
            (unsigned)snapshot->semantic_live_last_possession,
            (unsigned)snapshot->semantic_live_last_ball_holder,
            snapshot->semantic_live_synchronized ? "true" : "false")) {
        return false;
    }
    return true;
}

static bool live_proof_json(const TecmoGameplayScene *scene,
                            const char *event,
                            const char *output_png_path,
                            uint32_t frame_hash,
                            const LiveProofEventEvidence *evidence,
                            char *message, size_t message_size)
{
    size_t length = 0U;
    size_t actor;
    size_t target_count = 0U;
    size_t deferred_count = 0U;
    bool live_foul_event;
    bool claimant_event;
    bool foul_overlay_visible = false;
    bool scene_rebounds_nonzero = false;
    uint8_t foul_group = 255U;
    TecmoGameplayReboundAuditInput rebound_input;
    TecmoGameplayReboundAuditDecision rebound_decision;
    if (scene == NULL || event == NULL || output_png_path == NULL ||
        evidence == NULL || message == NULL || message_size == 0U) {
        return false;
    }
    (void)output_png_path;
    live_foul_event = strcmp(event, "defensive-foul-presentation") == 0 ||
        strcmp(event, "defensive-foul-free-throw") == 0;
    claimant_event = evidence->claimant_settlement_executed &&
        evidence->claimant_settlement.valid &&
        evidence->claimant_settlement.contract_tag ==
            TECMO_GAMEPLAY_SCENE_CLAIMANT_TRACE_TAG &&
        evidence->claimant_settlement.transaction.contract_tag ==
            TECMO_GAMEPLAY_LIVE_CLAIMANT_SETTLEMENT_TAG &&
        evidence->claimant_settlement.before.contract_tag ==
            TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG &&
        evidence->claimant_settlement.after.contract_tag ==
            TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG;
    /* The LIVE bridge may demonstrate a native claimant settlement, but it
       does not retain raw $BA, $0588, or the $BE/$BF owner at $C042. Keep all
       unsupported values absent rather than deriving them from a miss. */
    memset(&rebound_input, 0, sizeof(rebound_input));
    if (claimant_event && evidence->claimant_actor <
                              TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        const TecmoGameplaySceneActor *claimant =
            &scene->actors[evidence->claimant_actor];
        rebound_input.claimant_settlement_valid = true;
        rebound_input.claimant_actor = evidence->claimant_actor;
        rebound_input.claimant_team = claimant->team;
        rebound_input.claimant_roster_index = claimant->roster_index;
        rebound_input.claimant_event_serial =
            evidence->claimant_settlement.event_serial;
    }
    if (!tecmo_gameplay_rebound_audit_resolve(
            &scene->rebound_audit, &rebound_input, &rebound_decision)) {
        return false;
    }
    for (size_t side = 0U; side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT;
         ++side) {
        for (size_t roster = 0U; roster < TECMO_PLAYER_STATS_ROSTER_COUNT;
             ++roster) {
            if (scene->player_stats.counters[side][roster]
                    [TECMO_PLAYER_STATS_COUNTER_REBOUNDS] != 0U) {
                scene_rebounds_nonzero = true;
            }
        }
    }
    if (live_foul_event && scene->foul_presentation.valid &&
        scene->state.phase == TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
        foul_overlay_visible = scene->state.phase_frame >=
            TECMO_GAMEPLAY_VIOLATION_REFEREE_BLACK_FRAMES;
        (void)tecmo_gameplay_violation_referee_foul_group_for_frame(
            &scene->violation_referee_assets, scene->state.phase_frame,
            &foul_group);
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        if (scene->live_foundation.source_target_valid[actor]) ++target_count;
        if (scene->live_foundation.deferred[actor]) ++deferred_count;
    }
    message[0] = '\0';
    if (!live_proof_append(
            message, message_size, &length,
            "{\"schema\":\"tecmo.live-proof/TGLP-1\","
            "\"event\":\"%s\","
            "\"launch\":\"direct-bound-nonidentity-scene-launch\","
             "\"render_path\":\"TecmoRuntime/TECMO_MODE_COURT->tecmo_runtime_render\","
             "\"resolution\":[640,480],\"pretip_skip_hook\":false,"
             "\"pretip\":{\"in_presentation\":%s,"
             "\"is_presentation\":%s,\"phase\":%u,"
             "\"live_handoff\":%s,\"first_sync_pending\":%s,"
             "\"synchronized\":%s},"
             "\"frame\":%u,\"phase\":%u,\"possession\":%u,"
            "\"ball_holder\":%u,\"orientation\":%u,"
            "\"controlled\":[%u,%u],\"action_serial\":%u,"
            "\"shot_kind\":%u,\"shot_actor\":%u,\"shot_frame\":%u,"
            "\"frame_fingerprint_fnv1a32\":\"%08X\","
            "\"starter_roster_index\":{"
            "\"away\":[%u,%u,%u,%u,%u],"
            "\"home\":[%u,%u,%u,%u,%u]},\"live\":{"
            "\"valid\":%s,\"formation_index\":%u,"
            "\"primary_actor\":%u,\"defender_actor\":%u,"
            "\"orientation\":%u,\"sync_serial\":%u,"
            "\"tick_serial\":%u,\"first_sync_pending\":%s,"
            "\"last_shot_request\":%s,\"last_shot_deferred\":%s,"
            "\"last_shot_playback_supported\":%s,"
            "\"last_shot_actor\":%u,\"source_target_count\":%u,"
            "\"deferred_count\":%u},"
            "\"asm_evidence\":{\"formation_refresh\":"
            "\"Bank06 C-0039 $944D-$9465\","
            "\"command_stream\":\"Bank04 $9F2E five-byte records\","
            "\"primary_dispatch\":\"Bank06 $8374-$83F3 -> $8491\","
            "\"cpu_shot_gate\":\"Bank06 C-0011 $8431-$8475\"},"
            "\"cpu_primary_stream_step\":{\"proved\":%s,"
            "\"record_offset\":\"%04X\",\"wait_frames\":%u,"
            "\"stream\":[\"%04X\",\"%04X\"],"
            "\"last_step\":[\"%04X\",\"%04X\"],"
            "\"action\":[%u,%u],\"action_serial\":[%u,%u]},"
            "\"opcode4_ball_target\":{\"executed\":%s,"
            "\"record_offset\":\"%04X\",\"argument_c8\":%u,"
            "\"target_object\":%u,\"snapshot_ball\":[%d,%d],"
            "\"source_target\":[%d,%d]},"
            "\"live_foul\":{\"active\":%s,"
            "\"entrypoint\":\"%s\",\"direct_phase_injection\":false,"
            "\"contact_gate\":\"Bank05:$9968-$999D\","
            "\"commit\":\"Bank05:$9571-$9649:C83877F7\","
            "\"classifier\":\"Bank02:$B0F8-$B398:A06E397C\","
            "\"adapter_profile\":[%u,%u,%u],"
            "\"fouling_team\":%u,\"team_fouls_home\":%u,"
            "\"attempts_remaining\":%u,\"presentation\":{"
            "\"retained\":%s,\"foul_class\":%u,"
            "\"actor\":%u,\"roster\":%u,"
            "\"individual_delta\":%u,\"team_delta\":%u,"
            "\"individual_after\":%u,\"team_after\":%u,"
            "\"attempts\":%u,\"team_in_bonus\":%s,"
            "\"fouled_out\":%s,\"visible_phase_frame\":%u,"
            "\"overlay_visible\":%s,\"referee_group\":%u,"
            "\"court_actors_suppressed\":%s,"
            "\"overlay_writer\":\"Bank02:$B0F8-$B398\","
            "\"referee_script\":\"fixed:$E95E-$EA11:$2C-then-$22\","
            "\"timing\":\"TGVR capture-bounded blackout/fade; Bank02 completion frame unavailable\"}},",
              event,
             tecmo_gameplay_scene_in_pretip(scene) ? "true" : "false",
             tecmo_gameplay_pretip_is_presentation(
                 &scene->pretip_state) ? "true" : "false",
             (unsigned)scene->pretip_state.phase,
             scene->pretip_state.live_handoff ? "true" : "false",
             scene->live_foundation.first_sync_pending ? "true" : "false",
             (!scene->live_foundation.first_sync_pending &&
              scene->live_foundation.sync_serial != 0U) ? "true" : "false",
             (unsigned)scene->frame, (unsigned)scene->state.phase,
            (unsigned)scene->state.possession, (unsigned)scene->ball_holder,
            (unsigned)scene->orientation_state.attack_direction,
            (unsigned)scene->controlled_actor[0U],
            (unsigned)scene->controlled_actor[1U],
            (unsigned)scene->action_serial, (unsigned)scene->shot_kind,
            (unsigned)scene->shot_actor, (unsigned)scene->shot_frame,
            (unsigned)frame_hash,
            (unsigned)scene->launch.starter_roster_index[0U][0U],
            (unsigned)scene->launch.starter_roster_index[0U][1U],
            (unsigned)scene->launch.starter_roster_index[0U][2U],
            (unsigned)scene->launch.starter_roster_index[0U][3U],
            (unsigned)scene->launch.starter_roster_index[0U][4U],
            (unsigned)scene->launch.starter_roster_index[1U][0U],
            (unsigned)scene->launch.starter_roster_index[1U][1U],
            (unsigned)scene->launch.starter_roster_index[1U][2U],
            (unsigned)scene->launch.starter_roster_index[1U][3U],
            (unsigned)scene->launch.starter_roster_index[1U][4U],
            tecmo_gameplay_live_foundation_valid(
                &scene->cpu_steering_assets, &scene->live_foundation)
                ? "true" : "false",
            (unsigned)scene->live_foundation.formation_index,
            (unsigned)scene->live_foundation.primary_actor,
            (unsigned)scene->live_foundation.defender_actor,
            (unsigned)scene->live_foundation.orientation,
            (unsigned)scene->live_foundation.sync_serial,
            (unsigned)scene->live_foundation.tick_serial,
            scene->live_foundation.first_sync_pending ? "true" : "false",
            scene->live_foundation.last_shot_request ? "true" : "false",
            scene->live_foundation.last_shot_deferred ? "true" : "false",
            scene->live_foundation.last_shot_playback_supported ? "true" :
                                                                  "false",
            (unsigned)scene->live_foundation.last_shot_actor,
            (unsigned)target_count, (unsigned)deferred_count,
            evidence->cpu_primary_stream_stepped ? "true" : "false",
            (unsigned)evidence->primary_record_offset,
            (unsigned)evidence->primary_wait_frames,
            (unsigned)evidence->primary_stream_before,
            (unsigned)evidence->primary_stream_after,
            (unsigned)evidence->primary_last_step_before,
            (unsigned)evidence->primary_last_step_after,
            (unsigned)evidence->primary_action_before,
            (unsigned)evidence->primary_action_after,
            (unsigned)evidence->primary_action_serial_before,
            (unsigned)evidence->primary_action_serial_after,
            evidence->opcode4_ball_target ? "true" : "false",
            (unsigned)evidence->opcode4_record_offset,
            (unsigned)evidence->opcode4_argument_c8,
            (unsigned)evidence->opcode4_target_object,
            (int)evidence->opcode4_ball_snapshot.x,
            (int)evidence->opcode4_ball_snapshot.y,
            (int)evidence->opcode4_source_target.x,
            (int)evidence->opcode4_source_target.y,
            live_foul_event ? "true" : "false",
            live_foul_event
                ? "tecmo_gameplay_scene_update/human-defensive-B"
                : "none",
            (unsigned)TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_SAVED_ROUTE,
            (unsigned)TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CURRENT_ROUTE,
            (unsigned)TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CONTACT_SELECTOR,
            live_foul_event ? (unsigned)TECMO_GAMEPLAY_TEAM_HOME : 255U,
            (unsigned)scene->state.team_fouls[TECMO_GAMEPLAY_TEAM_HOME],
            (unsigned)scene->state.free_throws.attempts_remaining,
            evidence->foul_overlay_retained ? "true" : "false",
            (unsigned)scene->foul_presentation.foul_class,
            (unsigned)scene->foul_presentation.actor_index,
            (unsigned)scene->foul_presentation.roster_index,
            (unsigned)scene->foul_presentation.individual_foul_delta,
            (unsigned)scene->foul_presentation.team_foul_delta,
            (unsigned)scene->foul_presentation.individual_fouls_after,
            (unsigned)scene->foul_presentation.team_fouls_after,
            (unsigned)scene->foul_presentation.free_throw_attempts,
            scene->foul_presentation.team_in_bonus ? "true" : "false",
            scene->foul_presentation.fouled_out ? "true" : "false",
            (unsigned)evidence->foul_visible_phase_frame,
            foul_overlay_visible ? "true" : "false",
            (unsigned)foul_group,
             live_foul_event && evidence->foul_overlay_retained
                 ? "true" : "false")) {
        return false;
    }
    if (!live_proof_append(
            message, message_size, &length,
            "\"rebound_audit\":{\"contract\":\"TGRB-1\","
            "\"ledger_write_enabled\":%s,\"coverage_bit8\":%s,"
            "\"scene_ledger_coverage_bit8\":%s,"
            "\"scene_ledger_rebounds_nonzero\":%s,"
            "\"assets_available\":%s,\"raw_ba_available\":%s,"
            "\"raw_0588_available\":%s,\"be_bf_identity_fresh\":%s,"
            "\"claimant_bridge_observed\":%s,\"claimant_serial\":%u,"
            "\"source_gate_eligible\":%s,\"decision\":\"%s\","
            "\"limitation\":\"native TGLP has no raw $BA/$0588 or fresh $BE/$BF-at-$C042 ownership; REB remains unsupported\"},",
            rebound_decision.ledger_write_enabled ? "true" : "false",
            (TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE &
                 tecmo_player_stats_counter_bit(
                     TECMO_PLAYER_STATS_COUNTER_REBOUNDS)) != 0U
                ? "true" : "false",
            (scene->player_stats.coverage & tecmo_player_stats_counter_bit(
                 TECMO_PLAYER_STATS_COUNTER_REBOUNDS)) != 0U
                ? "true" : "false",
            scene_rebounds_nonzero ? "true" : "false",
            scene->rebound_audit.available ? "true" : "false",
            rebound_input.raw_ba_available ? "true" : "false",
            rebound_input.raw_0588_available ? "true" : "false",
            rebound_input.be_bf_identity_fresh ? "true" : "false",
            claimant_event ? "true" : "false",
            (unsigned)rebound_input.claimant_event_serial,
            rebound_decision.source_gate_eligible ? "true" : "false",
            tecmo_gameplay_rebound_audit_reason_name(
                rebound_decision.reason))) {
        return false;
    }
    if (!live_proof_append(
            message, message_size, &length,
            "\"actor_command_assignment\":{\"deferred_diagnostic\":%s,"
            "\"emitted\":false,"
            "\"caller_identity\":\"none\","
            "\"no_op_reason\":\"missing-source-shaped-object-dispatch-inputs\","
            "\"production_mutated\":%s,"
            "\"direct_fixture_input\":false,"
            "\"asm\":\"Bank05:$A023-$A0DC; callers $9F2F->$9FE2, $B73A, $B783, $B7B6\","
            "\"missing\":{\"object_slot10_state\":true,"
            "\"object_slot10_coordinate\":true,"
            "\"raw_ba_05a1_0499_0588_0067_0068_04af\":true,"
            "\"interaction_9f2f_predecessors\":true},"
            "\"observed_jump_ball_state\":%u,"
            "\"observed_holder_target_object\":%u,"
            "\"scene_frame\":[%u,%u],\"sync_serial\":[%u,%u],"
            "\"exclusions\":{\"primary\":%u,\"defender\":%u},"
            "\"scans\":{\"side10\":{\"executed\":false,\"winner\":null,\"score\":null},"
            "\"side00\":{\"executed\":false,\"winner\":null,\"score\":null}},"
            "\"selected_before_after\":{\"primary\":{\"stream\":[%u,%u],\"state\":[%u,%u]},"
            "\"defender\":{\"stream\":[%u,%u],\"state\":[%u,%u]}},"
            "\"screenshot_scope\":\"ordinary native pretip-to-live flow; not A023 gameplay parity\"},",
            evidence->actor_command_assignment_deferred ? "true" : "false",
            evidence->actor_command_assignment_production_mutated ? "true" : "false",
            (unsigned)evidence->actor_command_assignment_observed_jump_ball_state,
            (unsigned)evidence->actor_command_assignment_observed_ball_target_object,
            (unsigned)evidence->actor_command_assignment_scene_frame_before,
            (unsigned)evidence->actor_command_assignment_scene_frame_after,
            (unsigned)evidence->actor_command_assignment_sync_serial_before,
            (unsigned)evidence->actor_command_assignment_sync_serial_after,
            (unsigned)evidence->actor_command_assignment_primary_actor,
            (unsigned)evidence->actor_command_assignment_defender_actor,
            (unsigned)evidence->actor_command_assignment_primary_stream_before,
            (unsigned)evidence->actor_command_assignment_primary_stream_after,
            (unsigned)evidence->actor_command_assignment_primary_state_before,
            (unsigned)evidence->actor_command_assignment_primary_state_after,
            (unsigned)evidence->actor_command_assignment_defender_stream_before,
            (unsigned)evidence->actor_command_assignment_defender_stream_after,
            (unsigned)evidence->actor_command_assignment_defender_state_before,
            (unsigned)evidence->actor_command_assignment_defender_state_after)) {
        return false;
    }
    if (!live_proof_append(
            message, message_size, &length,
            "\"claimant_settlement\":{\"emitted\":%s,"
            "\"entrypoint\":\"%s\",\"direct_handoff_injection\":false,"
            "\"asm\":\"Bank05:$BA56-$BA9C -> $B87C-$B98A; $9042 $04B0 bit-$10 loop\","
            "\"fixture\":{\"starts_from_native_pretip_handoff\":%s,"
            "\"shooting_actor\":%u,\"claimant_actor\":%u},"
            "\"fixture_launch_frame\":%u,\"updates\":%u,"
            "\"event_serial\":%u",
            claimant_event ? "true" : "false",
            claimant_event
                ? "tecmo_gameplay_scene_update/normal-B-miss"
                : "none",
            claimant_event ? "true" : "false",
            claimant_event
                ? (unsigned)evidence->claimant_shooting_actor : 0U,
            claimant_event ? (unsigned)evidence->claimant_actor : 0U,
            claimant_event
                ? (unsigned)evidence->claimant_fixture_launch_frame : 0U,
            claimant_event
                ? (unsigned)evidence->claimant_settlement_updates : 0U,
            claimant_event
                ? (unsigned)evidence->claimant_settlement.event_serial : 0U)) {
        return false;
    }
    if (claimant_event) {
        const TecmoGameplayLiveClaimantSettlement *transaction =
            &evidence->claimant_settlement.transaction;
        if (!live_proof_append(
                message, message_size, &length,
                ",\"transaction\":{\"raw_0308_before\":%u,"
                "\"raw_0309_before\":%u,\"raw_030a_before\":%u,"
                "\"raw_030b_before\":%u,\"raw_0308_after\":%u,"
                "\"raw_0309_after\":%u,\"raw_030a_after\":%u,"
                "\"raw_030b_after\":%u,\"side_context_swapped\":%s,"
                "\"raw_04b0_bit10_toggled\":%s,"
                "\"automatic_defender_scan_ran\":%s,"
                "\"automatic_defender_match_found\":%s,"
                "\"raw_035a_save_and_toggle_observed\":%s},\"before\":",
                (unsigned)transaction->raw_0308_before,
                (unsigned)transaction->raw_0309_before,
                (unsigned)transaction->raw_030a_before,
                (unsigned)transaction->raw_030b_before,
                (unsigned)transaction->raw_0308_after,
                (unsigned)transaction->raw_0309_after,
                (unsigned)transaction->raw_030a_after,
                (unsigned)transaction->raw_030b_after,
                transaction->side_context_swapped ? "true" : "false",
                transaction->raw_04b0_bit10_toggled ? "true" : "false",
                transaction->automatic_defender_scan_ran ? "true" : "false",
                transaction->automatic_defender_match_found
                    ? "true" : "false",
                transaction->raw_035a_save_and_toggle_observed
                    ? "true" : "false") ||
            !live_proof_append_possession_snapshot(
                message, message_size, &length,
                &evidence->claimant_settlement.before) ||
            !live_proof_append(message, message_size, &length,
                               ",\"after\":") ||
            !live_proof_append_possession_snapshot(
                message, message_size, &length,
                &evidence->claimant_settlement.after) ||
            !live_proof_append(message, message_size, &length, "},")) {
            return false;
        }
    } else if (!live_proof_append(
                   message, message_size, &length,
                   ",\"transaction\":null,\"before\":null,\"after\":null},")) {
        return false;
    }
    if (!live_proof_append(message, message_size, &length, "\"actors\":[")) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        const TecmoGameplaySceneCpuActor *cpu = &scene->cpu_actors[actor];
        if (!live_proof_append(
                message, message_size, &length,
                "%s{\"slot\":%u,\"team\":%u,\"roster\":%u,"
                "\"x\":%d,\"y\":%d,\"link\":%u,\"cpu\":{"
                "\"target_valid\":%s,\"kind\":%u,\"x\":%d,"
                "\"y\":%d,\"direction\":%u},"
                "\"source_target_valid\":%s,\"source_target_object\":%u,"
                "\"source_target_x\":%d,\"source_target_y\":%d,"
                "\"source_direction_valid\":%s,\"source_direction\":%u,"
                "\"deferred\":%s,\"deferred_reason\":\"%s\"}",
                actor == 0U ? "" : ",", (unsigned)actor,
                (unsigned)item->team, (unsigned)item->roster_index,
                (int)item->position.x, (int)item->position.y,
                (unsigned)cpu->linked_actor,
                cpu->target_valid ? "true" : "false",
                (unsigned)cpu->target_kind,
                (int)cpu->target_position.x, (int)cpu->target_position.y,
                (unsigned)cpu->direction,
                scene->live_foundation.source_target_valid[actor]
                    ? "true" : "false",
                (unsigned)scene->live_foundation.play_state
                    .target_object[actor],
                (int)scene->live_foundation.play_state.target_x[actor],
                (int)scene->live_foundation.play_state.target_depth[actor],
                scene->live_foundation.source_direction_valid[actor]
                    ? "true" : "false",
                (unsigned)scene->live_foundation.source_direction[actor],
                scene->live_foundation.deferred[actor] ? "true" : "false",
                tecmo_gameplay_cpu_steering_deferred_reason_name(
                    scene->live_foundation.deferred_reason[actor]))) {
            return false;
        }
    }
    return live_proof_append(message, message_size, &length, "]}");
}

bool tecmo_gameplay_live_foundation_proof(
    const char *project_root,
    const char *asset_pack_path,
    const char *event,
    const char *output_png_path,
    char *message,
    size_t message_size)
{
    TecmoRuntime runtime;
    TecmoGameMemory memory;
    TecmoGameplaySceneLaunch launch;
    LiveProofEventEvidence evidence;
    void *permanent_block;
    void *transient_block;
    uint32_t frame_hash;
    bool ok = false;
    if (message != NULL && message_size != 0U) message[0] = '\0';
    if (project_root == NULL || asset_pack_path == NULL || event == NULL ||
        output_png_path == NULL || !live_proof_event_valid(event)) {
        live_proof_error(message, message_size,
                         "missing or unsupported LIVE proof input");
        return false;
    }
    memset(&runtime, 0, sizeof(runtime));
    memset(&memory, 0, sizeof(memory));
    memset(&evidence, 0, sizeof(evidence));
    permanent_block = malloc(LIVE_PROOF_MEMORY_SIZE);
    transient_block = malloc(LIVE_PROOF_MEMORY_SIZE);
    if (permanent_block == NULL || transient_block == NULL) {
        live_proof_error(message, message_size,
                         "LIVE proof memory allocation failed");
        free(permanent_block);
        free(transient_block);
        return false;
    }
    tecmo_arena_init(&memory.permanent, permanent_block,
                     LIVE_PROOF_MEMORY_SIZE);
    tecmo_arena_init(&memory.transient, transient_block,
                     LIVE_PROOF_MEMORY_SIZE);
    runtime.memory = &memory;
    tecmo_gameplay_scene_init(&runtime.gameplay_scene);
    if (!tecmo_music_asset_load_from_pack(&runtime.music_asset,
                                          asset_pack_path)) {
        live_proof_error(message, message_size,
                         runtime.music_asset.status);
        goto cleanup;
    }
    tecmo_music_player_init(&runtime.music_player, &runtime.music_asset);
    if (!tecmo_gameplay_scene_load(&runtime.gameplay_scene, project_root,
                                   asset_pack_path, &runtime.music_player)) {
        live_proof_error(message, message_size,
                         runtime.gameplay_scene.status);
        goto cleanup;
    }
    live_proof_launch_init(&launch);
    if (!tecmo_gameplay_scene_launch(&runtime.gameplay_scene, &launch) ||
        !runtime.gameplay_scene.active) {
        live_proof_error(message, message_size,
                         runtime.gameplay_scene.status[0] != '\0'
                             ? runtime.gameplay_scene.status
                             : "bound LIVE proof launch rejected");
        goto cleanup;
    }
    if (!live_proof_apply_event(&runtime.gameplay_scene, event, &evidence,
                                message, message_size)) {
        if (message == NULL || message[0] == '\0') {
            live_proof_error(message, message_size,
                             runtime.gameplay_scene.status[0] != '\0'
                                 ? runtime.gameplay_scene.status
                                 : "LIVE proof event rejected");
        }
        goto cleanup;
    }
    runtime.mode = TECMO_MODE_COURT;
    runtime.normal_play_active = true;
    runtime.debug_overlay = false;
    runtime.frame_counter = runtime.gameplay_scene.frame;
    runtime.mode_frame_counter = runtime.gameplay_scene.frame;
    runtime.frame_seconds = 1.0f / 60.0f;
    if (!live_proof_render(&runtime, output_png_path, &frame_hash) ||
        !live_proof_json(&runtime.gameplay_scene, event, output_png_path,
                         frame_hash, &evidence, message, message_size)) {
        live_proof_error(message, message_size,
                         "LIVE proof render or JSON emission failed");
        goto cleanup;
    }
    ok = true;

cleanup:
    tecmo_gameplay_scene_destroy(&runtime.gameplay_scene);
    tecmo_music_asset_shutdown(&runtime.music_asset);
    free(permanent_block);
    free(transient_block);
    return ok;
}
