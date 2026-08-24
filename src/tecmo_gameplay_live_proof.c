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
#define LIVE_PROOF_CATCH_ROUTE_OFFSET 0x00D7U
#define LIVE_PROOF_CATCH_MAX_UPDATES 64U
#define LIVE_PROOF_AUTO_PASS_STREAM_OFFSET 0x017CU
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
    bool cpu_auto_pass_stream_proved;
    bool cpu_auto_pass_non_deferred;
    bool cpu_auto_pass_object13_inferred;
    uint8_t cpu_auto_pass_checkpoint;
    uint8_t cpu_auto_pass_passer;
    uint8_t cpu_auto_pass_receiver;
    uint16_t cpu_auto_pass_updates;
    uint16_t cpu_auto_pass_stream[6U];
    uint8_t cpu_auto_pass_wait[7U];
    uint8_t cpu_auto_pass_action_after_opcode5;
    uint8_t cpu_auto_pass_action_after_opcode23;
    uint8_t cpu_auto_pass_action_after_opcode6;
    uint8_t cpu_auto_pass_action_gather;
    uint8_t cpu_auto_pass_phase;
    uint8_t cpu_auto_pass_packed;
    uint16_t cpu_auto_pass_flight_frame;
    uint16_t cpu_auto_pass_flight_duration;
    TecmoGameplayCourtCoordinate cpu_auto_pass_passer_start;
    TecmoGameplayCourtCoordinate cpu_auto_pass_passer_after_opcode5;
    TecmoGameplayCourtCoordinate cpu_auto_pass_passer_checkpoint;
    TecmoGameplayCourtCoordinate cpu_auto_pass_receiver_start;
    TecmoGameplayCourtCoordinate cpu_auto_pass_receiver_checkpoint;
    TecmoGameplayCourtCoordinateQ8 cpu_auto_pass_ball_gather;
    TecmoGameplayCourtCoordinateQ8 cpu_auto_pass_ball_checkpoint;
    bool cpu_route_state5_proved;
    uint8_t route_actor;
    uint16_t route_record_offset;
    uint16_t route_stream_before;
    uint16_t route_stream_after;
    TecmoGameplayCourtCoordinate route_actor_launch_position;
    TecmoGameplayCourtCoordinate route_actor_mid_position;
    TecmoGameplayCourtCoordinate route_target_snapshot;
    TecmoGameplayCourtCoordinate route_ball_after_launch;
    uint16_t route_duration;
    uint16_t route_timer_mid;
    uint16_t route_horizontal_q6_launch;
    uint16_t route_horizontal_q6_mid;
    uint16_t route_depth_q6_launch;
    uint16_t route_depth_q6_mid;
    uint32_t route_decision_serial_before;
    uint32_t route_decision_serial_after;
    bool route_target_frozen;
    bool route_no_tgmo_double_step;
    bool route_low_bit1_finished;
    bool route_low_bit0_extra_tick;
    bool route_high_bit0_finished;
    bool route_high_bit1_extra_tick;
    bool cpu_catch_state0_proved;
    bool cpu_catch_pass_proved;
    bool cpu_catch_inbound_proved;
    bool human_catch_state0_proved;
    bool selected_wait_state6_proved;
    bool action17_close_shot_proved;
    bool action17_far_recovery_proved;
    bool action17_nonmatch_unaffected;
    uint16_t action17_updates_to_reach;
    uint16_t action17_serial_before;
    uint16_t action17_serial_after;
    uint8_t action17_shot_kind;
    uint8_t action17_shot_actor;
    uint8_t action17_ball_holder;
    uint8_t action17_far_state;
    uint8_t action17_far_action;
    uint8_t catch_pass_receiver;
    uint8_t catch_inbound_receiver;
    uint8_t catch_human_receiver;
    uint8_t catch_source_state0;
    uint8_t catch_automatic_state;
    uint8_t catch_human_state;
    uint8_t catch_automatic_action;
    uint8_t catch_human_action;
    uint16_t catch_automatic_stream;
    uint16_t catch_stream_after_fetch;
    uint16_t catch_last_step_after_fetch;
    uint16_t catch_step_serial_before;
    uint16_t catch_step_serial_after_fetch;
    uint16_t catch_step_serial_after_move;
    uint16_t catch_stream_after_gate_plus5;
    uint16_t catch_stream_after_gate_plus10;
    uint8_t catch_gate_plus5_shot_clock;
    uint8_t catch_gate_plus5_clock_minutes;
    uint8_t catch_gate_plus5_clock_seconds;
    uint8_t catch_gate_plus10_shot_clock;
    uint8_t catch_gate_plus10_clock_minutes;
    uint8_t catch_gate_plus10_clock_seconds;
    bool catch_gate_exact_time_inputs;
    bool catch_gate_007e_bit1_exact;
    uint32_t catch_decision_serial_before;
    uint32_t catch_decision_serial_after_fetch;
    uint32_t catch_decision_serial_after_move;
    TecmoGameplayCourtCoordinate catch_position_at_transfer;
    TecmoGameplayCourtCoordinate catch_position_after_fetch;
    TecmoGameplayCourtCoordinate catch_position_after_move;
    TecmoGameplayCourtCoordinate catch_source_target;
    uint8_t catch_controlled_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t catch_controlled_after[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t catch_controller_team_before[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t catch_controller_team_after[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t catch_wait_sequence[8U];
    uint16_t catch_wait_stream_before;
    uint16_t catch_wait_stream_after;
    uint8_t foul_referee_group;
    uint16_t foul_visible_phase_frame;
    bool foul_overlay_retained;
    bool claimant_settlement_executed;
    uint8_t claimant_fixture_launch_frame;
    uint16_t claimant_settlement_updates;
    uint8_t claimant_shooting_actor;
    uint8_t claimant_actor;
    TecmoGameplaySceneClaimantSettlementTrace claimant_settlement;
    /* Ordinary no-shot observation remains a deferred diagnostic. Shot
       off-ball evidence separately proves the production state-$17 B783
       caller without synthesizing state-$10/state-$18/interaction inputs. */
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
    bool actor_command_assignment_b783_observed;
    uint8_t actor_command_assignment_b783_raw_0499;
    uint16_t actor_command_assignment_b783_handler_cpu;
    uint16_t actor_command_assignment_b783_opcode20_mask;
    bool shot_offball_capture_proved;
    uint16_t shot_offball_capture_frame;
    uint8_t shot_offball_route_actor;
    uint8_t shot_offball_controlled_actor;
    TecmoGameplayCourtCoordinate shot_offball_route_start;
    TecmoGameplayCourtCoordinate shot_offball_route_capture;
    TecmoGameplayCourtCoordinate shot_offball_controlled_start;
    TecmoGameplayCourtCoordinate shot_offball_controlled_capture;
    bool shot_offball_a9da_observed;
    uint8_t shot_offball_a9da_chosen_actor;
    uint16_t shot_offball_a9da_stream_after;
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

static bool live_proof_offball_capture_frame(const char *event,
                                             uint16_t *frame_out)
{
    static const char *const names[] = {
        "shot-offball-1", "shot-offball-9", "shot-offball-17",
        "shot-offball-25", "shot-offball-59", "shot-offball-65",
        "shot-offball-89"
    };
    static const uint16_t frames[] = {1U, 9U, 17U, 25U, 59U, 65U, 89U};
    size_t index;
    if (event == NULL || frame_out == NULL) return false;
    for (index = 0U; index < sizeof(frames) / sizeof(frames[0]); ++index) {
        if (strcmp(event, names[index]) == 0) {
            *frame_out = frames[index];
            return true;
        }
    }
    return false;
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
        "cpu-auto-pass-opcode5",
        "cpu-auto-pass-action10",
        "cpu-auto-pass-gather",
        "cpu-auto-pass-stream",
        "cpu-route-state5",
        "cpu-catch-state0",
        "shot-path",
        "claimant-settlement",
        "defensive-foul-presentation"
    };
    size_t index;
    uint16_t capture_frame;
    if (event == NULL || event[0] == '\0') return false;
    if (live_proof_offball_capture_frame(event, &capture_frame)) return true;
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

/* Launch a deterministic ordinary controller-B miss from the completed
 * pre-tip handoff. The bounded coordinate/frame search enters through the
 * production outer update and never injects a shot phase or outcome. */
static bool live_proof_launch_controller_b_miss(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    uint8_t *shooting_actor_out,
    TecmoGameplayTeam *shooting_team_out,
    size_t *controller_out,
    bool require_jump_rattle,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene launched;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoGameplayCourtCoordinate shooter;
    TecmoGameplayCourtCoordinate far_actor = {576, 192};
    TecmoGameplayTeam shooting_team;
    uint8_t shooting_actor;
    size_t controller;
    uint32_t seed;

    if (scene == NULL || evidence == NULL || shooting_actor_out == NULL ||
        shooting_team_out == NULL || controller_out == NULL ||
        scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->state.possession >= TECMO_GAMEPLAY_TEAM_COUNT ||
        scene->ball_holder >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->actors[scene->ball_holder].team !=
            (uint8_t)scene->state.possession ||
        scene->orientation_state.offensive_hoop.x <= 48 ||
        scene->orientation_state.offensive_hoop.x >=
            TECMO_GAMEPLAY_COURT_WORLD_MAX_X - 48) {
        return live_proof_reject(message, message_size,
                                 "controller-B miss native pre-tip handoff failed");
    }
    shooting_actor = scene->ball_holder;
    shooting_team = (TecmoGameplayTeam)scene->state.possession;
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] == shooting_team &&
            scene->controlled_actor[controller] == shooting_actor) {
            break;
        }
    }
    if (controller == TECMO_GAMEPLAY_CONTROLLER_COUNT) {
        return live_proof_reject(
            message, message_size,
            "controller-B miss native controller fixture failed");
    }

    /* Bounded low-byte frame search selects a real normal-B miss. The explicit
       valid-coordinate fixture is synchronized through the ordinary LIVE
       boundary before the outer update starts the shot; it does not inject a
       claimant, possession, phase, or terminal handler. */
    for (seed = 0U; seed < 256U; ++seed) {
        launched = *scene;
        shooter.x = (int16_t)(
            launched.orientation_state.attack_direction == 0U
                ? launched.orientation_state.offensive_hoop.x +
                      (require_jump_rattle ? 64 : 48)
                : launched.orientation_state.offensive_hoop.x -
                      (require_jump_rattle ? 64 : 48));
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
            launched.shot_outcome != TECMO_GAMEPLAY_SHOT_OUTCOME_MISS ||
            (require_jump_rattle &&
             (launched.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_JUMP ||
              !launched.shot_rim_rattle_selected))) {
            continue;
        }
        *scene = launched;
        evidence->claimant_fixture_launch_frame = (uint8_t)seed;
        break;
    }
    if (seed == 256U) {
        return live_proof_reject(
            message, message_size,
            "controller-B fixture could not launch normal miss");
    }
    *shooting_actor_out = shooting_actor;
    *shooting_team_out = shooting_team;
    *controller_out = controller;
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
    TecmoControlFrame neutral;
    TecmoGameplayCourtCoordinate claimant;
    TecmoGameplayCourtCoordinate far_actor = {576, 192};
    TecmoGameplayTeam shooting_team;
    TecmoGameplayTeam claimant_team;
    uint8_t shooting_actor;
    uint8_t claimant_actor;
    size_t controller;
    uint32_t serial_before;
    uint16_t update;

    if (!live_proof_launch_controller_b_miss(
            scene, evidence, &shooting_actor, &shooting_team, &controller,
            false, message, message_size)) {
        return false;
    }
    (void)controller;
    claimant_team = scene_other_team(shooting_team);
    claimant_actor = scene_first_actor_for_team(claimant_team);
    if (claimant_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        claimant_actor == shooting_actor ||
        scene->actors[claimant_actor].team != (uint8_t)claimant_team) {
        return live_proof_reject(
            message, message_size,
            "claimant settlement native claimant fixture failed");
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

/* Produce independently replayable temporal frames from the same normal-B
 * rattle. A source-shaped state-5 route and held direction on the non-shooting
 * controller exercise only the production no-possession movement phases. */
static bool live_proof_capture_shot_offball(
    TecmoGameplayScene *scene,
    uint16_t capture_frame,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoControlFrame controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoGameplayCpuSteeringRouteMotionState *motion;
    TecmoGameplayTeam shooting_team;
    uint8_t shooting_actor;
    uint8_t controlled_actor;
    uint8_t route_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t rattle_orientation;
    uint16_t target_x;
    size_t shooting_controller;
    size_t other_controller;
    int actor;

    if (!live_proof_launch_controller_b_miss(
            scene, evidence, &shooting_actor, &shooting_team,
            &shooting_controller, true, message, message_size)) {
        return false;
    }
    (void)shooting_team;
    if (capture_frame < scene->shot_frame || capture_frame > 89U) {
        return live_proof_reject(message, message_size,
                                 "off-ball capture frame outside rattle");
    }
    if (!scene_shot_captured_rattle_orientation(
            scene, &rattle_orientation)) {
        return live_proof_reject(message, message_size,
                                 "off-ball rattle orientation unavailable");
    }
    other_controller = (shooting_controller + 1U) %
        TECMO_GAMEPLAY_CONTROLLER_COUNT;
    controlled_actor = scene->controlled_actor[other_controller];
    for (actor = (int)TECMO_GAMEPLAY_SCENE_ACTOR_COUNT - 1;
         actor >= 0; --actor) {
        bool controlled = false;
        bool pretip_recovery = false;
        size_t index;
        for (index = 0U; index < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++index) {
            if (scene->controlled_actor[index] == (uint8_t)actor) {
                controlled = true;
            }
        }
        if (scene->pretip_jump_active && scene->pretip_state.live_handoff &&
            scene->pretip_state.simulation_active) {
            for (index = 0U;
                 index < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT; ++index) {
                if (scene->pretip_jumper_actor[index] == (uint8_t)actor) {
                    pretip_recovery = true;
                }
            }
        }
        if ((uint8_t)actor != shooting_actor &&
            (uint8_t)actor != scene->live_foundation.primary_actor &&
            (uint8_t)actor != scene->live_foundation.defender_actor &&
            !controlled && !pretip_recovery) {
            route_actor = (uint8_t)actor;
            break;
        }
    }
    if (controlled_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        controlled_actor == shooting_actor ||
        route_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return live_proof_reject(message, message_size,
                                 "off-ball capture actors unavailable");
    }

    target_x = rattle_orientation == 0U ? 0x00A0U : 0x0260U;
    scene->actors[route_actor].position.x = (int16_t)(
        rattle_orientation == 0U
            ? target_x + 32U : target_x - 32U);
    scene->actors[route_actor].position.y = 0x94;
    scene->actors[route_actor].anchor = scene->actors[route_actor].position;
    motion = &scene->live_foundation.play_state.route_motion[route_actor];
    memset(motion, 0, sizeof(*motion));
    motion->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    motion->horizontal_accumulator_q6 = (uint16_t)(
        (uint16_t)scene->actors[route_actor].position.x << 6U);
    motion->depth_accumulator_q6 = (uint16_t)(
        (uint16_t)scene->actors[route_actor].position.y << 6U);
    motion->horizontal_velocity_q6 =
        rattle_orientation == 0U ? -64 : 64;
    motion->depth_velocity_q6 = 0;
    motion->remaining_timer = 32U;
    motion->active = true;
    scene->live_foundation.play_state.actor_state[route_actor] = 0x05U;
    scene->live_foundation.play_state.wait_counter[route_actor] = 0U;
    scene->live_foundation.play_state.target_object[route_actor] =
        TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
    scene->live_foundation.play_state.target_x[route_actor] =
        (int16_t)target_x;
    scene->live_foundation.play_state.target_depth[route_actor] =
        scene->actors[route_actor].position.y;
    scene->live_foundation.source_target_valid[route_actor] = true;
    scene->live_foundation.source_direction_valid[route_actor] = false;
    scene->live_foundation.deferred[route_actor] = false;
    scene->live_foundation.deferred_reason[route_actor] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    scene->cpu_actors[route_actor].decision_serial = 1U;
    scene->cpu_actors[route_actor].snapshot_fingerprint = 0U;
    scene->cpu_actors[route_actor].target_position =
        scene->actors[route_actor].position;
    scene->cpu_actors[route_actor].target_position.x = (int16_t)target_x;
    scene->cpu_actors[route_actor].target_kind =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET;
    scene->cpu_actors[route_actor].direction =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    scene->cpu_actors[route_actor].held_direction_bits =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    scene->cpu_actors[route_actor].target_valid = true;
    scene->cpu_actors[route_actor].writes_direction = false;
    evidence->shot_offball_route_start = scene->actors[route_actor].position;
    evidence->shot_offball_controlled_start =
        scene->actors[controlled_actor].position;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &scene->live_foundation)) {
        return live_proof_reject(message, message_size,
                                 "off-ball source route fixture rejected");
    }

    live_proof_controls_neutral(&controls[0U]);
    live_proof_controls_neutral(&controls[1U]);
    if (scene->actors[controlled_actor].position.x >= 384) {
        controls[other_controller].held.left = true;
    } else {
        controls[other_controller].held.right = true;
    }
    while (scene->shot_frame < capture_frame) {
        uint16_t before_frame = scene->shot_frame;
        if (!tecmo_gameplay_scene_update(
                scene, &controls[0U], &controls[1U])) {
            char detail[256];
            (void)snprintf(detail, sizeof(detail),
                           "off-ball production update rejected before frame %u: %s",
                           (unsigned)before_frame, scene->status);
            return live_proof_reject(message, message_size, detail);
        }
        if (scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
            return live_proof_reject(message, message_size,
                                     "off-ball rattle settled before capture");
        }
    }
    evidence->shot_offball_capture_frame = capture_frame;
    evidence->shot_offball_route_actor = route_actor;
    evidence->shot_offball_controlled_actor = controlled_actor;
    evidence->shot_offball_route_capture = scene->actors[route_actor].position;
    evidence->shot_offball_controlled_capture =
        scene->actors[controlled_actor].position;
    evidence->shot_offball_a9da_observed =
        scene->shot_a9da_assignment_valid;
    evidence->shot_offball_a9da_chosen_actor =
        scene->shot_a9da_assignment_valid
            ? scene->shot_a9da_result.chosen_actor_002d
            : TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    evidence->shot_offball_a9da_stream_after =
        scene->shot_a9da_assignment_valid
            ? scene->live_foundation.last_step_offset[
                  scene->shot_a9da_result.chosen_actor_002d]
            : 0U;
    if (scene->shot_b783_assignment_applied) {
        if (scene->shot_b783_raw_0499 >= 0x04U ||
            scene->shot_b783_handler_cpu != 0xB775U ||
            (scene->shot_b783_opcode20_actor_mask & ~0x03FFU) != 0U ||
            scene->a023_latch_frame_context.available) {
            return live_proof_reject(
                message, message_size,
                "state17 B783 production assignment evidence rejected");
        }
        evidence->actor_command_assignment_production_mutated = true;
        evidence->actor_command_assignment_b783_observed = true;
        evidence->actor_command_assignment_b783_raw_0499 =
            scene->shot_b783_raw_0499;
        evidence->actor_command_assignment_b783_handler_cpu =
            scene->shot_b783_handler_cpu;
        evidence->actor_command_assignment_b783_opcode20_mask =
            scene->shot_b783_opcode20_actor_mask;
    }
    if (scene->shot_frame != capture_frame ||
        scene->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        (capture_frame > 1U &&
         (evidence->shot_offball_route_capture.x ==
              evidence->shot_offball_route_start.x ||
          evidence->shot_offball_controlled_capture.x ==
              evidence->shot_offball_controlled_start.x)) ||
        (capture_frame < 89U && scene->shot_a9da_assignment_valid) ||
        (capture_frame == 89U &&
         (!scene->shot_a9da_assignment_valid ||
          scene->shot_a9da_opcode13_pending ||
          scene->shot_a9da_result.chosen_actor_002d >=
              TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
          scene->shot_a9da_result.chosen_actor_002d == scene->shot_actor ||
          scene->shot_a9da_result.chosen_actor_002d ==
              scene->live_foundation.primary_actor ||
          scene->shot_a9da_result.chosen_actor_002d ==
              scene->live_foundation.defender_actor ||
          evidence->shot_offball_a9da_stream_after != 0x0032U))) {
        char detail[192];
        (void)snprintf(
            detail, sizeof(detail),
            "off-ball evidence mismatch frame=%u holder=%u route=%d/%d ctrl=%d/%d a9da=%u pending=%u chosen=%u prior-route=%u step=%04X",
            (unsigned)scene->shot_frame, (unsigned)scene->ball_holder,
            (int)evidence->shot_offball_route_start.x,
            (int)evidence->shot_offball_route_capture.x,
            (int)evidence->shot_offball_controlled_start.x,
            (int)evidence->shot_offball_controlled_capture.x,
            scene->shot_a9da_assignment_valid ? 1U : 0U,
            scene->shot_a9da_opcode13_pending ? 1U : 0U,
            (unsigned)evidence->shot_offball_a9da_chosen_actor,
            (unsigned)route_actor,
            (unsigned)evidence->shot_offball_a9da_stream_after);
        return live_proof_reject(message, message_size, detail);
    }
    evidence->shot_offball_capture_proved = true;
    return true;
}

/* This ordinary no-shot observation remains deferred for B73A/B7B6 and
 * interaction caller inputs. The production shot-offball proof separately
 * exercises the now-owned slot-10-height->$A214->$B775->$B783 path. */
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

static bool live_proof_route_actor_tgmo_equal(
    const TecmoGameplaySceneActor *left,
    const TecmoGameplaySceneActor *right)
{
    return left != NULL && right != NULL &&
           left->movement_action_state == right->movement_action_state &&
           left->movement_direction == right->movement_direction &&
           left->movement_fractional_accumulator ==
               right->movement_fractional_accumulator &&
           left->movement_animation_phase ==
               right->movement_animation_phase &&
           left->movement_boundary_latched ==
               right->movement_boundary_latched;
}

static bool live_proof_route_parity_case(
    const TecmoGameplayScene *source,
    uint8_t actor,
    uint8_t completion_side_bit,
    bool immediate_finish)
{
    TecmoGameplayScene *candidate;
    TecmoGameplaySceneCpuShotRequest shot_request;
    TecmoGameplayCpuSteeringRouteMotionState *motion;
    bool ok = false;
    size_t index;
    if (source == NULL || actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        completion_side_bit > 1U) {
        return false;
    }
    candidate = (TecmoGameplayScene *)malloc(sizeof(*candidate));
    if (candidate == NULL) return false;
    *candidate = *source;
    /* This fixture isolates Bank06 route completion; a prior proof event's
       shot actor must not suppress the chosen high-half ordinary actor. */
    candidate->shot_actor = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    candidate->pretip_jump_active = false;
    candidate->state.clock_divider = completion_side_bit != 0U ? 1U : 2U;
    for (index = 0U; index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++index) {
        candidate->live_foundation.play_state.route_motion[index].active =
            false;
        candidate->live_foundation.play_state.actor_state[index] = 0x06U;
        candidate->live_foundation.play_state.wait_counter[index] =
            LIVE_PROOF_PRIMARY_STREAM_HOLD_UPDATES;
    }
    motion = &candidate->live_foundation.play_state.route_motion[actor];
    motion->contract_tag =
        TECMO_GAMEPLAY_CPU_STEERING_ROUTE_MOTION_STATE_TAG;
    motion->horizontal_accumulator_q6 = (uint16_t)(
        (uint16_t)candidate->actors[actor].position.x << 6U);
    motion->depth_accumulator_q6 = (uint16_t)(
        (uint16_t)candidate->actors[actor].position.y << 6U);
    motion->horizontal_velocity_q6 = 64;
    motion->depth_velocity_q6 = 0;
    motion->remaining_timer = 1U;
    motion->active = true;
    candidate->live_foundation.play_state.actor_state[actor] = 0x05U;
    candidate->live_foundation.play_state.wait_counter[actor] = 0U;
    candidate->live_foundation.play_state.target_object[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
    candidate->live_foundation.play_state.target_x[actor] =
        candidate->actors[actor].position.x;
    candidate->live_foundation.play_state.target_depth[actor] =
        candidate->actors[actor].position.y;
    candidate->live_foundation.source_target_valid[actor] = true;
    candidate->live_foundation.source_direction_valid[actor] = false;
    candidate->live_foundation.source_direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate->live_foundation.deferred[actor] = false;
    candidate->live_foundation.deferred_reason[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    candidate->cpu_actors[actor].decision_serial = 1U;
    candidate->cpu_actors[actor].snapshot_fingerprint = 0U;
    candidate->cpu_actors[actor].target_position =
        candidate->actors[actor].position;
    candidate->cpu_actors[actor].command_offset =
        TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET;
    candidate->cpu_actors[actor].linked_actor =
        candidate->live_foundation.play_state.fixed_link[actor];
    candidate->cpu_actors[actor].target_kind =
        TECMO_GAMEPLAY_CPU_STEERING_HARNESS_BALL_OBJECT_TARGET;
    candidate->cpu_actors[actor].direction =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate->cpu_actors[actor].held_direction_bits =
        TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
    candidate->cpu_actors[actor].command_advance_pending = false;
    candidate->cpu_actors[actor].target_valid = true;
    candidate->cpu_actors[actor].writes_direction = false;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(candidate, &shot_request) ||
        shot_request.requested ||
        candidate->live_foundation.play_state.route_motion[actor].active ==
            immediate_finish ||
        candidate->live_foundation.play_state.route_motion[actor]
                .remaining_timer != 0U ||
        candidate->live_foundation.play_state.actor_state[actor] !=
            (immediate_finish ? 0x04U : 0x05U)) {
        goto done;
    }
    if (!immediate_finish) {
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(candidate, &shot_request) ||
            shot_request.requested ||
            candidate->live_foundation.play_state.route_motion[actor].active ||
            candidate->live_foundation.play_state.actor_state[actor] != 0x04U) {
            goto done;
        }
    }
    ok = true;

done:
    free(candidate);
    return ok;
}

static bool live_proof_cpu_route_state5(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    const uint8_t actor = 0U;
    TecmoGameplayCpuSteeringCommand opcode4;
    TecmoGameplayLiveFoundation candidate;
    TecmoGameplayCourtCoordinate unused_target;
    TecmoGameplayCourtCoordinate target;
    TecmoGameplayCourtCoordinate retarget;
    TecmoGameplayCourtCoordinateQ8 target_q8;
    TecmoGameplayCpuSteeringRouteMotionState expected_motion;
    TecmoGameplayCpuSteeringRouteStepResult expected_step;
    TecmoGameplaySceneCpuShotRequest shot_request;
    TecmoGameplaySceneActor tgmo_before;
    uint16_t source_offset;
    uint8_t source_wait;
    size_t index;

    scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    if (!live_proof_force_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, actor) ||
        !live_proof_find_opcode4_ball_record(
            scene, &source_offset, &source_wait, &unused_target) ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &scene->cpu_steering_assets, source_offset, &opcode4) ||
        opcode4.opcode != 4U || opcode4.arguments[0U] !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT) {
        return live_proof_reject(
            message, message_size,
            "CPU route fixture could not locate canonical opcode 4");
    }
    target = scene->actors[actor].position;
    target.x = (int16_t)(target.x <= 639 ? target.x + 64 : target.x - 64);
    retarget = target;
    retarget.y = (int16_t)(retarget.y <= 175
        ? retarget.y + 32 : retarget.y - 32);
    if (!tecmo_gameplay_court_coordinate_valid(&target) ||
        !tecmo_gameplay_court_coordinate_valid(&retarget) ||
        !tecmo_gameplay_court_coordinate_to_q8(&target, &target_q8)) {
        return live_proof_reject(message, message_size,
                                 "CPU route fixture target was invalid");
    }

    candidate = scene->live_foundation;
    for (index = 0U; index < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++index) {
        candidate.play_state.route_motion[index].active = false;
        candidate.play_state.actor_state[index] = 0x06U;
        candidate.play_state.wait_counter[index] =
            LIVE_PROOF_PRIMARY_STREAM_HOLD_UPDATES;
    }
    candidate.play_state.stream_offset[actor] = source_offset;
    candidate.last_step_offset[actor] = source_offset;
    candidate.play_state.actor_state[actor] = 0x04U;
    candidate.play_state.wait_counter[actor] = 0U;
    candidate.play_state.target_object[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_ACTOR;
    candidate.play_state.target_x[actor] = 0;
    candidate.play_state.target_depth[actor] = 0;
    candidate.play_state.direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate.source_target_valid[actor] = false;
    candidate.source_direction_valid[actor] = false;
    candidate.source_direction[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_NO_DIRECTION;
    candidate.deferred[actor] = false;
    candidate.deferred_reason[actor] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate)) {
        return live_proof_reject(message, message_size,
                                 "CPU route fixture foundation was rejected");
    }
    scene->live_foundation = candidate;
    scene->ball_position = target_q8;
    evidence->route_actor = actor;
    evidence->route_record_offset = source_offset;
    evidence->route_stream_before = source_offset;
    evidence->route_target_snapshot = target;
    evidence->route_decision_serial_before =
        scene->cpu_actors[actor].decision_serial;
    tgmo_before = scene->actors[actor];
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(scene, &shot_request) || shot_request.requested) {
        return live_proof_reject(message, message_size,
                                 "CPU route opcode-4 launch was rejected");
    }
    evidence->route_stream_after = scene->live_foundation.play_state
        .stream_offset[actor];
    evidence->route_actor_launch_position = scene->actors[actor].position;
    evidence->route_duration = scene->live_foundation.play_state
        .route_motion[actor].remaining_timer;
    evidence->route_horizontal_q6_launch = scene->live_foundation.play_state
        .route_motion[actor].horizontal_accumulator_q6;
    evidence->route_depth_q6_launch = scene->live_foundation.play_state
        .route_motion[actor].depth_accumulator_q6;
    if (evidence->route_stream_after !=
            (uint16_t)(evidence->route_stream_before +
                       TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
        scene->live_foundation.play_state.actor_state[actor] != 0x05U ||
        !scene->live_foundation.play_state.route_motion[actor].active ||
        evidence->route_duration < 3U ||
        scene->live_foundation.play_state.target_object[actor] !=
            TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT ||
        scene->live_foundation.play_state.target_x[actor] != target.x ||
        scene->live_foundation.play_state.target_depth[actor] != target.y ||
        scene->actors[actor].position.x != tgmo_before.position.x ||
        scene->actors[actor].position.y != tgmo_before.position.y ||
        !live_proof_route_actor_tgmo_equal(
            &scene->actors[actor], &tgmo_before) ||
        scene->cpu_actors[actor].decision_serial !=
            evidence->route_decision_serial_before + 1U) {
        return live_proof_reject(
            message, message_size,
            "CPU route launch/cursor/no-TGMO contract failed");
    }

    if (!tecmo_gameplay_court_coordinate_to_q8(
            &retarget, &scene->ball_position)) {
        return live_proof_reject(message, message_size,
                                 "CPU route retarget fixture was invalid");
    }
    evidence->route_ball_after_launch = retarget;
    for (index = 0U; index < 2U; ++index) {
        uint8_t completion_side_bit = (uint8_t)(
            scene->state.clock_divider & 1U);
        if (!tecmo_gameplay_cpu_steering_route_step(
                actor, completion_side_bit,
                &scene->live_foundation.play_state.route_motion[actor],
                &expected_motion, &expected_step)) {
            return live_proof_reject(message, message_size,
                                     "CPU route pure comparison rejected");
        }
        tgmo_before = scene->actors[actor];
        memset(&shot_request, 0, sizeof(shot_request));
        if (!scene_update_ai(scene, &shot_request) || shot_request.requested ||
            memcmp(&scene->live_foundation.play_state.route_motion[actor],
                   &expected_motion, sizeof(expected_motion)) != 0 ||
            scene->actors[actor].position.x !=
                (int16_t)expected_step.horizontal_position ||
            scene->actors[actor].position.y !=
                (int16_t)expected_step.depth_position ||
            !live_proof_route_actor_tgmo_equal(
                &scene->actors[actor], &tgmo_before) ||
            scene->cpu_actors[actor].decision_serial !=
                evidence->route_decision_serial_before + 1U ||
            scene->live_foundation.play_state.stream_offset[actor] !=
                evidence->route_stream_after ||
            scene->live_foundation.play_state.target_x[actor] != target.x ||
            scene->live_foundation.play_state.target_depth[actor] != target.y) {
            return live_proof_reject(
                message, message_size,
                "CPU route state-5 Q6/frozen-target comparison failed");
        }
    }
    evidence->route_actor_mid_position = scene->actors[actor].position;
    evidence->route_timer_mid = scene->live_foundation.play_state
        .route_motion[actor].remaining_timer;
    evidence->route_horizontal_q6_mid = scene->live_foundation.play_state
        .route_motion[actor].horizontal_accumulator_q6;
    evidence->route_depth_q6_mid = scene->live_foundation.play_state
        .route_motion[actor].depth_accumulator_q6;
    evidence->route_decision_serial_after =
        scene->cpu_actors[actor].decision_serial;
    evidence->route_target_frozen = true;
    evidence->route_no_tgmo_double_step = true;

    /* Canonical $8B79-$8B8F completion parity: low actors finish for odd
       $0359 and high actors for even $0359.  The complementary half performs
       exactly one additional state-5 integration with timer zero. */
    evidence->route_low_bit1_finished =
        live_proof_route_parity_case(scene, 0U, 1U, true);
    evidence->route_low_bit0_extra_tick =
        live_proof_route_parity_case(scene, 0U, 0U, false);
    evidence->route_high_bit0_finished =
        live_proof_route_parity_case(scene, 6U, 0U, true);
    evidence->route_high_bit1_extra_tick =
        live_proof_route_parity_case(scene, 6U, 1U, false);
    if (!evidence->route_low_bit1_finished) {
        return live_proof_reject(
            message, message_size,
            "CPU route low-actor bit-1 completion parity failed");
    }
    if (!evidence->route_low_bit0_extra_tick) {
        return live_proof_reject(
            message, message_size,
            "CPU route low-actor bit-0 extra-tick parity failed");
    }
    if (!evidence->route_high_bit0_finished) {
        return live_proof_reject(
            message, message_size,
            "CPU route high-actor bit-0 completion parity failed");
    }
    if (!evidence->route_high_bit1_extra_tick) {
        return live_proof_reject(
            message, message_size,
            "CPU route high-actor bit-1 extra-tick parity failed");
    }
    evidence->cpu_route_state5_proved = true;
    /* scene_update_ai stages movement against the immutable pre-AI snapshot;
       the normal scene update publishes the post-movement foundation sync.
       Mirror that final seam before asking the whole-scene ownership checker. */
    if (!scene_sync_live_foundation(scene)) {
        return live_proof_reject(
            message, message_size,
            "CPU route post-movement foundation sync failed");
    }
    return live_proof_live_ownership(scene, message, message_size);
}

static bool live_proof_make_away_automatic(TecmoGameplayScene *scene)
{
    if (scene == NULL) return false;
    scene->launch.controller_team[0U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    scene->controlled_actor[0U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    /* Controller mutation is explicit fixture input after launch. The normal
       production launch derives this typed mode once from the same mapping. */
    scene->live_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] = 1U;
    return true;
}

static bool live_proof_finish_pass_transport(TecmoGameplayScene *scene)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t update;
    if (scene == NULL || !scene_pass_active(scene)) return false;
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    for (update = 0U; scene_pass_active(scene) &&
         update < LIVE_PROOF_CATCH_MAX_UPDATES; ++update) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    return !scene_pass_active(scene);
}

static bool live_proof_finish_inbound_transport(TecmoGameplayScene *scene)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t update;
    if (scene == NULL || !scene_inbound_active(scene)) return false;
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    for (update = 0U; scene_inbound_active(scene) &&
         update < LIVE_PROOF_CATCH_MAX_UPDATES; ++update) {
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) return false;
    }
    return !scene_inbound_active(scene);
}

static bool live_proof_cpu_catch_progression(
    TecmoGameplayScene *scene,
    uint8_t receiver,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    TecmoGameplayCpuSteeringCommand catch_command;
    TecmoGameplaySceneCpuShotRequest gate_shot_request;
    TecmoGameplayScene *gate_skip = NULL;
    TecmoGameplayScene *natural_action17 = NULL;
    uint16_t cursor_at_transfer;
    uint32_t decision_at_transfer;
    bool ok = false;
    if (scene == NULL || evidence == NULL ||
        receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        scene->ball_holder != receiver ||
        scene->live_foundation.primary_actor != receiver ||
        scene->live_foundation.control_mode[scene->state.possession] == 0U ||
        !tecmo_gameplay_cpu_steering_decode_command(
            &scene->cpu_steering_assets, LIVE_PROOF_CATCH_ROUTE_OFFSET,
            &catch_command) || catch_command.opcode != 2U) {
        return live_proof_reject(message, message_size,
                                 "automatic catch route was not installed");
    }
    gate_skip = (TecmoGameplayScene *)malloc(sizeof(*gate_skip));
    natural_action17 =
        (TecmoGameplayScene *)malloc(sizeof(*natural_action17));
    if (gate_skip == NULL || natural_action17 == NULL) {
        live_proof_error(message, message_size,
                         "automatic catch proof allocation failed");
        goto done;
    }
    cursor_at_transfer = scene->live_foundation.play_state
        .stream_offset[receiver];
    decision_at_transfer = scene->cpu_actors[receiver].decision_serial;
    evidence->catch_source_state0 = 0U;
    evidence->catch_automatic_state = scene->live_foundation.play_state
        .actor_state[receiver];
    evidence->catch_automatic_action = scene->live_foundation.play_state
        .action_state_046e[receiver];
    evidence->catch_automatic_stream = cursor_at_transfer;
    evidence->catch_position_at_transfer = scene->actors[receiver].position;
    evidence->catch_step_serial_before = scene->live_foundation.play_state
        .step_serial;
    evidence->catch_decision_serial_before = decision_at_transfer;
    if (evidence->catch_automatic_state != 0x04U ||
        evidence->catch_automatic_action != 0x18U ||
        cursor_at_transfer != LIVE_PROOF_CATCH_ROUTE_OFFSET ||
        scene->live_foundation.last_step_offset[receiver] !=
            LIVE_PROOF_CATCH_ROUTE_OFFSET) {
        live_proof_error(message, message_size,
                         "automatic B24F catch did not enter long route");
        goto done;
    }

    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        live_proof_error(message, message_size,
                         "automatic catch first LIVE update rejected");
        goto done;
    }
    evidence->catch_stream_after_fetch = scene->live_foundation.play_state
        .stream_offset[receiver];
    evidence->catch_last_step_after_fetch = scene->live_foundation
        .last_step_offset[receiver];
    evidence->catch_step_serial_after_fetch = scene->live_foundation
        .play_state.step_serial;
    evidence->catch_decision_serial_after_fetch =
        scene->cpu_actors[receiver].decision_serial;
    evidence->catch_position_after_fetch = scene->actors[receiver].position;
    evidence->catch_source_target.x = scene->live_foundation.play_state
        .target_x[receiver];
    evidence->catch_source_target.y = scene->live_foundation.play_state
        .target_depth[receiver];
    if (evidence->catch_stream_after_fetch !=
            (uint16_t)(LIVE_PROOF_CATCH_ROUTE_OFFSET +
                       TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) ||
        evidence->catch_last_step_after_fetch !=
            evidence->catch_stream_after_fetch ||
        !scene->live_foundation.source_target_valid[receiver] ||
        scene->live_foundation.deferred[receiver] ||
        evidence->catch_decision_serial_after_fetch !=
            decision_at_transfer + 1U ||
        evidence->catch_position_after_fetch.x !=
            evidence->catch_position_at_transfer.x ||
        evidence->catch_position_after_fetch.y !=
            evidence->catch_position_at_transfer.y) {
        live_proof_error(message, message_size,
                         "automatic catch first fetch/TGMO latency failed");
        goto done;
    }

    *gate_skip = *scene;
    gate_skip->state.shot_clock = 4U;
    gate_skip->state.clock_minutes = 0U;
    gate_skip->state.clock_seconds = 4U;
    memset(&gate_shot_request, 0, sizeof(gate_shot_request));
    if (!scene_update_ai(gate_skip, &gate_shot_request) ||
        gate_shot_request.requested ||
        gate_skip->live_foundation.play_state.stream_offset[receiver] !=
            0x00E6U || gate_skip->live_foundation.deferred[receiver]) {
        if (message != NULL && message_size != 0U) {
            (void)snprintf(message, message_size,
                           "automatic catch opcode-21 +10 failed stream=%04X last=%04X defer=%u reason=%u requested=%u state=%u action=%u holder=%u primary=%u poss=%u controllers=%u/%u",
                           (unsigned)gate_skip->live_foundation.play_state
                               .stream_offset[receiver],
                           (unsigned)gate_skip->live_foundation
                               .last_step_offset[receiver],
                           gate_skip->live_foundation.deferred[receiver]
                               ? 1U : 0U,
                           (unsigned)gate_skip->live_foundation
                               .deferred_reason[receiver],
                           gate_shot_request.requested ? 1U : 0U,
                           (unsigned)gate_skip->live_foundation.play_state
                               .actor_state[receiver],
                           (unsigned)gate_skip->live_foundation.play_state
                               .action_state_046e[receiver],
                           (unsigned)gate_skip->ball_holder,
                           (unsigned)gate_skip->live_foundation.primary_actor,
                           (unsigned)gate_skip->state.possession,
                           (unsigned)gate_skip->launch.controller_team[0U],
                           (unsigned)gate_skip->launch.controller_team[1U]);
        }
        goto done;
    }
    evidence->catch_stream_after_gate_plus10 = 0x00E6U;
    evidence->catch_gate_plus10_shot_clock = gate_skip->state.shot_clock;
    evidence->catch_gate_plus10_clock_minutes = gate_skip->state.clock_minutes;
    evidence->catch_gate_plus10_clock_seconds = gate_skip->state.clock_seconds;
    evidence->catch_gate_exact_time_inputs = true;
    evidence->catch_gate_007e_bit1_exact = true;
    scene->state.shot_clock = 3U;
    scene->state.clock_minutes = 1U;
    scene->state.clock_seconds = 30U;
    evidence->catch_gate_plus5_shot_clock = scene->state.shot_clock;
    evidence->catch_gate_plus5_clock_minutes = scene->state.clock_minutes;
    evidence->catch_gate_plus5_clock_seconds = scene->state.clock_seconds;
    if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {
        live_proof_error(message, message_size,
                         "automatic catch movement update rejected");
        goto done;
    }
    evidence->catch_step_serial_after_move = scene->live_foundation.play_state
        .step_serial;
    evidence->catch_decision_serial_after_move =
        scene->cpu_actors[receiver].decision_serial;
    evidence->catch_position_after_move = scene->actors[receiver].position;
    if (scene->live_foundation.play_state.stream_offset[receiver] != 0x00E1U ||
        !scene->live_foundation.source_target_valid[receiver] ||
        scene->live_foundation.play_state.target_x[receiver] !=
            evidence->catch_source_target.x ||
        scene->live_foundation.play_state.target_depth[receiver] !=
            evidence->catch_source_target.y ||
        scene->live_foundation.deferred[receiver] ||
        evidence->catch_decision_serial_after_move !=
            evidence->catch_decision_serial_after_fetch + 1U ||
        (evidence->catch_position_after_move.x ==
             evidence->catch_position_after_fetch.x &&
         evidence->catch_position_after_move.y ==
             evidence->catch_position_after_fetch.y)) {
        live_proof_error(message, message_size,
                         "automatic catch opcode-21 +5/movement failed");
        goto done;
    }
    evidence->catch_stream_after_gate_plus5 = 0x00E1U;

    *natural_action17 = *scene;
    if (!tecmo_gameplay_scene_update(natural_action17, &p1, &p2) ||
        natural_action17->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        natural_action17->ball_holder != receiver ||
        !natural_action17->live_foundation.last_shot_request ||
        natural_action17->live_foundation.last_shot_playback_supported ||
        !natural_action17->live_foundation.last_shot_deferred ||
        natural_action17->live_foundation.play_state.actor_state[receiver] !=
            0x04U ||
        natural_action17->live_foundation.play_state
                .action_state_046e[receiver] != 0U ||
        natural_action17->live_foundation.play_state.stream_offset[receiver] !=
            0x00E6U ||
        !tecmo_gameplay_scene_update(natural_action17, &p1, &p2) ||
        natural_action17->live_foundation.play_state.stream_offset[receiver] !=
            0x007DU) {
        live_proof_error(message, message_size,
                         "natural catch action-17 recovery/loop failed");
        goto done;
    }
    evidence->action17_updates_to_reach = 2U;
    ok = true;
done:
    free(gate_skip);
    free(natural_action17);
    return ok;
}

static bool live_proof_selected_wait_state6(
    const TecmoGameplayScene *source,
    uint8_t receiver,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene *candidate;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    size_t update;
    bool ok = false;
    if (source == NULL || evidence == NULL ||
        receiver >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    candidate = (TecmoGameplayScene *)malloc(sizeof(*candidate));
    if (candidate == NULL) return false;
    *candidate = *source;
    candidate->live_foundation.play_state.actor_state[receiver] = 0x06U;
    candidate->live_foundation.play_state.wait_counter[receiver] = 8U;
    /* Explicit injected post-opcode-3 fixture: exact $007D advances to
       $0082 as it installs state 6/wait 8. This starts at that coherent
       endpoint and is independent of the chosen $00D7 catch approximation. */
    candidate->live_foundation.play_state.stream_offset[receiver] = 0x0082U;
    candidate->live_foundation.last_step_offset[receiver] = 0x0082U;
    evidence->catch_wait_stream_before = 0x0082U;
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    for (update = 0U; update < 8U; ++update) {
        if (!tecmo_gameplay_scene_update(candidate, &p1, &p2)) goto done;
        evidence->catch_wait_sequence[update] = candidate->live_foundation
            .play_state.wait_counter[receiver];
        if (candidate->live_foundation.play_state.stream_offset[receiver] !=
                0x0082U ||
            evidence->catch_wait_sequence[update] != (uint8_t)(7U - update) ||
            candidate->live_foundation.play_state.actor_state[receiver] !=
                (update == 7U ? 0x04U : 0x06U)) {
            goto done;
        }
    }
    if (!tecmo_gameplay_scene_update(candidate, &p1, &p2) ||
        candidate->live_foundation.play_state.stream_offset[receiver] !=
            (uint16_t)(0x0082U +
                       TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE)) {
        goto done;
    }
    evidence->catch_wait_stream_after = candidate->live_foundation.play_state
        .stream_offset[receiver];
    evidence->selected_wait_state6_proved = true;
    ok = true;
done:
    free(candidate);
    if (!ok) {
        live_proof_error(message, message_size,
                         "selected-primary state-6 wait lifecycle failed");
    }
    return ok;
}

static bool live_proof_action17_boundaries(
    const TecmoGameplayScene *source,
    uint8_t actor,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene *close = NULL;
    TecmoGameplayScene *far = NULL;
    TecmoGameplayScene *nonmatch = NULL;
    TecmoGameplaySceneCpuShotRequest shot_request;
    TecmoGameplayCourtCoordinate close_position;
    uint16_t serial_before;
    bool ok = false;
    if (source == NULL || evidence == NULL ||
        actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return false;
    close = (TecmoGameplayScene *)malloc(sizeof(*close));
    far = (TecmoGameplayScene *)malloc(sizeof(*far));
    nonmatch = (TecmoGameplayScene *)malloc(sizeof(*nonmatch));
    if (close == NULL || far == NULL || nonmatch == NULL) goto done;

    /* Exact opcode 9 at $008C installs state 0/action $17. Bank05 selected
       dispatch then reaches $8A6D->$8ACE; the supported close route must
       transfer ownership to shots.c in the same AI transaction. */
    *close = *source;
    close_position.x = (int16_t)(
        close->orientation_state.offensive_hoop.x < 384 ?
            close->orientation_state.offensive_hoop.x + 8 :
            close->orientation_state.offensive_hoop.x - 8);
    close_position.y = TECMO_GAMEPLAY_SHOT_TARGET_Y;
    close->actors[actor].position = close_position;
    close->actors[actor].anchor = close_position;
    close->live_foundation.play_state.actor_state[actor] = 0x04U;
    close->live_foundation.play_state.action_state_046e[actor] = 0U;
    close->live_foundation.play_state.stream_offset[actor] = 0x008CU;
    close->live_foundation.last_step_offset[actor] = 0x008CU;
    if (!scene_attach_ball(close)) goto done;
    serial_before = close->action_serial;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(close, &shot_request) || !shot_request.requested ||
        !shot_request.playback_supported || shot_request.deferred ||
        !scene_shot_is_close(close->shot_kind) || close->shot_actor != actor ||
        close->ball_holder != TECMO_GAMEPLAY_SCENE_NO_ACTOR ||
        close->action_serial != (uint16_t)(serial_before + 1U) ||
        !close->live_foundation.last_shot_request ||
        !close->live_foundation.last_shot_playback_supported ||
        close->live_foundation.last_shot_deferred) goto done;
    evidence->action17_serial_before = serial_before;
    evidence->action17_serial_after = close->action_serial;
    evidence->action17_shot_kind = (uint8_t)close->shot_kind;
    evidence->action17_shot_actor = close->shot_actor;
    evidence->action17_ball_holder = close->ball_holder;
    evidence->action17_close_shot_proved = true;

    /* A different state-0 action is outside this seam and stays untouched. */
    *nonmatch = *source;
    nonmatch->live_foundation.play_state.actor_state[actor] = 0U;
    nonmatch->live_foundation.play_state.action_state_046e[actor] = 0x18U;
    serial_before = nonmatch->action_serial;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(nonmatch, &shot_request) || shot_request.requested ||
        shot_request.deferred || nonmatch->shot_kind !=
            TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        nonmatch->action_serial != serial_before ||
        nonmatch->live_foundation.play_state.actor_state[actor] != 0U ||
        nonmatch->live_foundation.play_state.action_state_046e[actor] !=
            0x18U) goto done;
    evidence->action17_nonmatch_unaffected = true;

    /* Autonomous far/jump playback is not owned. The shallow shots.c
       candidate must be discarded and the explicitly approximate state-4
       recovery must clear action $17 without moving ownership or actors. */
    *far = *source;
    far->actors[actor].position.x = 320;
    far->actors[actor].position.y = 144;
    far->actors[actor].anchor = far->actors[actor].position;
    far->live_foundation.play_state.actor_state[actor] = 0x04U;
    far->live_foundation.play_state.action_state_046e[actor] = 0U;
    far->live_foundation.play_state.stream_offset[actor] = 0x008CU;
    far->live_foundation.last_step_offset[actor] = 0x008CU;
    if (!scene_attach_ball(far)) goto done;
    serial_before = far->action_serial;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(far, &shot_request) || shot_request.requested ||
        !shot_request.deferred || shot_request.playback_supported ||
        far->shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE ||
        far->ball_holder != actor || far->action_serial != serial_before ||
        far->live_foundation.play_state.actor_state[actor] != 0x04U ||
        far->live_foundation.play_state.action_state_046e[actor] != 0U ||
        !far->live_foundation.last_shot_request ||
        !far->live_foundation.last_shot_deferred ||
        far->live_foundation.last_shot_playback_supported) goto done;
    evidence->action17_far_state = far->live_foundation.play_state
        .actor_state[actor];
    evidence->action17_far_action = far->live_foundation.play_state
        .action_state_046e[actor];
    evidence->action17_far_recovery_proved = true;
    ok = true;
done:
    free(close);
    free(far);
    free(nonmatch);
    if (!ok) {
        live_proof_error(message, message_size,
                         "selected action-17 shot/recovery boundary failed");
    }
    return ok;
}

static bool live_proof_cpu_catch_state0(
    TecmoGameplayScene *scene,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayScene *base = NULL;
    TecmoGameplayScene *inbound = NULL;
    TecmoGameplayScene *human = NULL;
    uint8_t receiver;
    bool ok = false;
    if (scene == NULL || evidence == NULL ||
        !live_proof_live_ownership(scene, message, message_size)) return false;
    base = (TecmoGameplayScene *)malloc(sizeof(*base));
    inbound = (TecmoGameplayScene *)malloc(sizeof(*inbound));
    human = (TecmoGameplayScene *)malloc(sizeof(*human));
    if (base == NULL || inbound == NULL || human == NULL) goto done;
    *base = *scene;

    /* Automatic ordinary pass: the unrelated Home human remains assigned to
       the same actor throughout the controller-none B24F handoff. */
    if (!live_proof_make_away_automatic(scene) ||
        !live_proof_force_possession(scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        scene->live_foundation.control_mode[TECMO_GAMEPLAY_TEAM_AWAY] == 0U) {
        live_proof_error(message, message_size,
                         "automatic pass fixture ownership failed");
        goto done;
    }
    receiver = 1U;
    scene->live_foundation.candidate_actor_by_side[
        scene->live_foundation.offense_side] = receiver;
    scene->live_foundation.play_state.action_state_046e[0U] = 0x21U;
    memcpy(evidence->catch_controlled_before, scene->controlled_actor,
           sizeof(evidence->catch_controlled_before));
    memcpy(evidence->catch_controller_team_before,
           scene->launch.controller_team,
           sizeof(evidence->catch_controller_team_before));
    if (!scene_begin_cpu_pass_from_action21(scene, 0U) ||
        !live_proof_finish_pass_transport(scene)) {
        live_proof_error(message, message_size,
                         "automatic pass did not reach B24F catch");
        goto done;
    }
    evidence->catch_pass_receiver = receiver;
    memcpy(evidence->catch_controlled_after, scene->controlled_actor,
           sizeof(evidence->catch_controlled_after));
    memcpy(evidence->catch_controller_team_after,
           scene->launch.controller_team,
           sizeof(evidence->catch_controller_team_after));
    if (memcmp(evidence->catch_controlled_before,
               evidence->catch_controlled_after,
               sizeof(evidence->catch_controlled_before)) != 0 ||
        memcmp(evidence->catch_controller_team_before,
               evidence->catch_controller_team_after,
               sizeof(evidence->catch_controller_team_before)) != 0 ||
        !live_proof_cpu_catch_progression(
            scene, receiver, evidence, message, message_size)) goto done;
    evidence->cpu_catch_pass_proved = true;

    /* The state-6 branch is a separate source lifecycle guard. It proves a
       selected automatic holder cannot become inert merely because an opcode
       installed a wait; it is not attributed to the $00D7 catch route. */
    if (!live_proof_selected_wait_state6(
            scene, receiver, evidence, message, message_size)) goto done;
    if (!live_proof_action17_boundaries(
            scene, receiver, evidence, message, message_size)) goto done;

    /* Inbound uses the same shared typed catch helper. Its formation setup is
       fixture-shaped, while the final B24F->$96B6 continuation is the same
       production transaction proved above. */
    *inbound = *base;
    if (!live_proof_make_away_automatic(inbound) ||
        !live_proof_force_possession(
            inbound, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
        inbound->live_foundation.control_mode[
            TECMO_GAMEPLAY_TEAM_AWAY] == 0U ||
        !scene_begin_inbound(inbound, TECMO_GAMEPLAY_TEAM_AWAY)) {
        live_proof_error(message, message_size,
                         "automatic inbound fixture setup failed");
        goto done;
    }
    receiver = inbound->inbound_state.receiver;
    if (!live_proof_finish_inbound_transport(inbound) ||
        inbound->ball_holder != receiver ||
        inbound->live_foundation.primary_actor != receiver ||
        inbound->live_foundation.play_state.actor_state[receiver] != 0x04U ||
        inbound->live_foundation.play_state.stream_offset[receiver] !=
            LIVE_PROOF_CATCH_ROUTE_OFFSET ||
        inbound->live_foundation.play_state.action_state_046e[receiver] !=
            0x18U) {
        live_proof_error(message, message_size,
                         "automatic inbound catch continuation failed");
        goto done;
    }
    evidence->catch_inbound_receiver = receiver;
    evidence->cpu_catch_inbound_proved = true;

    /* Human catch intentionally stops at B24F state 0/action 0. This guards
       against broad normalization that would route a controlled holder
       through the automatic $96B6 continuation. */
    *human = *base;
    if (!live_proof_force_possession(
            human, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        live_proof_error(message, message_size,
                         "human catch fixture ownership failed");
        goto done;
    }
    receiver = human->live_foundation.candidate_actor_by_side[
        human->live_foundation.offense_side];
    if (!scene_begin_pass(human, 0U, receiver) ||
        !live_proof_finish_pass_transport(human) ||
        human->ball_holder != receiver ||
        human->controlled_actor[0U] != receiver ||
        human->live_foundation.play_state.actor_state[receiver] != 0U ||
        human->live_foundation.play_state.action_state_046e[receiver] != 0U) {
        live_proof_error(message, message_size,
                         "human B24F state-0 endpoint was not preserved");
        goto done;
    }
    evidence->catch_human_receiver = receiver;
    evidence->catch_human_state = human->live_foundation.play_state
        .actor_state[receiver];
    evidence->catch_human_action = human->live_foundation.play_state
        .action_state_046e[receiver];
    evidence->human_catch_state0_proved = true;
    evidence->cpu_catch_state0_proved = true;
    ok = live_proof_live_ownership(scene, message, message_size);
done:
    free(base);
    free(inbound);
    free(human);
    if (!ok && message != NULL && message[0] == '\0') {
        live_proof_error(message, message_size,
                         "CPU catch state-0 proof rejected");
    }
    return ok;
}

static bool live_proof_cpu_auto_pass_stream(
    TecmoGameplayScene *scene,
    uint8_t checkpoint,
    LiveProofEventEvidence *evidence,
    char *message,
    size_t message_size)
{
    TecmoGameplayLiveFoundation candidate;
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint8_t primary;
    uint8_t receiver;
    size_t actor;
    size_t update;
    if (scene == NULL || evidence == NULL || checkpoint < 1U ||
        checkpoint > 4U) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass checkpoint invalid");
    }
    live_proof_controls_neutral(&p1);
    live_proof_controls_neutral(&p2);
    if (!live_proof_make_away_automatic(scene) ||
        !live_proof_force_possession(
            scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass ownership setup failed");
    }
    candidate = scene->live_foundation;
    primary = candidate.primary_actor;
    receiver = candidate.candidate_actor_by_side[candidate.offense_side];
    if (primary != scene->ball_holder || primary >= 10U || receiver >= 10U ||
        receiver == primary ||
        scene->actors[receiver].team != scene->state.possession) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass actors unavailable");
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        candidate.play_state.actor_state[actor] = 0x06U;
        candidate.play_state.wait_counter[actor] = 0xFFU;
        candidate.deferred[actor] = false;
        candidate.deferred_reason[actor] =
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
    }
    candidate.play_state.actor_state[primary] = 0x04U;
    candidate.play_state.wait_counter[primary] = 0U;
    candidate.play_state.action_state_046e[primary] = 0U;
    candidate.play_state.stream_offset[primary] =
        LIVE_PROOF_AUTO_PASS_STREAM_OFFSET;
    candidate.last_step_offset[primary] = LIVE_PROOF_AUTO_PASS_STREAM_OFFSET;
    if (!tecmo_gameplay_live_foundation_valid(
            &scene->cpu_steering_assets, &candidate)) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass parked foundation invalid");
    }
    scene->live_foundation = candidate;
    if (!scene_attach_ball(scene)) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass ball attach failed");
    }
    evidence->cpu_auto_pass_checkpoint = checkpoint;
    evidence->cpu_auto_pass_passer = primary;
    evidence->cpu_auto_pass_receiver = receiver;
    evidence->cpu_auto_pass_stream[0U] = LIVE_PROOF_AUTO_PASS_STREAM_OFFSET;
    evidence->cpu_auto_pass_passer_start = scene->actors[primary].position;
    evidence->cpu_auto_pass_receiver_start = scene->actors[receiver].position;

#define AUTO_PASS_UPDATE_OR_REJECT(text)                                      \
    do {                                                                       \
        if (!tecmo_gameplay_scene_update(scene, &p1, &p2)) {                  \
            return live_proof_reject(message, message_size, (text));          \
        }                                                                      \
        ++evidence->cpu_auto_pass_updates;                                     \
    } while (0)

    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass opcode-5 update failed");
    evidence->cpu_auto_pass_stream[1U] =
        scene->live_foundation.play_state.stream_offset[primary];
    evidence->cpu_auto_pass_action_after_opcode5 =
        scene->live_foundation.play_state.action_state_046e[primary];
    evidence->cpu_auto_pass_passer_after_opcode5 =
        scene->actors[primary].position;
    if (evidence->cpu_auto_pass_stream[1U] != 0x0181U ||
        evidence->cpu_auto_pass_action_after_opcode5 != 0x18U ||
        scene->live_foundation.deferred[primary]) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass opcode-5 milestone failed");
    }
    if (checkpoint == 1U) goto checkpoint_reached;

    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass opcode-9 update failed");
    evidence->cpu_auto_pass_stream[2U] =
        scene->live_foundation.play_state.stream_offset[primary];
    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass opcode-3 update failed");
    evidence->cpu_auto_pass_stream[3U] =
        scene->live_foundation.play_state.stream_offset[primary];
    evidence->cpu_auto_pass_wait[0U] =
        scene->live_foundation.play_state.wait_counter[primary];
    if (evidence->cpu_auto_pass_stream[2U] != 0x0186U ||
        evidence->cpu_auto_pass_stream[3U] != 0x018BU ||
        evidence->cpu_auto_pass_wait[0U] != 6U) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass wait seed failed");
    }
    for (update = 1U; update < 7U; ++update) {
        AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass wait update failed");
        evidence->cpu_auto_pass_wait[update] =
            scene->live_foundation.play_state.wait_counter[primary];
    }
    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass opcode-23 update failed");
    evidence->cpu_auto_pass_stream[4U] =
        scene->live_foundation.play_state.stream_offset[primary];
    evidence->cpu_auto_pass_action_after_opcode23 =
        scene->live_foundation.play_state.action_state_046e[primary];
    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass opcode-6 update failed");
    evidence->cpu_auto_pass_stream[5U] =
        scene->live_foundation.play_state.stream_offset[primary];
    evidence->cpu_auto_pass_action_after_opcode6 =
        scene->live_foundation.play_state.action_state_046e[primary];
    receiver = scene->live_foundation.candidate_actor_by_side[
        scene->live_foundation.offense_side];
    if (evidence->cpu_auto_pass_stream[4U] != 0x0190U ||
        evidence->cpu_auto_pass_stream[5U] != 0x0190U ||
        evidence->cpu_auto_pass_action_after_opcode23 != 0x19U ||
        evidence->cpu_auto_pass_action_after_opcode6 != 0x10U ||
        scene->live_foundation.deferred[primary] || scene_pass_active(scene) ||
        receiver >= 10U || receiver == primary ||
        scene->actors[receiver].team != scene->state.possession) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass action-10 milestone failed");
    }
    /* Scene state observes action $10 but does not retain `$0478`. The paired
       `$0478=$13` write is therefore only an inference from the separately
       focused canonical opcode-6 executor/state-flow tests, never a claimed
       scene observation. */
    evidence->cpu_auto_pass_object13_inferred = true;
    evidence->cpu_auto_pass_receiver = receiver;
    evidence->cpu_auto_pass_receiver_start = scene->actors[receiver].position;
    if (checkpoint == 2U) goto checkpoint_reached;

    AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass gather update failed");
    evidence->cpu_auto_pass_action_gather =
        scene->live_foundation.play_state.action_state_046e[primary];
    evidence->cpu_auto_pass_ball_gather = scene->ball_position;
    if (scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_GATHER ||
        scene->pass_state.packed_animation_state != 0x32U ||
        scene->pass_state.passer != primary ||
        scene->pass_state.receiver != receiver ||
        evidence->cpu_auto_pass_action_gather != 0x0FU) {
        if (message != NULL && message_size != 0U) {
            (void)snprintf(
                message, message_size,
                "CPU auto-pass gather milestone failed: phase=%u packed=%u passer=%u receiver=%u action=%u expected=%u/%u",
                (unsigned)scene->pass_state.phase,
                (unsigned)scene->pass_state.packed_animation_state,
                (unsigned)scene->pass_state.passer,
                (unsigned)scene->pass_state.receiver,
                (unsigned)evidence->cpu_auto_pass_action_gather,
                (unsigned)primary, (unsigned)receiver);
        }
        return false;
    }
    if (checkpoint == 3U) goto checkpoint_reached;

    for (update = 0U; update < 5U; ++update) {
        AUTO_PASS_UPDATE_OR_REJECT("CPU auto-pass release update failed");
    }
    if (scene->pass_state.phase != TECMO_GAMEPLAY_SCENE_PASS_FLIGHT ||
        scene->pass_state.flight_frame == 0U ||
        scene->pass_state.flight_duration == 0U ||
        (scene->ball_position.x_q8 == evidence->cpu_auto_pass_ball_gather.x_q8 &&
         scene->ball_position.y_q8 == evidence->cpu_auto_pass_ball_gather.y_q8)) {
        return live_proof_reject(message, message_size,
                                 "CPU auto-pass visible flight failed");
    }

checkpoint_reached:
    evidence->cpu_auto_pass_phase = (uint8_t)scene->pass_state.phase;
    evidence->cpu_auto_pass_packed = scene->pass_state.packed_animation_state;
    evidence->cpu_auto_pass_flight_frame = scene->pass_state.flight_frame;
    evidence->cpu_auto_pass_flight_duration = scene->pass_state.flight_duration;
    evidence->cpu_auto_pass_receiver_checkpoint =
        scene->actors[receiver].position;
    evidence->cpu_auto_pass_passer_checkpoint = scene->actors[primary].position;
    evidence->cpu_auto_pass_ball_checkpoint = scene->ball_position;
    evidence->cpu_auto_pass_non_deferred =
        !scene->live_foundation.deferred[primary];
    evidence->cpu_auto_pass_stream_proved = true;
#undef AUTO_PASS_UPDATE_OR_REJECT
    return live_proof_live_ownership(scene, message, message_size);
}

static bool live_proof_apply_event(TecmoGameplayScene *scene,
                                   const char *event,
                                   LiveProofEventEvidence *evidence,
                                   char *message,
                                   size_t message_size)
{
    TecmoControlFrame p1;
    TecmoControlFrame p2;
    uint16_t offball_capture_frame;
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
    if (live_proof_offball_capture_frame(
            event, &offball_capture_frame)) {
        return live_proof_capture_shot_offball(
            scene, offball_capture_frame, evidence, message, message_size);
    }
    if (strcmp(event, "actor-command-assignment-deferred") == 0) {
        return live_proof_observe_actor_command_assignment_deferred(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-route-state5") == 0) {
        return live_proof_cpu_route_state5(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-catch-state0") == 0) {
        return live_proof_cpu_catch_state0(
            scene, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-auto-pass-opcode5") == 0) {
        return live_proof_cpu_auto_pass_stream(
            scene, 1U, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-auto-pass-action10") == 0) {
        return live_proof_cpu_auto_pass_stream(
            scene, 2U, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-auto-pass-gather") == 0) {
        return live_proof_cpu_auto_pass_stream(
            scene, 3U, evidence, message, message_size);
    }
    if (strcmp(event, "cpu-auto-pass-stream") == 0) {
        return live_proof_cpu_auto_pass_stream(
            scene, 4U, evidence, message, message_size);
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
            "\"cpu_auto_pass_stream\":{\"proved\":%s,"
            "\"checkpoint\":%u,\"fixture\":{"
            "\"selected_cursor\":\"parked at canonical $017C\","
            "\"selected_initial\":{\"state\":4,\"wait\":0,\"action\":0},"
            "\"other_actors\":\"suspended at state6/waitFF for proof isolation\","
            "\"production_play_selection\":false},"
            "\"upstream_play_selection_claimed\":false,"
            "\"nondeferred\":%s,\"passer\":%u,\"receiver\":%u,"
            "\"updates\":%u,\"records\":[[\"017C\",5],[\"018B\",23],[\"0190\",6]],"
            "\"stream\":[\"%04X\",\"%04X\",\"%04X\",\"%04X\",\"%04X\",\"%04X\"],"
            "\"wait\":[%u,%u,%u,%u,%u,%u,%u],"
            "\"actions\":{\"opcode5\":%u,\"opcode23\":%u,"
            "\"opcode6\":%u,\"gather\":%u},"
            "\"opcode6_object10_state\":{\"inferred\":%s,"
            "\"observed_in_scene\":false,\"value\":19,"
            "\"provenance\":\"canonical TGAI-3 opcode-6 executor and scene action10 state-flow tests\"},"
            "\"pass\":{\"phase\":%u,\"packed\":%u,"
            "\"flight_frame\":%u,\"flight_duration\":%u},"
            "\"positions\":{\"passer_start\":[%d,%d],"
            "\"passer_after_opcode5\":[%d,%d],"
            "\"passer_checkpoint\":[%d,%d],"
            "\"receiver_start\":[%d,%d],\"receiver_checkpoint\":[%d,%d],"
            "\"ball_gather_q8\":[%d,%d],\"ball_checkpoint_q8\":[%d,%d]}},"
            "\"cpu_route_state5\":{\"proved\":%s,"
            "\"scope\":\"native LIVE integration; not ROM-frame parity\","
            "\"extra_adjust_admission\":\"typed no-controller native approximation; not raw $030C/$030D parity\","
            "\"actor\":%u,\"record_offset\":\"%04X\","
            "\"stream\":[\"%04X\",\"%04X\"],"
            "\"target_snapshot\":[%d,%d],"
            "\"ball_after_launch\":[%d,%d],\"target_frozen\":%s,"
            "\"actor_launch\":[%d,%d],\"actor_mid\":[%d,%d],"
            "\"duration\":%u,\"timer_mid\":%u,"
            "\"horizontal_q6\":[%u,%u],\"depth_q6\":[%u,%u],"
            "\"decision_serial\":[%u,%u],"
            "\"no_tgmo_double_step\":%s,\"parity\":{"
            "\"low_bit1_finish\":%s,\"low_bit0_extra_tick\":%s,"
            "\"high_bit0_finish\":%s,\"high_bit1_extra_tick\":%s}},"
            "\"cpu_catch_state0\":{\"proved\":%s,"
            "\"scope\":\"typed production handoff; screenshot presentation-only\","
            "\"source\":\"Bank05 $B24F->$B2EC->$96B6-$9708\","
            "\"route_choice\":\"chosen source-valid $00D7 long-route approximation; raw $0373/$0095/$0094 unavailable\","
            "\"state0_intermediate_runtime_observable\":false,"
            "\"automatic_pass\":%s,\"automatic_inbound\":%s,"
            "\"human_state0_endpoint\":%s,\"selected_wait_state6\":%s,"
            "\"receivers\":{\"pass\":%u,\"inbound\":%u,\"human\":%u},"
            "\"catch\":{\"source_state0\":%u,\"automatic_state\":%u,"
            "\"human_state\":%u,\"automatic_action_046e\":%u,"
            "\"human_action_046e\":%u,\"stream\":\"%04X\"},"
            "\"progression\":{\"stream_after_fetch\":\"%04X\","
            "\"last_step_after_fetch\":\"%04X\","
            "\"step_serial\":[%u,%u,%u],"
            "\"decision_serial\":[%u,%u,%u],"
            "\"position\":[[%d,%d],[%d,%d],[%d,%d]],"
            "\"source_target\":[%d,%d],"
            "\"opcode21\":{\"exact_typed_time_inputs\":%s,"
            "\"plus5_input\":[%u,%u,%u],"
            "\"plus10_input\":[%u,%u,%u],"
            "\"raw_007e_bit1_exact\":%s,"
            "\"whole_gate_exact\":true,\"plus5_stream\":\"%04X\","
            "\"plus10_stream\":\"%04X\"}},"
            "\"controller_assignment\":{\"controlled_before\":[%u,%u],"
            "\"controlled_after\":[%u,%u],"
            "\"team_before\":[%u,%u],\"team_after\":[%u,%u],"
            "\"automatic_handoff_unchanged\":true},"
            "\"wait_state6\":{\"sequence\":[%u,%u,%u,%u,%u,%u,%u,%u],"
            "\"stream\":[\"%04X\",\"%04X\"]},"
            "\"action17\":{\"close_shot\":%s,\"far_recovery\":%s,"
            "\"nonmatch_unaffected\":%s,"
            "\"source\":\"Bank05 $81F2-$822F; action $17 -> $8A6D->$8ACE\","
            "\"fixture_record\":\"$008C exact opcode 9 state0/action17\","
            "\"close_admission\":\"native adapter; $8ACE raw $0478/$0499/$007E gates unavailable\","
            "\"close\":{\"action_serial\":[%u,%u],\"shot_kind\":%u,"
            "\"shot_actor\":%u,\"ball_holder\":%u},"
            "\"far\":{\"state\":%u,\"action_046e\":%u,"
            "\"classification\":\"state4/action0 native approximation; autonomous far/jump playback unavailable\"}}},"
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
            evidence->cpu_auto_pass_stream_proved ? "true" : "false",
            (unsigned)evidence->cpu_auto_pass_checkpoint,
            evidence->cpu_auto_pass_non_deferred ? "true" : "false",
            (unsigned)evidence->cpu_auto_pass_passer,
            (unsigned)evidence->cpu_auto_pass_receiver,
            (unsigned)evidence->cpu_auto_pass_updates,
            (unsigned)evidence->cpu_auto_pass_stream[0U],
            (unsigned)evidence->cpu_auto_pass_stream[1U],
            (unsigned)evidence->cpu_auto_pass_stream[2U],
            (unsigned)evidence->cpu_auto_pass_stream[3U],
            (unsigned)evidence->cpu_auto_pass_stream[4U],
            (unsigned)evidence->cpu_auto_pass_stream[5U],
            (unsigned)evidence->cpu_auto_pass_wait[0U],
            (unsigned)evidence->cpu_auto_pass_wait[1U],
            (unsigned)evidence->cpu_auto_pass_wait[2U],
            (unsigned)evidence->cpu_auto_pass_wait[3U],
            (unsigned)evidence->cpu_auto_pass_wait[4U],
            (unsigned)evidence->cpu_auto_pass_wait[5U],
            (unsigned)evidence->cpu_auto_pass_wait[6U],
            (unsigned)evidence->cpu_auto_pass_action_after_opcode5,
            (unsigned)evidence->cpu_auto_pass_action_after_opcode23,
            (unsigned)evidence->cpu_auto_pass_action_after_opcode6,
            (unsigned)evidence->cpu_auto_pass_action_gather,
            evidence->cpu_auto_pass_object13_inferred ? "true" : "false",
            (unsigned)evidence->cpu_auto_pass_phase,
            (unsigned)evidence->cpu_auto_pass_packed,
            (unsigned)evidence->cpu_auto_pass_flight_frame,
            (unsigned)evidence->cpu_auto_pass_flight_duration,
            (int)evidence->cpu_auto_pass_passer_start.x,
            (int)evidence->cpu_auto_pass_passer_start.y,
            (int)evidence->cpu_auto_pass_passer_after_opcode5.x,
            (int)evidence->cpu_auto_pass_passer_after_opcode5.y,
            (int)evidence->cpu_auto_pass_passer_checkpoint.x,
            (int)evidence->cpu_auto_pass_passer_checkpoint.y,
            (int)evidence->cpu_auto_pass_receiver_start.x,
            (int)evidence->cpu_auto_pass_receiver_start.y,
            (int)evidence->cpu_auto_pass_receiver_checkpoint.x,
            (int)evidence->cpu_auto_pass_receiver_checkpoint.y,
            (int)evidence->cpu_auto_pass_ball_gather.x_q8,
            (int)evidence->cpu_auto_pass_ball_gather.y_q8,
            (int)evidence->cpu_auto_pass_ball_checkpoint.x_q8,
            (int)evidence->cpu_auto_pass_ball_checkpoint.y_q8,
            evidence->cpu_route_state5_proved ? "true" : "false",
            (unsigned)evidence->route_actor,
            (unsigned)evidence->route_record_offset,
            (unsigned)evidence->route_stream_before,
            (unsigned)evidence->route_stream_after,
            (int)evidence->route_target_snapshot.x,
            (int)evidence->route_target_snapshot.y,
            (int)evidence->route_ball_after_launch.x,
            (int)evidence->route_ball_after_launch.y,
            evidence->route_target_frozen ? "true" : "false",
            (int)evidence->route_actor_launch_position.x,
            (int)evidence->route_actor_launch_position.y,
            (int)evidence->route_actor_mid_position.x,
            (int)evidence->route_actor_mid_position.y,
            (unsigned)evidence->route_duration,
            (unsigned)evidence->route_timer_mid,
            (unsigned)evidence->route_horizontal_q6_launch,
            (unsigned)evidence->route_horizontal_q6_mid,
            (unsigned)evidence->route_depth_q6_launch,
            (unsigned)evidence->route_depth_q6_mid,
            (unsigned)evidence->route_decision_serial_before,
            (unsigned)evidence->route_decision_serial_after,
            evidence->route_no_tgmo_double_step ? "true" : "false",
            evidence->route_low_bit1_finished ? "true" : "false",
            evidence->route_low_bit0_extra_tick ? "true" : "false",
            evidence->route_high_bit0_finished ? "true" : "false",
            evidence->route_high_bit1_extra_tick ? "true" : "false",
            evidence->cpu_catch_state0_proved ? "true" : "false",
            evidence->cpu_catch_pass_proved ? "true" : "false",
            evidence->cpu_catch_inbound_proved ? "true" : "false",
            evidence->human_catch_state0_proved ? "true" : "false",
            evidence->selected_wait_state6_proved ? "true" : "false",
            (unsigned)evidence->catch_pass_receiver,
            (unsigned)evidence->catch_inbound_receiver,
            (unsigned)evidence->catch_human_receiver,
            (unsigned)evidence->catch_source_state0,
            (unsigned)evidence->catch_automatic_state,
            (unsigned)evidence->catch_human_state,
            (unsigned)evidence->catch_automatic_action,
            (unsigned)evidence->catch_human_action,
            (unsigned)evidence->catch_automatic_stream,
            (unsigned)evidence->catch_stream_after_fetch,
            (unsigned)evidence->catch_last_step_after_fetch,
            (unsigned)evidence->catch_step_serial_before,
            (unsigned)evidence->catch_step_serial_after_fetch,
            (unsigned)evidence->catch_step_serial_after_move,
            (unsigned)evidence->catch_decision_serial_before,
            (unsigned)evidence->catch_decision_serial_after_fetch,
            (unsigned)evidence->catch_decision_serial_after_move,
            (int)evidence->catch_position_at_transfer.x,
            (int)evidence->catch_position_at_transfer.y,
            (int)evidence->catch_position_after_fetch.x,
            (int)evidence->catch_position_after_fetch.y,
            (int)evidence->catch_position_after_move.x,
            (int)evidence->catch_position_after_move.y,
            (int)evidence->catch_source_target.x,
            (int)evidence->catch_source_target.y,
            evidence->catch_gate_exact_time_inputs ? "true" : "false",
            (unsigned)evidence->catch_gate_plus5_shot_clock,
            (unsigned)evidence->catch_gate_plus5_clock_minutes,
            (unsigned)evidence->catch_gate_plus5_clock_seconds,
            (unsigned)evidence->catch_gate_plus10_shot_clock,
            (unsigned)evidence->catch_gate_plus10_clock_minutes,
            (unsigned)evidence->catch_gate_plus10_clock_seconds,
            evidence->catch_gate_007e_bit1_exact ? "true" : "false",
            (unsigned)evidence->catch_stream_after_gate_plus5,
            (unsigned)evidence->catch_stream_after_gate_plus10,
            (unsigned)evidence->catch_controlled_before[0U],
            (unsigned)evidence->catch_controlled_before[1U],
            (unsigned)evidence->catch_controlled_after[0U],
            (unsigned)evidence->catch_controlled_after[1U],
            (unsigned)evidence->catch_controller_team_before[0U],
            (unsigned)evidence->catch_controller_team_before[1U],
            (unsigned)evidence->catch_controller_team_after[0U],
            (unsigned)evidence->catch_controller_team_after[1U],
            (unsigned)evidence->catch_wait_sequence[0U],
            (unsigned)evidence->catch_wait_sequence[1U],
            (unsigned)evidence->catch_wait_sequence[2U],
            (unsigned)evidence->catch_wait_sequence[3U],
            (unsigned)evidence->catch_wait_sequence[4U],
            (unsigned)evidence->catch_wait_sequence[5U],
            (unsigned)evidence->catch_wait_sequence[6U],
            (unsigned)evidence->catch_wait_sequence[7U],
            (unsigned)evidence->catch_wait_stream_before,
            (unsigned)evidence->catch_wait_stream_after,
            evidence->action17_close_shot_proved ? "true" : "false",
            evidence->action17_far_recovery_proved ? "true" : "false",
            evidence->action17_nonmatch_unaffected ? "true" : "false",
            (unsigned)evidence->action17_serial_before,
            (unsigned)evidence->action17_serial_after,
            (unsigned)evidence->action17_shot_kind,
            (unsigned)evidence->action17_shot_actor,
            (unsigned)evidence->action17_ball_holder,
            (unsigned)evidence->action17_far_state,
            (unsigned)evidence->action17_far_action,
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
            "\"shot_offball\":{\"proved\":%s,"
            "\"entrypoint\":\"tecmo_gameplay_scene_update/normal-B-rattle\","
            "\"capture_frame\":%u,\"ball_holder\":%u,"
            "\"route_actor\":%u,\"route_position\":[[%d,%d],[%d,%d]],"
            "\"controlled_actor\":%u,"
            "\"controlled_position\":[[%d,%d],[%d,%d]],"
            "\"a9da\":{\"observed\":%s,\"chosen_actor\":%u,"
            "\"last_step_after\":\"%04X\"},"
            "\"fixture\":\"source-shaped state5 route plus held direction; shot launch/outcome and every temporal advance use production update\"},",
            evidence->shot_offball_capture_proved ? "true" : "false",
            (unsigned)evidence->shot_offball_capture_frame,
            (unsigned)scene->ball_holder,
            (unsigned)evidence->shot_offball_route_actor,
            (int)evidence->shot_offball_route_start.x,
            (int)evidence->shot_offball_route_start.y,
            (int)evidence->shot_offball_route_capture.x,
            (int)evidence->shot_offball_route_capture.y,
            (unsigned)evidence->shot_offball_controlled_actor,
            (int)evidence->shot_offball_controlled_start.x,
            (int)evidence->shot_offball_controlled_start.y,
            (int)evidence->shot_offball_controlled_capture.x,
            (int)evidence->shot_offball_controlled_capture.y,
            evidence->shot_offball_a9da_observed ? "true" : "false",
            (unsigned)evidence->shot_offball_a9da_chosen_actor,
            (unsigned)evidence->shot_offball_a9da_stream_after)) {
        return false;
    }
    if (!live_proof_append(
            message, message_size, &length,
            "\"actor_command_assignment\":{\"deferred_diagnostic\":%s,"
            "\"emitted\":%s,"
            "\"caller_identity\":\"%s\","
            "\"no_op_reason\":\"%s\","
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
            "\"state17_production\":{\"observed\":%s,"
            "\"dispatch_handler\":\"%04X\",\"raw_0499\":%u,"
            "\"opcode20_actor_mask\":%u,"
            "\"same_update_latch_consumed\":%s},"
            "\"screenshot_scope\":\"ordinary native pretip-to-live flow; not A023 gameplay parity\"},",
            evidence->actor_command_assignment_deferred ? "true" : "false",
            evidence->actor_command_assignment_b783_observed ? "true" : "false",
            evidence->actor_command_assignment_b783_observed
                ? "object-state17-B783" : "none",
            evidence->actor_command_assignment_b783_observed
                ? "none" : "missing-source-shaped-object-dispatch-inputs",
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
            (unsigned)evidence->actor_command_assignment_defender_state_after,
            evidence->actor_command_assignment_b783_observed ? "true" : "false",
            (unsigned)evidence->actor_command_assignment_b783_handler_cpu,
            (unsigned)evidence->actor_command_assignment_b783_raw_0499,
            (unsigned)evidence->actor_command_assignment_b783_opcode20_mask,
            evidence->actor_command_assignment_b783_observed ? "true" : "false")) {
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
