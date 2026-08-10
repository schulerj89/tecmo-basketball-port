#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_controls.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_candidate_selection.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_pretip.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_scene_internal.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_memory.h"
#include "tecmo_win32_keys.h"
#include "png_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

#define TECMO_CLI_PRETIP_CAPTURE_FRAME 452U
#define TECMO_CLI_PRETIP_SIMULATION_START_FRAME 481U
#define TECMO_CLI_PRETIP_LIVE_START_FRAME 606U
#define TECMO_CLI_TIPOFF_PROOF_LAST_FRAME \
    (TECMO_CLI_PRETIP_LIVE_START_FRAME + 20U)
#define TECMO_CLI_FREE_THROW_CHECKPOINT_FRAME \
    (TECMO_CLI_PRETIP_LIVE_START_FRAME + 5U)

typedef struct TecmoCliGameplayCheckpointConfig {
    unsigned checkpoint;
    uint8_t away_team;
    uint8_t home_team;
    bool jump;
    bool jump_make;
    bool jump_rattle;
    bool dunk;
    bool layup;
    bool pretip_checkpoint;
    bool live_start;
    bool facing_checkpoint;
    bool tipoff_proof;
    bool tipoff_continuity;
    bool tipoff_away_win;
    bool ball_bounce;
    bool cpu_steering;
    bool pass_handoff_proof;
    bool directional_selection_proof;
    bool shot_clock_violation;
    bool out_of_bounds_violation;
    bool backcourt_violation;
    int possession_slice;
    int free_throw_orientation;
} TecmoCliGameplayCheckpointConfig;

typedef struct TecmoCliTipoffInputEvidence {
    bool raw_x_down;
    bool raw_x_up;
    bool raw_x_down_logical;
    bool raw_x_up_logical;
    bool fast_x_effective_cancel;
    bool fast_x_effective_pressed;
    bool fast_x_post_bridge_cancel;
    bool current_effective_cancel;
    bool current_effective_pressed;
    bool current_effective_released;
    bool literal_b_mapped;
    bool bridge_used;
    uint32_t fast_x_pulse_frame;
    unsigned bridge_begin_count;
    unsigned bridge_end_count;
    unsigned bridge_update_players_count;
} TecmoCliTipoffInputEvidence;

static bool gameplay_checkpoint_hud_player(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam team,
    uint8_t *actor_out,
    const TecmoTeamDataPlayer **player_out)
{
    uint8_t candidate = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t reference = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    uint8_t team_id;
    size_t controller;

    if (scene == NULL || actor_out == NULL || player_out == NULL ||
        (team != TECMO_GAMEPLAY_TEAM_AWAY &&
         team != TECMO_GAMEPLAY_TEAM_HOME) ||
        scene->pretip_team_data == NULL ||
        !scene->pretip_team_data->available) {
        return false;
    }
    for (controller = 0U;
         controller < TECMO_GAMEPLAY_CONTROLLER_COUNT; ++controller) {
        if (scene->launch.controller_team[controller] == (uint8_t)team) {
            candidate = scene->controlled_actor[controller];
            break;
        }
    }
    if (candidate == TECMO_GAMEPLAY_SCENE_NO_ACTOR) {
        if (scene->ball_holder < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
            scene->actors[scene->ball_holder].active) {
            reference = scene->ball_holder;
        } else if (scene->shot_actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT &&
                   scene->actors[scene->shot_actor].active) {
            reference = scene->shot_actor;
        }
        candidate = (uint8_t)((uint8_t)team *
                              TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT +
                              (reference < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT
                                   ? reference %
                                         TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                                   : 0U));
    }
    if (candidate >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        !scene->actors[candidate].active ||
        scene->actors[candidate].team != (uint8_t)team ||
        scene->actors[candidate].roster_index >=
            TECMO_TEAM_DATA_PLAYERS_PER_TEAM) {
        return false;
    }
    team_id = team == TECMO_GAMEPLAY_TEAM_AWAY
                  ? scene->launch.away_team : scene->launch.home_team;
    if (team_id >= TECMO_TEAM_DATA_TEAM_COUNT) return false;
    *actor_out = candidate;
    *player_out = &scene->pretip_team_data->players[team_id][
        scene->actors[candidate].roster_index];
    return (*player_out)->name[0] != '\0';
}

static bool gameplay_checkpoint_compact_name(
    const char source[21], char compact[21])
{
    size_t index;

    if (source == NULL || compact == NULL) return false;
    for (index = 0U; index + 1U < 21U && source[index] != '\0'; ++index)
        compact[index] = source[index] == ' ' ? '_' : source[index];
    if (index == 0U || index >= 21U || source[index] != '\0') return false;
    compact[index] = '\0';
    return true;
}

static bool gameplay_checkpoint_run_fast_x_bridge(
    TecmoRuntime *runtime,
    TecmoWin32KeyboardState *keyboard,
    TecmoControls *controls,
    TecmoCliTipoffInputEvidence *evidence)
{
    TecmoWin32KeyBinding binding;
    const TecmoInput *player_one;
    const TecmoInput *player_two;
    bool logical_down;
    uint32_t frame_before;
    size_t control_count = TECMO_GAMEPLAY_CONTROLLER_COUNT;

    if (runtime == NULL || keyboard == NULL || controls == NULL ||
        evidence == NULL ||
        tecmo_win32_translate_key((uint32_t)'B', &binding)) {
        return false;
    }
    evidence->literal_b_mapped = false;
    if (!tecmo_win32_keyboard_update(
            keyboard, (uint32_t)'X', true, &binding, &logical_down) ||
        binding.player_index != 0U || binding.button != TECMO_CONTROL_CANCEL ||
        !logical_down) {
        return false;
    }
    evidence->raw_x_down = true;
    evidence->raw_x_down_logical = logical_down;
    tecmo_controls_set_button(
        &controls[binding.player_index], binding.button, logical_down);
    if (!tecmo_win32_keyboard_update(
            keyboard, (uint32_t)'X', false, &binding, &logical_down) ||
        binding.player_index != 0U || binding.button != TECMO_CONTROL_CANCEL ||
        logical_down) {
        return false;
    }
    evidence->raw_x_up = true;
    evidence->raw_x_up_logical = logical_down;
    tecmo_controls_set_button(
        &controls[binding.player_index], binding.button, logical_down);

    tecmo_win32_keyboard_begin_controls_frame(
        keyboard, controls, control_count);
    ++evidence->bridge_begin_count;
    player_one = tecmo_controls_held(&controls[0U]);
    player_two = tecmo_controls_held(&controls[1U]);
    if (player_one == NULL || player_two == NULL) {
        tecmo_win32_keyboard_end_controls_frame(
            keyboard, controls, control_count);
        ++evidence->bridge_end_count;
        return false;
    }
    evidence->fast_x_effective_cancel = player_one->cancel;
    evidence->fast_x_effective_pressed = tecmo_controls_pressed(
        &controls[0U], TECMO_CONTROL_CANCEL);
    evidence->current_effective_cancel = player_one->cancel;
    evidence->current_effective_pressed =
        evidence->fast_x_effective_pressed;
    evidence->current_effective_released = tecmo_controls_released(
        &controls[0U], TECMO_CONTROL_CANCEL);
    if (!evidence->fast_x_effective_cancel ||
        !evidence->fast_x_effective_pressed ||
        player_two->cancel) {
        tecmo_win32_keyboard_end_controls_frame(
            keyboard, controls, control_count);
        ++evidence->bridge_end_count;
        return false;
    }
    frame_before = runtime->gameplay_scene.frame;
    tecmo_runtime_update_players(runtime, player_one, player_two);
    ++evidence->bridge_update_players_count;
    tecmo_win32_keyboard_end_controls_frame(
        keyboard, controls, control_count);
    ++evidence->bridge_end_count;
    player_one = tecmo_controls_held(&controls[0U]);
    if (player_one == NULL || player_one->cancel ||
        runtime->gameplay_scene.frame != frame_before + 1U) {
        return false;
    }
    evidence->fast_x_post_bridge_cancel = player_one->cancel;
    evidence->fast_x_pulse_frame = runtime->gameplay_scene.frame;
    evidence->bridge_used = true;
    return true;
}

static bool gameplay_checkpoint_run_adapter_update(
    TecmoRuntime *runtime,
    TecmoWin32KeyboardState *keyboard,
    TecmoControls *controls,
    TecmoCliTipoffInputEvidence *evidence)
{
    const TecmoInput *player_one;
    const TecmoInput *player_two;
    uint32_t frame_before;
    size_t control_count = TECMO_GAMEPLAY_CONTROLLER_COUNT;

    if (runtime == NULL || keyboard == NULL || controls == NULL ||
        evidence == NULL) {
        return false;
    }
    frame_before = runtime->gameplay_scene.frame;
    tecmo_win32_keyboard_begin_controls_frame(
        keyboard, controls, control_count);
    ++evidence->bridge_begin_count;
    player_one = tecmo_controls_held(&controls[0U]);
    player_two = tecmo_controls_held(&controls[1U]);
    if (player_one == NULL || player_two == NULL || player_two->cancel) {
        tecmo_win32_keyboard_end_controls_frame(
            keyboard, controls, control_count);
        ++evidence->bridge_end_count;
        return false;
    }
    evidence->current_effective_cancel = player_one->cancel;
    evidence->current_effective_pressed = tecmo_controls_pressed(
        &controls[0U], TECMO_CONTROL_CANCEL);
    evidence->current_effective_released = tecmo_controls_released(
        &controls[0U], TECMO_CONTROL_CANCEL);
    tecmo_runtime_update_players(runtime, player_one, player_two);
    ++evidence->bridge_update_players_count;
    tecmo_win32_keyboard_end_controls_frame(
        keyboard, controls, control_count);
    ++evidence->bridge_end_count;
    return runtime->gameplay_scene.frame == frame_before + 1U;
}

static bool gameplay_checkpoint_goal_facing_right(
    const TecmoGameplayScene *scene,
    uint8_t team,
    bool *facing_right_out)
{
    TecmoGameplayCourtCoordinate hoop;
    if (scene == NULL || facing_right_out == NULL ||
        !tecmo_gameplay_court_orientation_team_hoop(
            &scene->court_orientation, &scene->orientation_state,
            team, &hoop)) {
        return false;
    }
    if (hoop.x == TECMO_GAMEPLAY_COURT_LEFT_HOOP_X) {
        *facing_right_out = false;
    } else if (hoop.x == TECMO_GAMEPLAY_COURT_RIGHT_HOOP_X) {
        *facing_right_out = true;
    } else {
        return false;
    }
    return true;
}

static bool parse_gameplay_render_checkpoint_mode(const char *mode_name, TecmoCliGameplayCheckpointConfig *config)
{
    unsigned checkpoint = 0U;
    uint8_t away_team = 0U;
    uint8_t home_team = 1U;
    bool jump = false;
    bool jump_make = false;
    bool jump_rattle = false;
    bool dunk = false;
    bool layup = false;
    bool pretip_checkpoint = false;
    bool live_start = false;
    bool facing_checkpoint = false;
    bool tipoff_proof = false;
    bool tipoff_continuity = false;
    bool tipoff_away_win = false;
    bool ball_bounce = false;
    bool cpu_steering = false;
    bool pass_handoff_proof = false;
    bool directional_selection_proof = false;
    bool shot_clock_violation = false;
    bool out_of_bounds_violation = false;
    bool backcourt_violation = false;
    int possession_slice = -1;
    int free_throw_orientation = -1;

    if (mode_name == NULL || config == NULL) return false;
    if (strcmp(mode_name, "gameplay-start") == 0) {
        checkpoint = 0U;
        pretip_checkpoint = true;
    } else if (strcmp(mode_name, "gameplay-pretip-bulls-pacers") == 0) {
        checkpoint = TECMO_CLI_PRETIP_SIMULATION_START_FRAME;
        away_team = 3U;
        home_team = 10U;
        pretip_checkpoint = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-pretip-frame", &checkpoint)) {
        pretip_checkpoint = true;
    } else if (strcmp(mode_name, "gameplay-live-start") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        live_start = true;
    } else if (strcmp(mode_name, "gameplay-facing-away-left") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        live_start = true;
        facing_checkpoint = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-tipoff-proof-frame", &checkpoint)) {
        away_team = 3U;
        home_team = 10U;
        tipoff_proof = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-tipoff-continuity-frame",
                   &checkpoint)) {
        away_team = 3U;
        home_team = 10U;
        tipoff_proof = true;
        tipoff_continuity = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-tip-ball-away-frame", &checkpoint)) {
        away_team = 3U;
        home_team = 10U;
        tipoff_proof = true;
        tipoff_away_win = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-tip-ball-home-frame", &checkpoint)) {
        away_team = 3U;
        home_team = 10U;
        tipoff_proof = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-ball-bounce-frame", &checkpoint)) {
        ball_bounce = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-cpu-steering-frame", &checkpoint)) {
        cpu_steering = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-pass-handoff-proof-frame",
                   &checkpoint)) {
        pass_handoff_proof = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-directional-selection-frame",
                   &checkpoint)) {
        directional_selection_proof = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-shot-clock-violation-frame",
                   &checkpoint)) {
        shot_clock_violation = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-out-of-bounds-frame",
                   &checkpoint)) {
        out_of_bounds_violation = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-backcourt-frame",
                   &checkpoint)) {
        backcourt_violation = true;
    } else if (strcmp(mode_name, "gameplay-uniform-pacers") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        away_team = 3U;
        home_team = 10U;
        live_start = true;
    } else if (strcmp(mode_name, "gameplay-possession-left") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        possession_slice = 0;
    } else if (strcmp(mode_name, "gameplay-possession-center") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        possession_slice = 1;
    } else if (strcmp(mode_name, "gameplay-possession-right") == 0) {
        checkpoint = TECMO_CLI_PRETIP_LIVE_START_FRAME;
        possession_slice = 2;
    } else if (strcmp(mode_name, "gameplay-free-throw-left") == 0) {
        checkpoint = TECMO_CLI_FREE_THROW_CHECKPOINT_FRAME;
        free_throw_orientation = 0;
    } else if (strcmp(mode_name, "gameplay-free-throw-right") == 0) {
        checkpoint = TECMO_CLI_FREE_THROW_CHECKPOINT_FRAME;
        free_throw_orientation = 1;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-frame", &checkpoint)) {
        jump = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-make-frame", &checkpoint)) {
        jump = true;
        jump_make = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-jump-rattle-frame", &checkpoint)) {
        jump = true;
        jump_rattle = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-dunk-frame", &checkpoint)) {
        dunk = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-layup-frame", &checkpoint)) {
        const char *suffix = mode_name + strlen("gameplay-layup-frame");
        /* Keep this new production checkpoint canonical: positive decimal,
           no leading zeroes, and no alternate spelling of the frame. */
        if (suffix[0] == '0' && suffix[1] != '\0') return false;
        layup = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-close-shot-frame", &checkpoint)) {
        /* Compatibility spelling for the former numeric-variant-0 mode. */
        dunk = true;
    } else {
        return false;
    }
    if (pretip_checkpoint &&
        checkpoint >= TECMO_CLI_PRETIP_LIVE_START_FRAME) {
        return false;
    }
    if (tipoff_proof &&
        (checkpoint < TECMO_CLI_PRETIP_CAPTURE_FRAME ||
         checkpoint > TECMO_CLI_TIPOFF_PROOF_LAST_FRAME)) {
        return false;
    }
    if (ball_bounce && (checkpoint == 0U || checkpoint > 15U)) {
        return false;
    }
    if (cpu_steering && (checkpoint == 0U || checkpoint > 240U)) {
        return false;
    }
    if (pass_handoff_proof && checkpoint > 2U) return false;
    if (directional_selection_proof && checkpoint > 5U) return false;
    if ((shot_clock_violation || out_of_bounds_violation ||
         backcourt_violation) &&
        checkpoint >= TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES) {
        return false;
    }
    if ((jump && (checkpoint == 0U ||
                  checkpoint >
                      (jump_make ? 111U : (jump_rattle ? 103U : 87U)))) ||
        (dunk && (checkpoint == 0U || checkpoint > 132U)) ||
        (layup && (checkpoint == 0U || checkpoint > 17U))) {
        return false;
    }


    config->checkpoint = checkpoint;
    config->away_team = away_team;
    config->home_team = home_team;
    config->jump = jump;
    config->jump_make = jump_make;
    config->jump_rattle = jump_rattle;
    config->dunk = dunk;
    config->layup = layup;
    config->pretip_checkpoint = pretip_checkpoint;
    config->live_start = live_start;
    config->facing_checkpoint = facing_checkpoint;
    config->tipoff_proof = tipoff_proof;
    config->tipoff_continuity = tipoff_continuity;
    config->tipoff_away_win = tipoff_away_win;
    config->ball_bounce = ball_bounce;
    config->cpu_steering = cpu_steering;
    config->pass_handoff_proof = pass_handoff_proof;
    config->directional_selection_proof = directional_selection_proof;
    config->shot_clock_violation = shot_clock_violation;
    config->out_of_bounds_violation = out_of_bounds_violation;
    config->backcourt_violation = backcourt_violation;
    config->possession_slice = possession_slice;
    config->free_throw_orientation = free_throw_orientation;
    return true;
}

static bool gameplay_checkpoint_report_tipoff_proof(
    const TecmoGameplayScene *scene,
    const TecmoCliTipoffInputEvidence *input_evidence)
{
    TecmoGameplaySceneCourtFrame court_frame;
    const TecmoTeamDataPlayer *away_hud_player;
    const TecmoTeamDataPlayer *home_hud_player;
    char away_hud_name[21];
    char home_hud_name[21];
    uint8_t away_actor;
    uint8_t home_actor;
    uint8_t away_hud_actor;
    uint8_t home_hud_actor;
    uint8_t left_actor;
    uint8_t right_actor;
    const TecmoGameplaySceneActor *away;
    const TecmoGameplaySceneActor *home;
    const TecmoGameplaySceneActor *left;
    const TecmoGameplaySceneActor *right;
    uint8_t away_phase = 0U;
    uint8_t home_phase = 0U;
    uint8_t away_state;
    uint8_t home_state;
    if (scene == NULL || input_evidence == NULL || !scene->active ||
        !tecmo_gameplay_scene_court_frame(scene, &court_frame)) {
        return false;
    }
    if (!gameplay_checkpoint_hud_player(
            scene, TECMO_GAMEPLAY_TEAM_AWAY,
            &away_hud_actor, &away_hud_player) ||
        !gameplay_checkpoint_hud_player(
            scene, TECMO_GAMEPLAY_TEAM_HOME,
            &home_hud_actor, &home_hud_player) ||
        !gameplay_checkpoint_compact_name(
            away_hud_player->name, away_hud_name) ||
        !gameplay_checkpoint_compact_name(
            home_hud_player->name, home_hud_name)) {
        return false;
    }
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    if (away_actor >= TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        home_actor < TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        away_actor == home_actor) {
        return false;
    }
    away = &scene->actors[away_actor];
    home = &scene->actors[home_actor];
    away_phase = scene->pretip_state.away_animation_phase;
    home_phase = scene->pretip_state.home_animation_phase;
    away_state = scene->pretip_state.away_actor_state;
    home_state = scene->pretip_state.home_actor_state;
    if (away->anchor.x == home->anchor.x) return false;
    if (away->anchor.x < home->anchor.x) {
        left_actor = away_actor;
        right_actor = home_actor;
    } else {
        left_actor = home_actor;
        right_actor = away_actor;
    }
    left = &scene->actors[left_actor];
    right = &scene->actors[right_actor];
    if (tecmo_gameplay_scene_in_pretip(scene) &&
        (!court_frame.projection.players[away_actor].visible ||
         !court_frame.projection.players[home_actor].visible)) {
        return false;
    }
    if (!tecmo_gameplay_scene_in_pretip(scene) &&
        !scene->pretip_jump_active &&
        (scene->pretip_jumper_altitude_q8[0U] != 0U ||
         scene->pretip_jumper_altitude_q8[1U] != 0U)) {
        return false;
    }
    printf(
        "tipoff-proof frame=%u pretip=%s pretip-frame=%u contest-frame=%u "
        "away-actor=%u away-world-y=%d away-screen-y=%u away-visible=%u "
        "away-selector=%u away-state=%u away-phase=%u away-pose=%u "
        "away-altitude-q8=%u away-facing-right=%u "
        "away-pose-encoded=%u away-renderer-mirror=0 away-anchor-x=%d home-actor=%u "
        "home-world-y=%d "
        "home-screen-y=%u home-visible=%u home-selector=%u home-state=%u "
        "home-phase=%u home-pose=%u "
        "home-altitude-q8=%u home-facing-right=%u home-pose-encoded=%u "
        "home-renderer-mirror=0 home-anchor-x=%d left-actor=%u left-facing-right=%u "
        "right-actor=%u right-facing-right=%u "
        "away-sampled=%u away-sample-frame=%u away-capture-clock=%u away-error=%u "
        "home-sampled=%u home-sample-frame=%u home-capture-clock=%u home-error=%u "
        "capture-source-6a=%u capture-clock=%u capture-clock-ticks=%u "
        "hud-ready=1 hud-away-actor=%u hud-home-actor=%u "
        "hud-away-name=%s hud-home-name=%s "
        "input-adapter=win32-keyboard-controls input-raw-x-down=%u "
        "input-raw-x-up=%u input-raw-x-down-logical=%u "
        "input-raw-x-up-logical=%u input-fast-x-effective-cancel=%u "
        "input-fast-x-effective-pressed=%u "
        "input-fast-x-post-bridge-cancel=%u "
        "input-current-effective-cancel=%u "
        "input-current-effective-pressed=%u "
        "input-current-effective-released=%u "
        "input-fast-x-pulse-frame=%u "
        "input-fast-x-bridge=%u input-literal-b-mapped=%u "
        "input-bridge-begin=%u "
        "input-bridge-end=%u input-bridge-update-players=%u "
        "input-direct-cancel=0 "
        "possession=%u direction=%u hoop-x=%d "
        "ball-x-q8=%ld ball-y-q8=%ld camera-x=%u fine-scroll=%u "
        "input=physical-X-fast-pulse-p1-away\n",
        scene->frame,
        tecmo_gameplay_pretip_phase_name(scene->pretip_state.phase),
        (unsigned)scene->pretip_state.phase_frame,
        (unsigned)scene->pretip_state.contest_frame,
        (unsigned)away_actor, (int)away->position.y,
        (unsigned)court_frame.projection.players[away_actor].screen_y,
        court_frame.projection.players[away_actor].visible ? 1U : 0U,
        (unsigned)scene->pretip_jumper_selector[0U],
        (unsigned)away_state, (unsigned)away_phase,
        (unsigned)away->pose_index,
        (unsigned)scene->pretip_jumper_altitude_q8[0U],
        away->facing_right ? 1U : 0U,
        away->pose_orientation_encoded ? 1U : 0U,
        (int)away->anchor.x,
        (unsigned)home_actor, (int)home->position.y,
        (unsigned)court_frame.projection.players[home_actor].screen_y,
        court_frame.projection.players[home_actor].visible ? 1U : 0U,
        (unsigned)scene->pretip_jumper_selector[1U],
        (unsigned)home_state, (unsigned)home_phase,
        (unsigned)home->pose_index,
        (unsigned)scene->pretip_jumper_altitude_q8[1U],
        home->facing_right ? 1U : 0U,
        home->pose_orientation_encoded ? 1U : 0U,
        (int)home->anchor.x,
        (unsigned)left_actor,
        left->facing_right ? 1U : 0U,
        (unsigned)right_actor,
        right->facing_right ? 1U : 0U,
        scene->pretip_state.away_tip_sampled ? 1U : 0U,
        (unsigned)scene->pretip_state.away_tip_sample_frame,
        (unsigned)scene->pretip_state.away_tip_capture_clock,
        (unsigned)scene->pretip_state.away_tip_error,
        scene->pretip_state.home_tip_sampled ? 1U : 0U,
        (unsigned)scene->pretip_state.home_tip_sample_frame,
        (unsigned)scene->pretip_state.home_tip_capture_clock,
        (unsigned)scene->pretip_state.home_tip_error,
        (unsigned)scene->pretip_state.tip_capture_source_6a,
        (unsigned)scene->pretip_state.tip_capture_clock,
        (unsigned)scene->pretip_state.tip_capture_clock_ticks,
        (unsigned)away_hud_actor,
        (unsigned)home_hud_actor,
        away_hud_name,
        home_hud_name,
        input_evidence->raw_x_down ? 1U : 0U,
        input_evidence->raw_x_up ? 1U : 0U,
        input_evidence->raw_x_down_logical ? 1U : 0U,
        input_evidence->raw_x_up_logical ? 1U : 0U,
        input_evidence->fast_x_effective_cancel ? 1U : 0U,
        input_evidence->fast_x_effective_pressed ? 1U : 0U,
        input_evidence->fast_x_post_bridge_cancel ? 1U : 0U,
        input_evidence->current_effective_cancel ? 1U : 0U,
        input_evidence->current_effective_pressed ? 1U : 0U,
        input_evidence->current_effective_released ? 1U : 0U,
        (unsigned)input_evidence->fast_x_pulse_frame,
        input_evidence->bridge_used ? 1U : 0U,
        input_evidence->literal_b_mapped ? 1U : 0U,
        input_evidence->bridge_begin_count,
        input_evidence->bridge_end_count,
        input_evidence->bridge_update_players_count,
        (unsigned)scene->state.possession,
        (unsigned)scene->orientation_state.current_direction,
        (int)scene->orientation_state.offensive_hoop.x,
        (long)scene->ball_position.x_q8,
        (long)scene->ball_position.y_q8,
        (unsigned)court_frame.projection.camera_x,
        (unsigned)court_frame.slice.viewport.fine_scroll_x);
    printf(
        "tipoff-timing frame=%u total-frame=%u simulation-tick=%u "
        "presentation-phase=%s cinematic-visible=%u ball-screen-y=%ld ball-raw-height=%u "
        "rng-threshold=%u capture-source-6a=%u capture-clock=%u "
        "capture-clock-ticks=%u capture-source-initial=%u "
        "capture-source-current=%u capture-source-mixes=%u "
        "capture-scheduler-phase=%u capture-scheduler-yields=%u "
        "capture-special-yields=%u away-latch=%u away-capture-clock=%u away-countdown=%u "
        "away-commit-frame=%u away-committed=%u away-altitude-q8=%u "
        "away-state=%u away-phase=%u away-pose=%u away-fraction=%u "
        "away-velocity-q8=%d away-apex-frame=%u away-commit-count=%u "
        "home-latch=%u home-capture-clock=%u home-countdown=%u "
        "home-commit-frame=%u home-committed=%u home-altitude-q8=%u "
        "home-state=%u home-phase=%u home-pose=%u home-fraction=%u "
        "home-velocity-q8=%d home-apex-frame=%u home-commit-count=%u "
        "ball-state=%u contact-state17=%u event-0588-bit20=%u "
        "first-cinematic-frame=%u\n",
        scene->frame, scene->pretip_state.total_frame,
        (unsigned)scene->pretip_state.simulation_tick,
        tecmo_gameplay_pretip_phase_name(scene->pretip_state.phase),
        scene->pretip_state.cinematic_visible ? 1U : 0U,
        (long)court_frame.projection.ball.screen_y,
        (unsigned)scene->pretip_state.tip_ball_high_raw,
        (unsigned)(scene->pretip_assets.tip_auto_threshold_base +
            ((scene->pretip_state.tip_rng_6a &
              scene->pretip_assets.tip_auto_threshold_mask) >>
             scene->pretip_assets.tip_auto_threshold_shift)),
        (unsigned)scene->pretip_state.tip_capture_source_6a,
        (unsigned)scene->pretip_state.tip_capture_clock,
        (unsigned)scene->pretip_state.tip_capture_clock_ticks,
        (unsigned)scene->pretip_state.tip_capture_source_6a_initial,
        (unsigned)scene->pretip_state.tip_capture_source_6a_current,
        (unsigned)scene->pretip_state.tip_capture_source_mix_count,
        (unsigned)scene->pretip_state.tip_capture_scheduler_phase,
        (unsigned)scene->pretip_state.tip_capture_scheduler_yields,
        (unsigned)scene->pretip_state.tip_capture_special_yields,
        scene->pretip_state.away_tip_sampled ? 1U : 0U,
        (unsigned)scene->pretip_state.away_tip_capture_clock,
        (unsigned)scene->pretip_state.away_tip_countdown,
        (unsigned)scene->pretip_state.away_jump_commit_frame,
        scene->pretip_state.away_jump_committed ? 1U : 0U,
        (unsigned)scene->pretip_jumper_altitude_q8[0U],
        (unsigned)scene->pretip_state.away_actor_state,
        (unsigned)away_phase, (unsigned)away->pose_index,
        (unsigned)scene->pretip_state.away_height_fraction,
        (int)scene->pretip_state.away_jump_velocity_signed_q8,
        (unsigned)scene->pretip_state.away_apex_frame,
        (unsigned)scene->pretip_state.away_jump_commit_count,
        scene->pretip_state.home_tip_sampled ? 1U : 0U,
        (unsigned)scene->pretip_state.home_tip_capture_clock,
        (unsigned)scene->pretip_state.home_tip_countdown,
        (unsigned)scene->pretip_state.home_jump_commit_frame,
        scene->pretip_state.home_jump_committed ? 1U : 0U,
        (unsigned)scene->pretip_jumper_altitude_q8[1U],
        (unsigned)scene->pretip_state.home_actor_state,
        (unsigned)home_phase, (unsigned)home->pose_index,
        (unsigned)scene->pretip_state.home_height_fraction,
        (int)scene->pretip_state.home_jump_velocity_signed_q8,
        (unsigned)scene->pretip_state.home_apex_frame,
        (unsigned)scene->pretip_state.home_jump_commit_count,
        (unsigned)scene->pretip_state.ball_actor_state,
        scene->pretip_state.contact_state_17 ? 1U : 0U,
        scene->pretip_state.event_0588_bit20 ? 1U : 0U,
        (unsigned)scene->pretip_state.first_cinematic_frame);
    {
        int32_t dx = (int32_t)scene->pretip_state.receiver_target.x * 256 -
                     scene->pretip_state.ball_world_x_q8;
        int32_t dy = (int32_t)scene->pretip_state.receiver_target.y * 256 -
                     scene->pretip_state.ball_world_depth_q8;
        uint32_t distance_q8 = (uint32_t)(dx < 0 ? -dx : dx) +
                               (uint32_t)(dy < 0 ? -dy : dy);
        printf(
            "tipball-trajectory logical-frame=%u total-frame=%u simulation-tick=%u "
            "presentation-phase=%s cinematic-visible=%u claimant=%u "
            "selector-037f=%u selector-0380=%u receiver=%u "
            "target-x=%d target-depth=%d ball-state=%u ball-x-q8=%ld "
            "ball-depth-q8=%ld ball-height-q8=%u velocity-x-q8=%ld "
            "velocity-depth-q8=%ld velocity-height-q8=%d distance-q8=%lu "
            "position-x-q6=%u position-depth-q6=%u "
            "velocity-x-prehalf-q6=%d velocity-depth-prehalf-q6=%d "
            "velocity-x-q6=%d velocity-depth-q6=%d duration=%u remaining=%u "
            "workspace-6768=%u event-bit20=%u claim-frame=%u first-cinematic-frame=%u "
            "holder=%u possession=%u attached=%u in-flight=%u flight-tick=%u\n",
            scene->frame, scene->pretip_state.total_frame,
            (unsigned)scene->pretip_state.simulation_tick,
            tecmo_gameplay_pretip_phase_name(scene->pretip_state.phase),
            scene->pretip_state.cinematic_visible ? 1U : 0U,
            (unsigned)scene->pretip_state.claimant_jumper,
            (unsigned)scene->pretip_state.raw_selector_037f,
            (unsigned)scene->pretip_state.raw_selector_0380,
            (unsigned)scene->pretip_state.receiver_actor,
            (int)scene->pretip_state.receiver_target.x,
            (int)scene->pretip_state.receiver_target.y,
            (unsigned)scene->pretip_state.ball_actor_state,
            (long)scene->pretip_state.ball_world_x_q8,
            (long)scene->pretip_state.ball_world_depth_q8,
            (unsigned)scene->pretip_state.ball_height_q8,
            (long)scene->pretip_state.ball_velocity_x_q8,
            (long)scene->pretip_state.ball_velocity_depth_q8,
            (int)scene->pretip_state.ball_velocity_height_q8,
            (unsigned long)distance_q8,
            (unsigned)scene->pretip_state.ball_world_x_q6,
            (unsigned)scene->pretip_state.ball_world_depth_q6,
            (int)scene->pretip_state.ball_velocity_x_prehalf_q6,
            (int)scene->pretip_state.ball_velocity_depth_prehalf_q6,
            (int)scene->pretip_state.ball_velocity_x_q6,
            (int)scene->pretip_state.ball_velocity_depth_q6,
            (unsigned)scene->pretip_state.ball_flight_duration,
            (unsigned)scene->pretip_state.ball_duration_count,
            (unsigned)scene->pretip_state.ball_workspace_6768,
            scene->pretip_state.event_0588_bit20 ? 1U : 0U,
            (unsigned)scene->pretip_state.claim_frame,
            (unsigned)scene->pretip_state.first_cinematic_frame,
            (unsigned)scene->ball_holder,
            (unsigned)scene->state.possession,
            scene->pretip_state.ball_attached_to_receiver ? 1U : 0U,
            scene->pretip_state.ball_state17_in_flight ? 1U : 0U,
            (unsigned)scene->pretip_state.ball_flight_tick);
    }
    printf("tipoff-continuity logical-frame=%u phase=%s holder=%u "
           "possession=%u camera-x=%u follow-serial=%u claimant=%u "
           "receiver=%u selected-0308=%u selected-0309=%u ball-state=%u "
           "ball-x-q8=%ld ball-y-q8=%ld ball-attached=%u "
           "away-state=%u away-altitude-q8=%u away-commits=%u "
           "home-state=%u home-altitude-q8=%u home-commits=%u "
           "foundation-sync-serial=%lu",
           scene->frame,
           tecmo_gameplay_pretip_phase_name(scene->pretip_state.phase),
           (unsigned)scene->ball_holder, (unsigned)scene->state.possession,
           (unsigned)court_frame.projection.camera_x,
           (unsigned)scene->camera_follow_count,
           (unsigned)scene->pretip_state.claimant_jumper,
           (unsigned)scene->pretip_state.receiver_actor,
           (unsigned)scene->pretip_jumper_actor[0U],
           (unsigned)scene->pretip_jumper_actor[1U],
           (unsigned)scene->pretip_state.ball_actor_state,
           (long)scene->ball_position.x_q8,
           (long)scene->ball_position.y_q8,
           scene->pretip_state.ball_attached_to_receiver ? 1U : 0U,
           (unsigned)scene->pretip_state.away_actor_state,
           (unsigned)scene->pretip_jumper_altitude_q8[0U],
           (unsigned)scene->pretip_state.away_jump_commit_count,
           (unsigned)scene->pretip_state.home_actor_state,
           (unsigned)scene->pretip_jumper_altitude_q8[1U],
           (unsigned)scene->pretip_state.home_jump_commit_count,
           (unsigned long)scene->live_foundation.sync_serial);
    for (size_t actor = 0U;
         actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        const TecmoGameplaySceneCpuActor *cpu = &scene->cpu_actors[actor];
        printf(" actor%u=%d,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%d,%d,%d,%d",
               (unsigned)actor, (int)item->position.x,
               (int)item->position.y, (int)item->anchor.x,
               (int)item->anchor.y, (unsigned)item->pose_index,
               item->facing_right ? 1U : 0U,
               item->pose_orientation_encoded ? 1U : 0U,
               (unsigned)item->movement_action_state,
               (unsigned)item->movement_direction,
               (unsigned)item->movement_fractional_accumulator,
               (unsigned)item->movement_animation_phase,
               (unsigned long)cpu->decision_serial,
               (unsigned)cpu->command_offset,
               (unsigned)cpu->linked_actor,
               cpu->target_valid ? 1U : 0U,
               (int)cpu->target_position.x, (int)cpu->target_position.y,
               (int)scene->live_foundation.actor_position[actor].x,
               (int)scene->live_foundation.actor_position[actor].y);
    }
    printf("\n");
    return true;
}

static bool run_gameplay_checkpoint_preflight(TecmoRuntime *runtime, const TecmoCliGameplayCheckpointConfig *config, bool *done_out)
{
    TecmoGameplaySceneLaunch launch;
    TecmoInput input;
    TecmoWin32KeyboardState keyboard;
    TecmoControls controls[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoCliTipoffInputEvidence input_evidence;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const uint8_t away_team = config->away_team;
    const uint8_t home_team = config->home_team;
    const bool pretip_checkpoint = config->pretip_checkpoint;
    const bool live_start = config->live_start;
    const bool facing_checkpoint = config->facing_checkpoint;
    const bool tipoff_proof = config->tipoff_proof;
    const bool tipoff_continuity = config->tipoff_continuity;
    const bool tipoff_away_win = config->tipoff_away_win;
    const bool ball_bounce = config->ball_bounce;
    const bool cpu_steering = config->cpu_steering;
    const bool pass_handoff_proof = config->pass_handoff_proof;
    const bool directional_selection_proof =
        config->directional_selection_proof;
    const int possession_slice = config->possession_slice;
    const int free_throw_orientation = config->free_throw_orientation;
    const unsigned first_contest_update = TECMO_CLI_PRETIP_CAPTURE_FRAME;
    const bool away_live_adapter = !cpu_steering && !pretip_checkpoint;

    *done_out = false;
    memset(&keyboard, 0, sizeof(keyboard));
    memset(controls, 0, sizeof(controls));
    memset(&input_evidence, 0, sizeof(input_evidence));
    input_evidence.fast_x_pulse_frame = 0xFFFFFFFFU;
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = away_team;
    launch.home_team = home_team;
    launch.regulation_minutes = 3U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0] = cpu_steering || tipoff_away_win
        ? TECMO_GAMEPLAY_TEAM_HOME : TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(&runtime->gameplay_scene, &launch)) {
        return false;
    }
    tecmo_runtime_set_mode(runtime, TECMO_MODE_COURT);
    if (away_live_adapter) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        const unsigned adapter_updates = tipoff_proof || live_start
            ? checkpoint : TECMO_CLI_PRETIP_LIVE_START_FRAME;
        tecmo_win32_keyboard_init(&keyboard);
        tecmo_controls_init(&controls[0U]);
        tecmo_controls_init(&controls[1U]);
        {
            TecmoWin32KeyBinding binding;
            if (tecmo_win32_translate_key((uint32_t)'B', &binding)) {
                return false;
            }
            input_evidence.literal_b_mapped = false;
        }
        for (update = 0U; update < adapter_updates; ++update) {
            if (!input_evidence.bridge_used &&
                scene->frame == TECMO_CLI_PRETIP_CAPTURE_FRAME - 1U &&
                scene->pretip_state.phase ==
                    TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP) {
                if (!gameplay_checkpoint_run_fast_x_bridge(
                    runtime, &keyboard, controls, &input_evidence)) {
                    return false;
                }
            } else if (!gameplay_checkpoint_run_adapter_update(
                           runtime, &keyboard, controls, &input_evidence)) {
                return false;
            }
        }
        if (!tipoff_proof && !live_start && !pass_handoff_proof &&
            !directional_selection_proof &&
            (!scene_handoff_possession(
                 scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
             !scene_sync_live_foundation(scene))) {
            return false;
        }
        if (directional_selection_proof) {
            const TecmoControlFrame *selection_controls[2];
            TecmoGameplayCandidateInput selection_input;
            TecmoGameplayCandidateResult selection_result;
            uint8_t side;
            uint8_t sector;
            uint8_t reference;
            uint8_t excluded;
            uint8_t polarity;
            uint8_t chosen;
            uint8_t pass_target = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            size_t actor;
            memset(controls, 0, sizeof(controls));
            /* This native proof fixture exercises the canonical bound path;
               the generic render launcher otherwise marks its identity
               lineup as legacy/test-only. */
            scene->legacy_direct_launch = false;
            scene->launch.starter_binding_bound = true;
            for (actor = 0U; actor < 5U; ++actor) {
                scene->launch.starter_roster_index[0U][actor] = (uint8_t)actor;
                scene->launch.starter_roster_index[1U][actor] = (uint8_t)actor;
            }
            selection_controls[0U] = &controls[0U].frame;
            selection_controls[1U] = &controls[1U].frame;
            if (checkpoint <= 2U) {
                if (!scene_handoff_possession(
                        scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U)) return false;
                scene->launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
                scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
                scene->controlled_actor[0U] = 0U;
                scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                if (!scene_sync_live_foundation(scene)) return false;
                scene->actors[0U].position.x = 320; scene->actors[0U].position.y = 140;
                scene->actors[1U].position.x = 360; scene->actors[1U].position.y = 100;
                scene->actors[2U].position.x = 370; scene->actors[2U].position.y = 180;
                scene->actors[3U].position.x = 250; scene->actors[3U].position.y = 100;
                scene->actors[4U].position.x = 260; scene->actors[4U].position.y = 180;
                controls[0U].frame.held.right = checkpoint != 1U;
                controls[0U].frame.held.left = checkpoint == 1U;
                side = scene->live_foundation.offense_side;
                sector = checkpoint == 1U ? 2U : 1U;
                reference = 0U; excluded = 0U; polarity = 0U;
            } else {
                if (!scene_handoff_possession(
                        scene, TECMO_GAMEPLAY_TEAM_HOME, 5U)) return false;
                scene->launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
                scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
                scene->controlled_actor[0U] = 0U;
                scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
                if (!scene_sync_live_foundation(scene)) return false;
                scene->live_foundation.defender_actor = 0U;
                scene->live_foundation.play_state.defender_actor = 0U;
                scene->live_foundation.selected_actor_by_side[0U] = 0U;
                scene->live_foundation.candidate_actor_by_side[0U] = 1U;
                scene->actors[0U].position.x = (int16_t)(
                    scene->camera_state.camera_x + 100U);
                scene->actors[0U].position.y = 140;
                scene->actors[1U].position.x = (int16_t)(
                    scene->camera_state.camera_x + 150U);
                scene->actors[1U].position.y = 100;
                scene->actors[2U].position.x = (int16_t)(
                    scene->camera_state.camera_x + 160U);
                scene->actors[2U].position.y = 180;
                scene->actors[3U].position.x = (int16_t)(
                    scene->camera_state.camera_x + 50U);
                scene->actors[3U].position.y = 100;
                scene->actors[4U].position.x = (int16_t)(
                    scene->camera_state.camera_x + 60U);
                scene->actors[4U].position.y = 180;
                controls[0U].frame.held.right = checkpoint == 3U;
                controls[0U].frame.held.left = checkpoint != 3U;
                side = scene->live_foundation.defense_side;
                sector = checkpoint == 3U ? 1U : 2U;
                reference = 0U; excluded = 0U; polarity = 0x10U;
            }
            for (actor = 0U; actor < 10U; ++actor) {
                scene->live_foundation.actor_position[actor] =
                    scene->actors[actor].position;
            }
            if (!scene_ball_position_for_actors(
                    scene, scene->actors, scene->ball_holder,
                    &scene->ball_position) ||
                !scene_update_selection_candidates(scene, selection_controls))
                return false;
            chosen = scene->live_foundation.candidate_actor_by_side[side];
            if (checkpoint == 2U) {
                pass_target = chosen;
                if (!scene_pass_or_switch(scene, 0U) ||
                    !scene_sync_live_foundation(scene)) return false;
            } else if (checkpoint == 5U) {
                if (!scene_pass_or_switch(scene, 0U) ||
                    !scene_sync_live_foundation(scene)) return false;
            }
            memset(&selection_input, 0, sizeof(selection_input));
            selection_input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
            selection_input.direction_sector = sector;
            selection_input.excluded_actor = excluded;
            selection_input.required_polarity = polarity;
            selection_input.reference_actor = reference;
            selection_input.viewport_x = scene->camera_state.camera_x;
            for (actor = 0U; actor < 10U; ++actor) {
                selection_input.actor_x[actor] =
                    (uint16_t)scene->actors[actor].position.x;
                selection_input.actor_depth[actor] =
                    (uint8_t)scene->actors[actor].position.y;
                selection_input.actor_flags[actor] =
                    scene->live_foundation.actor_selector_flags[actor];
            }
            if (!tecmo_gameplay_candidate_directional_select(
                    &selection_input, &selection_result)) return false;
            printf("directional-selection-proof {\"logical_frame\":%u,"
                   "\"raw_0308\":%u,\"raw_0309\":%u,"
                   "\"raw_030a\":%u,\"raw_030b\":%u,"
                   "\"raw_000e\":%u,\"raw_000f\":%u,"
                   "\"raw_037f\":%u,\"raw_0380\":%u,"
                   "\"control_modes\":[%u,%u],\"direction_nibble\":%u,"
                   "\"cpu_actor_direction\":%u,\"mapped_sector\":%u,"
                   "\"chosen_candidate\":%u,\"pass_target\":%u,"
                   "\"final_holder\":%u,\"final_defender\":%u,"
                   "\"ai_stream_0\":%u,\"actors\":[",
                   checkpoint, scene->live_foundation.primary_actor,
                   scene->live_foundation.defender_actor,
                   scene->live_foundation.offense_side,
                   scene->live_foundation.defense_side,
                   scene->live_foundation.selected_actor_by_side[0U],
                   scene->live_foundation.selected_actor_by_side[1U],
                   scene->live_foundation.candidate_actor_by_side[0U],
                   scene->live_foundation.candidate_actor_by_side[1U],
                   scene->live_foundation.control_mode[0U],
                   scene->live_foundation.control_mode[1U], sector,
                   scene->actors[reference].movement_direction, sector,
                   chosen, pass_target, scene->ball_holder,
                   scene->live_foundation.defender_actor,
                   scene->live_foundation.play_state.stream_offset[0U]);
            for (actor = 0U; actor < 10U; ++actor) {
                printf("%s{\"slot\":%u,\"x\":%d,\"depth\":%d,"
                       "\"polarity\":%u,\"eligible\":%u,"
                       "\"filter\":%u,\"score\":%u}",
                       actor == 0U ? "" : ",", (unsigned)actor,
                       scene->actors[actor].position.x,
                       scene->actors[actor].position.y,
                       (scene->live_foundation.actor_selector_flags[actor] &
                        0x10U) != 0U ? 1U : 0U,
                       scene->live_foundation.defender_eligible[actor] ? 1U : 0U,
                       selection_result.filter[actor],
                       selection_result.score[actor]);
            }
            printf("]}\n");
            *done_out = true;
            return true;
        }
        if (tipoff_continuity && scene->pretip_state.live_handoff &&
            !scene_sync_live_foundation(scene)) {
            return false;
        }
        if (pass_handoff_proof) {
            TecmoGameplaySceneCpuShotRequest shot_request;
            if (!scene_handoff_possession(
                    scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
                !scene_sync_live_foundation(scene)) {
                return false;
            }
            scene->launch.controller_team[0U] = TECMO_GAMEPLAY_TEAM_AWAY;
            scene->launch.controller_team[1U] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
            scene->controlled_actor[0U] = 0U;
            scene->controlled_actor[1U] = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
            if (!scene_sync_live_foundation(scene)) return false;
            scene->live_foundation.defender_actor = 5U;
            scene->live_foundation.play_state.defender_actor = 5U;
            scene->live_foundation.control_mode[TECMO_GAMEPLAY_TEAM_HOME] = 1U;
            scene->live_foundation.dynamic_link[9U] = 1U;
            scene->live_foundation.defender_eligible[9U] = false;
            scene->live_foundation.dynamic_link[8U] = 0U;
            scene->live_foundation.dynamic_link[7U] = 0U;
            scene->live_foundation.dynamic_link[6U] = 1U;
            scene->live_foundation.defender_eligible[6U] = true;
            scene->actors[5U].position.x =
                (int16_t)(scene->actors[0U].position.x - 18);
            scene->actors[5U].position.y = scene->actors[0U].position.y;
            scene->actors[6U].position.x =
                (int16_t)(scene->actors[1U].position.x - 34);
            scene->actors[6U].position.y = scene->actors[1U].position.y;
            if (checkpoint >= 1U) {
                if (!tecmo_gameplay_live_foundation_pass_handoff(
                        &scene->cpu_steering_assets, 1U,
                        &scene->live_foundation) ||
                    !scene_ball_position_for_actors(
                        scene, scene->actors, 1U, &scene->ball_position)) {
                    return false;
                }
                scene->ball_holder = 1U;
                scene->controlled_actor[0U] = 1U;
            }
            if (checkpoint >= 2U &&
                !scene_update_ai(scene, &shot_request)) {
                return false;
            }
            printf("pass-handoff-proof stage=%u selected_offense=%u "
                   "prior_offense=%u old_holder_state=%u "
                   "old_holder_cursor=%04X selected_defense=%u "
                   "prior_defense=%u linked=%u eligible=%u\n",
                   checkpoint, scene->live_foundation.primary_actor,
                   scene->live_foundation.prior_selected_actor,
                   scene->live_foundation.play_state.actor_state[0U],
                   scene->live_foundation.play_state.stream_offset[0U],
                   scene->live_foundation.defender_actor,
                   scene->live_foundation.prior_defender_actor,
                   scene->live_foundation.dynamic_link[6U],
                   scene->live_foundation.defender_eligible[6U] ? 1U : 0U);
            *done_out = true;
            return true;
        }
        if (tipoff_proof) {
            *done_out = true;
            if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
                scene->frame != checkpoint ||
                (checkpoint < TECMO_CLI_PRETIP_CAPTURE_FRAME &&
                 (input_evidence.bridge_used ||
                  scene->pretip_state.away_tip_sampled ||
                  scene->pretip_state.home_tip_sampled)) ||
                (checkpoint >= first_contest_update &&
                 (!input_evidence.bridge_used ||
                  input_evidence.bridge_begin_count != checkpoint ||
                  input_evidence.bridge_end_count != checkpoint ||
                  input_evidence.bridge_update_players_count != checkpoint ||
                  !input_evidence.raw_x_down ||
                  !input_evidence.raw_x_up ||
                  !input_evidence.raw_x_down_logical ||
                  input_evidence.raw_x_up_logical ||
                  !input_evidence.fast_x_effective_cancel ||
                  !input_evidence.fast_x_effective_pressed ||
                  input_evidence.fast_x_post_bridge_cancel ||
                  input_evidence.fast_x_pulse_frame !=
                      first_contest_update ||
                  input_evidence.literal_b_mapped ||
                  (tipoff_away_win
                    ? (!scene->pretip_state.home_tip_sampled ||
                       scene->pretip_state.home_tip_sample_frame != 0U ||
                       scene->pretip_state.home_tip_capture_clock != 0xE1U ||
                       scene->pretip_state.home_tip_error !=
                            11U)
                    : (!scene->pretip_state.away_tip_sampled ||
                       scene->pretip_state.away_tip_sample_frame != 0U ||
                       scene->pretip_state.away_tip_capture_clock != 0xE1U ||
                       scene->pretip_state.away_tip_error !=
                             11U)))) ||
                (checkpoint < first_contest_update &&
                 (input_evidence.bridge_used ||
                  input_evidence.raw_x_down ||
                  input_evidence.raw_x_up ||
                  input_evidence.bridge_begin_count != checkpoint ||
                  input_evidence.bridge_end_count != checkpoint ||
                  input_evidence.bridge_update_players_count != checkpoint ||
                  input_evidence.fast_x_pulse_frame != 0xFFFFFFFFU)) ||
                (!tecmo_gameplay_scene_in_pretip(scene) &&
                 ((scene->pretip_state.claimant_jumper == 0U
                    ? scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY
                    : scene->state.possession != TECMO_GAMEPLAY_TEAM_HOME) ||
                  scene->ball_holder !=
                      scene->pretip_state.receiver_actor))) {
                return false;
            }
            return gameplay_checkpoint_report_tipoff_proof(
                scene, &input_evidence);
        }
        if (live_start) {
            *done_out = true;
            if (facing_checkpoint &&
                (!scene_handoff_possession(
                     scene, TECMO_GAMEPLAY_TEAM_AWAY, 3U) ||
                 !scene_sync_live_foundation(scene))) {
                return false;
            }
            return runtime->mode == TECMO_MODE_COURT && scene->active &&
                   !tecmo_gameplay_scene_in_pretip(scene) &&
                   (facing_checkpoint
                      ? (scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY &&
                         scene->ball_holder == 3U)
                      : scene->pretip_state.claimant_jumper == 0U
                      ? (scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY &&
                         scene->ball_holder ==
                             scene->pretip_state.receiver_actor)
                      : (scene->state.possession == TECMO_GAMEPLAY_TEAM_HOME &&
                         scene->ball_holder ==
                             scene->pretip_state.receiver_actor));
        }
        if (ball_bounce) {
            for (update = 0U; update < checkpoint; ++update) {
                memset(&input, 0, sizeof(input));
                tecmo_runtime_update(runtime, &input);
            }
            *done_out = true;
            return runtime->mode == TECMO_MODE_COURT && scene->active &&
                   scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
                   scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
                   scene->frame == TECMO_CLI_PRETIP_LIVE_START_FRAME +
                       checkpoint &&
                   scene->ball_holder == 0U;
        }
        if (possession_slice >= 0 || free_throw_orientation >= 0) {
            return true;
        }
        return true;
    }
    if (pretip_checkpoint) {
        memset(&input, 0, sizeof(input));
        for (update = 0U; update < checkpoint; ++update)
            tecmo_runtime_update(runtime, &input);
        *done_out = true; return runtime->mode == TECMO_MODE_COURT &&
               runtime->gameplay_scene.active &&
               tecmo_gameplay_scene_in_pretip(
                   &runtime->gameplay_scene);
    }
    memset(&input, 0, sizeof(input));
    if (cpu_steering) {
        runtime->gameplay_scene.launch.controller_team[0U] =
            TECMO_GAMEPLAY_TEAM_AWAY;
    }
    for (update = 0U; update < TECMO_CLI_PRETIP_LIVE_START_FRAME; ++update) {
        input.cancel = runtime->gameplay_scene.frame ==
                TECMO_CLI_PRETIP_CAPTURE_FRAME - 1U &&
            runtime->gameplay_scene.pretip_state.phase ==
                TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP;
        tecmo_runtime_update(runtime, &input);
        if (cpu_steering &&
            runtime->gameplay_scene.pretip_state.claim_resolved) {
            runtime->gameplay_scene.launch.controller_team[0U] =
                TECMO_GAMEPLAY_TEAM_HOME;
        }
    }
    if (cpu_steering) {
        runtime->gameplay_scene.launch.controller_team[0U] =
            TECMO_GAMEPLAY_TEAM_HOME;
    }
    if (live_start) {
        *done_out = true;
        return runtime->mode == TECMO_MODE_COURT &&
               runtime->gameplay_scene.active &&
               !tecmo_gameplay_scene_in_pretip(
                   &runtime->gameplay_scene);
    }
    if (ball_bounce) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        *done_out = true; return runtime->mode == TECMO_MODE_COURT && scene->active &&
               scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
               scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               scene->frame == TECMO_CLI_PRETIP_LIVE_START_FRAME + checkpoint &&
               scene->ball_holder == 0U;
    }
    if (cpu_steering) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        uint32_t initial_decision_serial;
        if (!scene_handoff_possession(
                scene, TECMO_GAMEPLAY_TEAM_AWAY, 0U) ||
            !scene_sync_live_foundation(scene)) {
            return false;
        }
        initial_decision_serial = scene->cpu_actors[0].decision_serial;
        memset(&input, 0, sizeof(input));
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        *done_out = true; return runtime->mode == TECMO_MODE_COURT && scene->active &&
               scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               scene->frame == TECMO_CLI_PRETIP_LIVE_START_FRAME + checkpoint &&
               scene->ball_holder == 0U &&
               scene->actors[0].position.x < 0x0210 &&
               scene->cpu_actors[0].decision_serial ==
                   initial_decision_serial + checkpoint &&
               scene->cpu_actors[0].target_valid &&
               scene->cpu_actors[0].target_kind ==
                   TECMO_GAMEPLAY_CPU_STEERING_HARNESS_HOOP_APPROACH &&
               scene->cpu_actors[0].target_position.x == 208 &&
               scene->cpu_actors[0].target_position.y == 148;
    }
    return true;
}

static bool run_gameplay_facing_checkpoint(
    const TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config)
{
    const TecmoGameplayScene *scene;
    TecmoGameplaySceneCourtFrame court_frame;
    size_t actor;
    size_t visible_away = 0U;
    (void)config;
    if (runtime == NULL || !runtime->gameplay_scene.active) return false;
    scene = &runtime->gameplay_scene;
    if (scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
        scene->orientation_state.current_direction != 0U ||
        scene->orientation_state.tracked_possession_team !=
            TECMO_GAMEPLAY_TEAM_AWAY ||
        scene->orientation_state.offensive_hoop.x !=
            TECMO_GAMEPLAY_COURT_LEFT_HOOP_X ||
        scene->orientation_state.offensive_hoop.y !=
            TECMO_GAMEPLAY_COURT_HOOP_Y ||
        !tecmo_gameplay_scene_court_frame(scene, &court_frame)) {
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
        const TecmoGameplaySceneActor *item = &scene->actors[actor];
        bool expected_facing;
        if (!item->active ||
            !gameplay_checkpoint_goal_facing_right(
                scene, item->team, &expected_facing) ||
            item->facing_right != expected_facing) {
            return false;
        }
        if (item->team == TECMO_GAMEPLAY_TEAM_AWAY &&
            court_frame.projection.players[actor].visible) {
            ++visible_away;
        }
    }
    return visible_away > 0U;
}

static bool run_gameplay_violation_checkpoint(
    TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config,
    bool *handled_out)
{
    TecmoInput input;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const bool shot_clock_violation = config->shot_clock_violation;
    const bool out_of_bounds_violation = config->out_of_bounds_violation;
    const bool backcourt_violation = config->backcourt_violation;

    *handled_out = false;
    memset(&input, 0, sizeof(input));
    if (shot_clock_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        *handled_out = true;
        scene->state.shot_clock = 1U;
        scene->state.clock_divider = 1U;
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation !=
                TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK ||
            scene->state.phase_frame != 0U) {
            return false;
        }
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK &&
               scene->state.phase_frame == checkpoint;
    }
    if (out_of_bounds_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *holder = &scene->actors[0U];

        *handled_out = true;
        /* Drive the production TGMO -> TPNL -> TGVR path instead of injecting
           a violation phase. At Y=148, X=149 is the exact page-0 clamp. The
           first held-left update takes TGMO's action-state latency; the second
           attempts X=148, clamps back to 149, and raises selector $0742=1. */
        holder->position.x = 149;
        holder->position.y = 148;
        holder->anchor = holder->position;
        holder->movement_action_state =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
        holder->movement_fractional_accumulator = 15U;
        holder->movement_boundary_latched = false;
        scene->ball_position.x_q8 = (int32_t)(holder->position.x + 7) * 256;
        scene->ball_position.y_q8 = (int32_t)(holder->position.y - 17) * 256;
        input.left = true;
        tecmo_runtime_update(runtime, &input);
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation !=
                TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS ||
            scene->state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
            scene->state.phase_frame != 0U ||
            holder->position.x != 149 || holder->movement_boundary_latched) {
            return false;
        }
        memset(&input, 0, sizeof(input));
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS &&
               scene->state.restart_possession == TECMO_GAMEPLAY_TEAM_HOME &&
               scene->state.phase_frame == checkpoint;
    }
    if (backcourt_violation) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *holder = &scene->actors[0U];

        *handled_out = true;
        /* Drive the production held-ball -> TGBC -> TPNL -> TGVR route.
           Orientation 0 establishes frontcourt with ball X<=375, preserves
           the original eight-pixel neutral band, then calls BACKCOURT at
           ball X>=386. Away attacks left in fresh TGOR, so retain that goal
           facing and account for the resolved -4 TGBD attachment. */
        holder->position.x = 379;
        holder->position.y = 148;
        holder->anchor = holder->position;
        if (!gameplay_checkpoint_goal_facing_right(
                scene, holder->team, &holder->facing_right) ||
            holder->facing_right) {
            return false;
        }
        holder->movement_action_state =
            TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL;
        holder->movement_fractional_accumulator = 0U;
        holder->movement_boundary_latched = false;
        scene->ball_position.x_q8 = 375 * 256;
        scene->ball_position.y_q8 = 131 * 256;
        tecmo_runtime_update(runtime, &input);
        if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
            scene->state.phase != TECMO_GAMEPLAY_PHASE_LIVE ||
            scene->backcourt_state.frontcourt_established != 1U) {
            return false;
        }
        holder->position.x = 390;
        holder->anchor = holder->position;
        scene->ball_position.x_q8 = 386 * 256;
        scene->ball_position.y_q8 = 131 * 256;
        tecmo_runtime_update(runtime, &input);
        if (scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
            scene->state.violation != TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
            scene->state.restart_possession != TECMO_GAMEPLAY_TEAM_HOME ||
            scene->state.phase_frame != 0U) {
            return false;
        }
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        return scene->state.phase ==
                   TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION &&
               scene->state.violation ==
                   TECMO_GAMEPLAY_VIOLATION_BACKCOURT &&
               scene->state.restart_possession == TECMO_GAMEPLAY_TEAM_HOME &&
               scene->state.phase_frame == checkpoint;
    }
    return true;
}

static bool run_gameplay_camera_checkpoint(
    TecmoRuntime *runtime,
    const TecmoCliGameplayCheckpointConfig *config,
    bool *handled_out)
{
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const int possession_slice = config->possession_slice;
    const int free_throw_orientation = config->free_throw_orientation;
    *handled_out = false;
    if (possession_slice >= 0) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplaySceneActor *actor;
        TecmoGameplaySceneCourtFrame court_frame;
        uint8_t actor_index;
        *handled_out = true;
        if (!tecmo_gameplay_camera_state_initialize(
                &scene->camera_assets, &scene->camera_state) ||
            !tecmo_gameplay_camera_state_prime_live(
                &scene->camera_assets, &scene->camera_state)) {
            return false;
        }
        if (possession_slice == 1) {
            return runtime->mode == TECMO_MODE_COURT &&
                   scene->active &&
                   tecmo_gameplay_scene_court_frame(
                       scene, &court_frame) &&
                    court_frame.slice.viewport.camera_x == 0x0100U &&
                    court_frame.projection.camera_x == 0x0100U &&
                   court_frame.slice.possession ==
                       TECMO_GAMEPLAY_TEAM_AWAY &&
                   court_frame.slice.direction == 0U;
        }
        actor_index =
            possession_slice == 0
                ? 0U
                : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        if (possession_slice == 2 &&
            (!tecmo_gameplay_reset_possession(
                 &scene->state, TECMO_GAMEPLAY_TEAM_HOME) ||
             !tecmo_gameplay_court_orientation_synchronize(
                 &scene->court_orientation, &scene->orientation_state,
                 TECMO_GAMEPLAY_TEAM_HOME))) {
            return false;
        }
        actor = &scene->actors[actor_index];
        actor->position.x =
            (int16_t)(possession_slice == 0 ? 0x00F3 : 0x020D);
        actor->anchor = actor->position;
        if (!gameplay_checkpoint_goal_facing_right(
                scene, actor->team, &actor->facing_right)) {
            return false;
        }
        scene->ball_holder = actor_index;
        scene->ball_position.x_q8 =
            (int32_t)(actor->position.x +
                      (actor->facing_right ? 7 : -7)) * 256;
        scene->ball_position.y_q8 =
            (int32_t)(actor->position.y - 17) * 256;
        scene->camera_state.thresholds_valid = false;
        scene->camera_state.endpoint_latched = false;
        if (!tecmo_gameplay_camera_settle_court(
                &scene->camera_assets, &scene->camera_state,
                &scene->ball_position,
                scene->orientation_state.current_direction, false) ||
            !tecmo_gameplay_camera_state_live_valid(
                &scene->camera_assets, &scene->camera_state)) {
            return false;
        }
        return tecmo_gameplay_scene_court_frame(
                   scene, &court_frame) &&
               court_frame.slice.viewport.camera_x ==
                   (uint16_t)(possession_slice == 0
                                  ? 0x0066U
                                  : 0x0198U) &&
               court_frame.projection.camera_x ==
                   court_frame.slice.viewport.camera_x &&
               court_frame.slice.possession ==
                   (uint8_t)(possession_slice == 0
                                 ? TECMO_GAMEPLAY_TEAM_AWAY
                                 : TECMO_GAMEPLAY_TEAM_HOME) &&
               court_frame.slice.direction ==
                   (uint8_t)(possession_slice == 0 ? 0U : 1U);
    }
    if (free_throw_orientation >= 0) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        TecmoGameplayFoulRequest request;
        TecmoGameplayFreeThrowLineup lineup;
        TecmoGameplaySceneCourtFrame court_frame;
        TecmoControlFrame player_one;
        TecmoControlFrame player_two;
        uint8_t shooter =
            free_throw_orientation == 0 ? 0U
                                        : TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT;
        uint8_t secondary =
            free_throw_orientation == 0
                ? TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT
                : 0U;
        uint16_t camera_x =
            (uint16_t)(free_throw_orientation == 0 ? 0x0066U : 0x0198U);
        size_t actor;
        *handled_out = true;
        memset(&request, 0, sizeof(request));
        request.fouling_team =
            free_throw_orientation == 0 ? TECMO_GAMEPLAY_TEAM_HOME
                                        : TECMO_GAMEPLAY_TEAM_AWAY;
        request.free_throw_team =
            free_throw_orientation == 0 ? TECMO_GAMEPLAY_TEAM_AWAY
                                        : TECMO_GAMEPLAY_TEAM_HOME;
        request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_BOTH;
        request.free_throw_attempts = 2U;
        if (!tecmo_gameplay_request_foul(&scene->state, &request)) {
            return false;
        }
        memset(&player_one, 0, sizeof(player_one));
        memset(&player_two, 0, sizeof(player_two));
        for (update = 0U;
             update < TECMO_GAMEPLAY_PRESENTATION_LEAD_IN_FRAMES;
             ++update) {
            if (!tecmo_gameplay_scene_update(
                    scene, &player_one, &player_two) ||
                scene->state.phase !=
                    TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION) {
                return false;
            }
        }
        player_one.released.shoot = true;
        if (!tecmo_gameplay_scene_update(
                scene, &player_one, &player_two) ||
            scene->frame != checkpoint ||
            scene->state.phase !=
                TECMO_GAMEPLAY_PHASE_FREE_THROW_SEQUENCE ||
            !tecmo_gameplay_scene_free_throw_lineup(scene, &lineup) ||
            !tecmo_gameplay_scene_court_frame(scene, &court_frame) ||
            lineup.orientation != (uint8_t)free_throw_orientation ||
            lineup.shooter_slot != shooter ||
            lineup.secondary_slot != secondary ||
            lineup.actors[shooter].raw_world_x !=
                (uint16_t)(free_throw_orientation == 0 ? 250U : 518U) ||
            lineup.actors[shooter].raw_world_y != 148U ||
            court_frame.slice.direction !=
                (uint8_t)free_throw_orientation ||
            court_frame.slice.viewport.camera_x != camera_x ||
            court_frame.projection.camera_x != camera_x) {
            return false;
        }
        for (actor = 0U;
             actor < TECMO_GAMEPLAY_SCENE_ACTOR_COUNT; ++actor) {
            if (!lineup.actors[actor].position_defined ||
                scene->actors[actor].position.x !=
                    (int16_t)lineup.actors[actor].raw_world_x ||
                scene->actors[actor].position.y !=
                    (int16_t)lineup.actors[actor].raw_world_y) {
                return false;
            }
        }
        return runtime->mode == TECMO_MODE_COURT && scene->active;
    }
    return true;
}

static bool run_gameplay_shot_checkpoint(TecmoRuntime *runtime, const TecmoCliGameplayCheckpointConfig *config)
{
    TecmoInput input;
    TecmoGameplaySceneShotKind expected_shot_kind;
    unsigned update;
    const unsigned checkpoint = config->checkpoint;
    const bool jump = config->jump;
    const bool jump_make = config->jump_make;
    const bool jump_rattle = config->jump_rattle;
    const bool dunk = config->dunk;
    const bool layup = config->layup;
    bool layup_variant2_seen = false;
    runtime->gameplay_scene.frame = 0U;

    if (dunk) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];

        actor->position.x = 0x00AEU;
        actor->position.y = 160;
        actor->anchor.x = actor->position.x;
        actor->anchor.y = actor->position.y;
        /* Deliberate shot checkpoint setup; launch immediately replaces this
           with the validated offensive-hoop facing override. */
        actor->facing_right = true;
        runtime->gameplay_scene.ball_holder = 0U;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        runtime->gameplay_scene.ball_position.y_q8 =
            (int32_t)(actor->position.y - 18) * 256;
        runtime->gameplay_scene.camera_state.thresholds_valid = false;
        runtime->gameplay_scene.camera_state.endpoint_latched = false;
        if (!tecmo_gameplay_camera_settle_court(
                &runtime->gameplay_scene.camera_assets,
                &runtime->gameplay_scene.camera_state,
                &runtime->gameplay_scene.ball_position,
                runtime->gameplay_scene.orientation_state.current_direction,
                false)) {
            return false;
        }
    } else if (layup) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];

        /* This is a coherent live-court input fixture.  The production shot
           selector observes the resulting TGOR geometry and stable sample to
           choose numeric TGCS variant 2; this fixture does not author any
           shot kind, pose, schedule, outcome, score, claimant, or settlement
           state. */
        actor->position.x = 0x00C0U;
        actor->position.y = 0x008FU;
        actor->anchor.x = actor->position.x;
        actor->anchor.y = actor->position.y;
        runtime->gameplay_scene.ball_holder = 0U;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        runtime->gameplay_scene.ball_position.y_q8 =
            (int32_t)(actor->position.y - 18) * 256;
        runtime->gameplay_scene.camera_state.thresholds_valid = false;
        runtime->gameplay_scene.camera_state.endpoint_latched = false;
        if (!tecmo_gameplay_camera_settle_court(
                &runtime->gameplay_scene.camera_assets,
                &runtime->gameplay_scene.camera_state,
                &runtime->gameplay_scene.ball_position,
                runtime->gameplay_scene.orientation_state.current_direction,
                false)) {
            return false;
        }
    } else if (jump) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];

        /* Every jump checkpoint starts from the same complete legal court
           tuple.  The old generic/rattle route changed only X and inherited
           Y/anchor/ball depth from its preflight, which made the frame-72
           rim-rattle render contract depend on unrelated tip-off state. */
        actor->position.x = 0x013CU;
        actor->position.y = 180;
        actor->anchor.x = actor->position.x;
        actor->anchor.y = actor->position.y;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        runtime->gameplay_scene.ball_position.y_q8 =
            (int32_t)(actor->position.y - 18) * 256;
        if (jump_make) {
            runtime->gameplay_scene.action_serial = 0U;
        } else {
            if (!tecmo_gameplay_set_score(
                    &runtime->gameplay_scene.state,
                    TECMO_GAMEPLAY_TEAM_HOME, 2U)) {
                return false;
            }
            runtime->gameplay_scene.action_serial = 1U;
        }
    }
    if (jump_rattle) {
        /* Explicit diagnostic selector setup; the production shot launch
           immediately resolves the actual offensive-hoop facing. */
        runtime->gameplay_scene.actors[0].facing_right = true;
        runtime->gameplay_scene.actors[0].movement_direction = 0U;
        if (!tecmo_gameplay_scene_start_rim_rattle_debug(
                &runtime->gameplay_scene)) {
            return false;
        }
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    if (layup && runtime->gameplay_scene.shot_kind ==
                     TECMO_GAMEPLAY_SCENE_SHOT_LAYUP) {
        if (runtime->gameplay_scene.close_shot_variant !=
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2) {
            return false;
        }
        layup_variant2_seen = true;
    }
    for (update = 1U; update < checkpoint; ++update) {
        memset(&input, 0, sizeof(input));
        if (jump_make && update < 8U) input.cancel = true;
        tecmo_runtime_update(runtime, &input);
        if (layup && runtime->gameplay_scene.shot_kind ==
                         TECMO_GAMEPLAY_SCENE_SHOT_LAYUP) {
            if (runtime->gameplay_scene.close_shot_variant !=
                TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2) {
                return false;
            }
            layup_variant2_seen = true;
        }
    }
    if (jump &&
        ((!jump_make && !jump_rattle && checkpoint == 87U) ||
         (jump_rattle && checkpoint == 103U) ||
         (jump_make && checkpoint == 111U))) {
        expected_shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    } else if (layup) {
        expected_shot_kind = checkpoint == 17U
                                 ? TECMO_GAMEPLAY_SCENE_SHOT_NONE
                                 : TECMO_GAMEPLAY_SCENE_SHOT_LAYUP;
    } else {
        expected_shot_kind = dunk ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                                  : TECMO_GAMEPLAY_SCENE_SHOT_JUMP;
    }
    if (runtime->mode != TECMO_MODE_COURT ||
        !runtime->gameplay_scene.active ||
        (layup &&
         (!layup_variant2_seen ||
          (runtime->gameplay_scene.shot_kind !=
               TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           runtime->gameplay_scene.close_shot_variant !=
               TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2)))) {
        return false;
    }
    return runtime->gameplay_scene.shot_kind == expected_shot_kind;
}

bool tecmo_cli_setup_gameplay_render_checkpoint(TecmoRuntime *runtime, const char *mode_name)
{
    TecmoCliGameplayCheckpointConfig config;
    bool done;
    bool handled;

    if (runtime == NULL || mode_name == NULL ||
        !parse_gameplay_render_checkpoint_mode(mode_name, &config)) {
        return false;
    }
    if (!run_gameplay_checkpoint_preflight(runtime, &config, &done)) {
        return false;
    }
    if (done) {
        return !config.facing_checkpoint ||
               run_gameplay_facing_checkpoint(runtime, &config);
    }
    if (!run_gameplay_violation_checkpoint(runtime, &config, &handled)) {
        return false;
    }
    if (handled) return true;
    if (!run_gameplay_camera_checkpoint(runtime, &config, &handled)) {
        return false;
    }
    if (handled) return true;
    return run_gameplay_shot_checkpoint(runtime, &config);
}

/* Continuous native tip-off proof ---------------------------------------- */

#define TECMO_CLI_TIPOFF_TRACE_WIDTH 640
#define TECMO_CLI_TIPOFF_TRACE_HEIGHT 480
#define TECMO_CLI_TIPOFF_TRACE_PERMANENT_BYTES (16U * 1024U * 1024U)
#define TECMO_CLI_TIPOFF_TRACE_TRANSIENT_BYTES (16U * 1024U * 1024U)
#define TECMO_CLI_TIPOFF_TRACE_MAX_UPDATES 840U
#define TECMO_CLI_TIPOFF_TRACE_CAPTURE_SCENE_FRAME 451U

typedef enum TecmoCliTipoffTraceCaptureSide {
    TECMO_CLI_TIPOFF_TRACE_CAPTURE_NONE = 0,
    TECMO_CLI_TIPOFF_TRACE_CAPTURE_AWAY,
    TECMO_CLI_TIPOFF_TRACE_CAPTURE_HOME
} TecmoCliTipoffTraceCaptureSide;

typedef struct TecmoCliTipoffTraceScenario {
    const char *name;
    uint8_t away_team;
    uint8_t home_team;
    const char *away_city;
    const char *away_nickname;
    const char *home_city;
    const char *home_nickname;
    uint8_t controller_team[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    TecmoCliTipoffTraceCaptureSide capture_side;
    uint8_t capture_delay;
    uint8_t capture_hold_frames;
    bool expect_cinematic;
    bool expect_exact_slow_cinematic;
} TecmoCliTipoffTraceScenario;

typedef struct TecmoCliTipoffTraceJumpTuple {
    TecmoGameplaySceneActor actor[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint8_t state[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint8_t phase[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint16_t altitude_q8[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    int16_t velocity_q8[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    bool jump_active;
} TecmoCliTipoffTraceJumpTuple;

static void tipoff_trace_capture_tuple(
    const TecmoGameplayScene *scene,
    TecmoCliTipoffTraceJumpTuple *tuple)
{
    size_t jumper;
    if (scene == NULL || tuple == NULL) return;
    memset(tuple, 0, sizeof(*tuple));
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        uint8_t actor = scene->pretip_jumper_actor[jumper];
        if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) return;
        tuple->actor[jumper] = scene->actors[actor];
        tuple->state[jumper] = jumper == 0U
            ? scene->pretip_state.away_actor_state
            : scene->pretip_state.home_actor_state;
        tuple->phase[jumper] = jumper == 0U
            ? scene->pretip_state.away_animation_phase
            : scene->pretip_state.home_animation_phase;
        tuple->altitude_q8[jumper] = jumper == 0U
            ? scene->pretip_state.away_jump_altitude_q8
            : scene->pretip_state.home_jump_altitude_q8;
        tuple->velocity_q8[jumper] = jumper == 0U
            ? scene->pretip_state.away_jump_velocity_signed_q8
            : scene->pretip_state.home_jump_velocity_signed_q8;
    }
    tuple->jump_active = scene->pretip_jump_active;
}

static bool tipoff_trace_tuple_equal(
    const TecmoCliTipoffTraceJumpTuple *left,
    const TecmoCliTipoffTraceJumpTuple *right)
{
    return left != NULL && right != NULL &&
        memcmp(left, right, sizeof(*left)) == 0;
}

static bool tipoff_trace_jumpers_anchored(const TecmoGameplayScene *scene)
{
    size_t jumper;
    if (scene == NULL) return false;
    if (!scene->pretip_jump_active) return true;
    for (jumper = 0U; jumper < TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT;
         ++jumper) {
        uint8_t actor = scene->pretip_jumper_actor[jumper];
        if (actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
            scene->actors[actor].position.x != scene->actors[actor].anchor.x ||
            scene->actors[actor].position.y != scene->actors[actor].anchor.y) {
            return false;
        }
    }
    return true;
}

static bool tipoff_trace_write_png(const char *path,
                                   const uint32_t *pixels,
                                   uint8_t *rgba)
{
    size_t index;
    const size_t pixel_count = (size_t)TECMO_CLI_TIPOFF_TRACE_WIDTH *
        TECMO_CLI_TIPOFF_TRACE_HEIGHT;
    if (path == NULL || pixels == NULL || rgba == NULL) return false;
    for (index = 0U; index < pixel_count; ++index) {
        uint32_t pixel = pixels[index];
        rgba[index * 4U + 0U] = (uint8_t)((pixel >> 16U) & 0xFFU);
        rgba[index * 4U + 1U] = (uint8_t)((pixel >> 8U) & 0xFFU);
        rgba[index * 4U + 2U] = (uint8_t)(pixel & 0xFFU);
        rgba[index * 4U + 3U] = (uint8_t)((pixel >> 24U) & 0xFFU);
    }
    return png_write_rgba8(path, rgba, TECMO_CLI_TIPOFF_TRACE_WIDTH,
                           TECMO_CLI_TIPOFF_TRACE_HEIGHT) == 0;
}

static bool tipoff_trace_save_frame(const char *output_directory,
                                    const char *scenario_name,
                                    unsigned frame,
                                    const char *label,
                                    const uint32_t *pixels,
                                    uint8_t *rgba,
                                    char *path_out,
                                    size_t path_out_size)
{
    int written;
    if (output_directory == NULL || scenario_name == NULL || label == NULL ||
        path_out == NULL || path_out_size == 0U) {
        return false;
    }
    written = snprintf(path_out, path_out_size, "%s\\%s-%04u-%s.png",
                       output_directory, scenario_name, frame, label);
    if (written < 0 || (size_t)written >= path_out_size) return false;
    return tipoff_trace_write_png(path_out, pixels, rgba);
}

static bool tipoff_trace_write_row(FILE *csv,
                                   const TecmoCliTipoffTraceScenario *scenario,
                                   const TecmoGameplayScene *scene,
                                   bool render_ok)
{
    const TecmoGameplayPreTipState *state;
    uint8_t away_actor;
    uint8_t home_actor;
    if (csv == NULL || scenario == NULL || scene == NULL) return false;
    state = &scene->pretip_state;
    away_actor = scene->pretip_jumper_actor[0U];
    home_actor = scene->pretip_jumper_actor[1U];
    if (away_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT ||
        home_actor >= TECMO_GAMEPLAY_SCENE_ACTOR_COUNT) {
        return false;
    }
    return fprintf(csv, "%s,%u,%u,%s,%u,%u,%u,%u,",
                   scenario->name, scene->frame, state->total_frame,
                   tecmo_gameplay_pretip_phase_name(state->phase),
                   state->phase_frame, state->cinematic_visible ? 1U : 0U,
                   state->live_handoff ? 1U : 0U, render_ok ? 1U : 0U) >= 0 &&
        fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,",
                state->tip_capture_source_6a, state->tip_capture_clock,
                state->tip_capture_clock_ticks,
                state->tip_capture_clock_complete ? 1U : 0U,
                state->tip_capture_source_6a_initial,
                state->tip_capture_source_6a_current,
                state->tip_capture_source_mix_count,
                state->tip_capture_scheduler_phase,
                state->tip_capture_scheduler_yields,
                state->tip_capture_special_yields,
                state->away_tip_sampled ? 1U : 0U,
                state->away_tip_capture_clock, state->away_tip_error,
                state->away_tip_countdown) >= 0 &&
        fprintf(csv, "%u,%u,%u,%u,%u,%u,%u,%u,",
                state->home_tip_sampled ? 1U : 0U,
                state->home_tip_capture_clock, state->home_tip_error,
                state->home_tip_countdown,
                state->away_jump_committed ? 1U : 0U,
                state->home_jump_committed ? 1U : 0U,
                state->away_actor_state, state->away_animation_phase) >= 0 &&
        fprintf(csv, "%d,%u,%u,%u,%d,%u,%u,",
                (int)state->away_jump_velocity_signed_q8,
                state->away_jump_altitude_q8, state->home_actor_state,
                state->home_animation_phase,
                (int)state->home_jump_velocity_signed_q8,
                state->home_jump_altitude_q8,
                scene->pretip_jump_active ? 1U : 0U) >= 0 &&
        fprintf(csv, "%d,%d,%d,%d,%u,",
                scene->actors[away_actor].position.x,
                scene->actors[away_actor].position.y,
                scene->actors[away_actor].anchor.x,
                scene->actors[away_actor].anchor.y,
                scene->actors[away_actor].pose_index) >= 0 &&
        fprintf(csv, "%d,%d,%d,%d,%u,%u,%u,%u\n",
                scene->actors[home_actor].position.x,
                scene->actors[home_actor].position.y,
                scene->actors[home_actor].anchor.x,
                scene->actors[home_actor].anchor.y,
                scene->actors[home_actor].pose_index,
                scene->ball_holder, scene->pretip_state.receiver_actor,
                scene->state.possession) >= 0;
}

static bool tipoff_trace_capture_button_down(
    const TecmoCliTipoffTraceScenario *scenario,
    const TecmoGameplayScene *scene)
{
    unsigned start;
    unsigned end;
    if (scenario == NULL || scene == NULL ||
        scenario->capture_side == TECMO_CLI_TIPOFF_TRACE_CAPTURE_NONE) {
        return false;
    }
    start = TECMO_CLI_TIPOFF_TRACE_CAPTURE_SCENE_FRAME +
        scenario->capture_delay;
    end = start + scenario->capture_hold_frames;
    return scene->frame >= start && scene->frame < end;
}

static bool tipoff_trace_run_scenario(
    TecmoRuntime *runtime,
    const TecmoCliTipoffTraceScenario *scenario,
    const char *output_directory,
    uint32_t *pixels,
    uint8_t *rgba,
    char *failure,
    size_t failure_size)
{
    TecmoGameplaySceneLaunch launch;
    TecmoFramebuffer framebuffer;
    TecmoInput p1;
    TecmoInput p2;
    TecmoCliTipoffTraceJumpTuple frozen;
    static uint32_t last_court_pixels[
        TECMO_CLI_TIPOFF_TRACE_WIDTH * TECMO_CLI_TIPOFF_TRACE_HEIGHT];
    bool frozen_set = false;
    bool last_court_saved = false;
    bool last_court_available = false;
    bool first_cinematic_saved = false;
    bool returned_saved = false;
    bool visible_airborne_saved = false;
    bool live_handoff_saved = false;
    bool landing_saved = false;
    bool live_after_landing_saved = false;
    bool no_input_stall_saved = false;
    bool saw_resumed = false;
    bool saw_landing = false;
    unsigned live_renders_after_landing = 0U;
    unsigned last_court_frame = 0U;
    unsigned update;
    FILE *csv = NULL;
    char csv_path[1024];
    char image_path[1024];
    const char *diagnostic;

    if (runtime == NULL || scenario == NULL || output_directory == NULL ||
        pixels == NULL || rgba == NULL || failure == NULL || failure_size == 0U) {
        return false;
    }
    if (snprintf(csv_path, sizeof(csv_path), "%s\\%s-trace.csv",
                 output_directory, scenario->name) < 0) {
        (void)snprintf(failure, failure_size, "trace CSV path rejected");
        return false;
    }
    csv = fopen(csv_path, "wb");
    if (csv == NULL) {
        (void)snprintf(failure, failure_size, "cannot write %s", csv_path);
        return false;
    }
    (void)fprintf(csv,
        "scenario,scene_frame,total_frame,phase,phase_frame,cinematic,live_handoff,render_ok,"
        "capture_source_6a,capture_clock,capture_ticks,capture_complete,"
        "capture_source_initial,capture_source_current,capture_source_mix_count,"
        "capture_scheduler_phase,capture_scheduler_yields,capture_special_yields,"
        "away_sampled,away_capture_clock,away_error,away_countdown,"
        "home_sampled,home_capture_clock,home_error,home_countdown,"
        "away_committed,home_committed,away_state,away_phase,away_velocity_q8,away_altitude_q8,"
        "home_state,home_phase,home_velocity_q8,home_altitude_q8,jump_active,"
        "away_x,away_y,away_anchor_x,away_anchor_y,away_pose,"
        "home_x,home_y,home_anchor_x,home_anchor_y,home_pose,holder,receiver,possession\n");
    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = scenario->away_team;
    launch.home_team = scenario->home_team;
    launch.regulation_minutes = 2U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.game_music_enabled = false;
    launch.controller_team[0U] = scenario->controller_team[0U];
    launch.controller_team[1U] = scenario->controller_team[1U];
    if (!tecmo_gameplay_scene_launch(&runtime->gameplay_scene, &launch)) {
        (void)snprintf(failure, failure_size, "%s launch rejected: %s",
                       scenario->name, runtime->gameplay_scene.status);
        fclose(csv);
        return false;
    }
    if (runtime->gameplay_scene.pretip_team_data == NULL ||
        strcmp(runtime->gameplay_scene.pretip_team_data->teams[
                   scenario->away_team].city, scenario->away_city) != 0 ||
        strcmp(runtime->gameplay_scene.pretip_team_data->teams[
                   scenario->away_team].nickname, scenario->away_nickname) != 0 ||
        strcmp(runtime->gameplay_scene.pretip_team_data->teams[
                   scenario->home_team].city, scenario->home_city) != 0 ||
        strcmp(runtime->gameplay_scene.pretip_team_data->teams[
                   scenario->home_team].nickname, scenario->home_nickname) != 0) {
        (void)snprintf(failure, failure_size,
                       "%s team selector mapping rejected", scenario->name);
        fclose(csv);
        return false;
    }
    tecmo_runtime_set_mode(runtime, TECMO_MODE_COURT);
    memset(&framebuffer, 0, sizeof(framebuffer));
    framebuffer.pixels = pixels;
    framebuffer.width = TECMO_CLI_TIPOFF_TRACE_WIDTH;
    framebuffer.height = TECMO_CLI_TIPOFF_TRACE_HEIGHT;
    framebuffer.pitch_pixels = TECMO_CLI_TIPOFF_TRACE_WIDTH;
    memset(&frozen, 0, sizeof(frozen));
    for (update = 0U; update < TECMO_CLI_TIPOFF_TRACE_MAX_UPDATES; ++update) {
        TecmoGameplayScene *scene = &runtime->gameplay_scene;
        bool render_ok;
        memset(&p1, 0, sizeof(p1));
        memset(&p2, 0, sizeof(p2));
        if (tipoff_trace_capture_button_down(scenario, scene)) {
            if (scenario->capture_side == TECMO_CLI_TIPOFF_TRACE_CAPTURE_AWAY)
                p1.cancel = true;
            else if (scenario->capture_side ==
                     TECMO_CLI_TIPOFF_TRACE_CAPTURE_HOME)
                p2.cancel = true;
        }
        tecmo_runtime_update_players(runtime, &p1, &p2);
        scene = &runtime->gameplay_scene;
        if (!scene->active || strstr(scene->status, "rejected") != NULL) {
            (void)snprintf(failure, failure_size,
                           "%s update rejected at scene frame %u: %s",
                           scenario->name, scene->frame, scene->status);
            fclose(csv);
            return false;
        }
        render_ok = tecmo_render_gameplay_scene(runtime, &framebuffer);
        if (!tipoff_trace_write_row(csv, scenario, scene, render_ok)) {
            (void)snprintf(failure, failure_size, "%s trace write failed",
                           scenario->name);
            fclose(csv);
            return false;
        }
        if (!render_ok) {
            diagnostic = tecmo_gameplay_scene_render_diagnostic(
                scene, &framebuffer, 64, 0, 2, true);
            (void)snprintf(failure, failure_size,
                           "%s GAMEPLAY RENDER REJECTED at scene frame %u: %s",
                           scenario->name, scene->frame,
                           diagnostic != NULL ? diagnostic : "draw composition");
            fclose(csv);
            return false;
        }
        if (!tipoff_trace_jumpers_anchored(scene)) {
            (void)snprintf(failure, failure_size,
                           "%s tipped jumper was double-processed at scene frame %u",
                           scenario->name, scene->frame);
            fclose(csv);
            return false;
        }
        if (scenario->capture_side != TECMO_CLI_TIPOFF_TRACE_CAPTURE_NONE &&
            scene->pretip_state.total_frame ==
                TECMO_CLI_PRETIP_CAPTURE_FRAME + scenario->capture_delay) {
            uint8_t expected_clock = (uint8_t)(
                0xE1U + (scenario->capture_delay + 1U) / 2U);
            uint8_t expected_delta = (uint8_t)(0xF9U - expected_clock);
            uint8_t expected_absolute = (expected_delta & 0x80U) != 0U
                ? (uint8_t)(0U - expected_delta) : expected_delta;
            uint8_t expected_error = expected_absolute < 0x0CU
                ? expected_absolute : 0x0BU;
            bool away = scenario->capture_side ==
                TECMO_CLI_TIPOFF_TRACE_CAPTURE_AWAY;
            uint8_t sampled = away ? scene->pretip_state.away_tip_sampled
                                    : scene->pretip_state.home_tip_sampled;
            uint8_t captured = away
                ? scene->pretip_state.away_tip_capture_clock
                : scene->pretip_state.home_tip_capture_clock;
            uint8_t error = away ? scene->pretip_state.away_tip_error
                                 : scene->pretip_state.home_tip_error;
            uint8_t countdown = away ? scene->pretip_state.away_tip_countdown
                                     : scene->pretip_state.home_tip_countdown;
            if (!sampled || captured != expected_clock ||
                error != expected_error || countdown != expected_error ||
                captured == 0xF9U || error == 0U) {
                (void)snprintf(failure, failure_size,
                               "%s did not retain Bank04 capture clock $%02X/error %u",
                               scenario->name, expected_clock,
                               (unsigned)expected_error);
                fclose(csv);
                return false;
            }
            if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "capture", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size, "%s capture PNG failed",
                               scenario->name);
                fclose(csv);
                return false;
            }
        }
        if (scenario->expect_cinematic && !visible_airborne_saved &&
            !scene->pretip_state.cinematic_visible &&
            (scene->pretip_state.away_jump_altitude_q8 != 0U ||
             scene->pretip_state.home_jump_altitude_q8 != 0U)) {
            if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "visible-airborne",
                                         pixels, rgba, image_path,
                                         sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s did not visibly jump before cutaway",
                               scenario->name);
                fclose(csv);
                return false;
            }
            visible_airborne_saved = true;
        }
        if (scenario->expect_cinematic &&
            scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT) {
            /* Keep the actual final court framebuffer, then emit it when the
             * next update enters state $17's cutaway.  The primary capture
             * reaches this at frame 515; a later valid capture has a different
             * last court frame and must not inherit that fixture convention. */
            memcpy(last_court_pixels, pixels, sizeof(last_court_pixels));
            last_court_frame = scene->frame;
            last_court_available = true;
        }
        if (scene->pretip_state.phase == TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
            TecmoCliTipoffTraceJumpTuple tuple;
            if (!last_court_saved &&
                (!last_court_available ||
                 !tipoff_trace_save_frame(output_directory, scenario->name,
                                          last_court_frame, "last-court",
                                          last_court_pixels, rgba, image_path,
                                          sizeof(image_path)))) {
                (void)snprintf(failure, failure_size,
                               "%s last-court PNG failed", scenario->name);
                fclose(csv);
                return false;
            }
            last_court_saved = true;
            tipoff_trace_capture_tuple(scene, &tuple);
            if (!frozen_set) {
                frozen = tuple;
                frozen_set = true;
                if (!visible_airborne_saved) {
                    (void)snprintf(failure, failure_size,
                                   "%s entered cutaway before a human jump was visible",
                                   scenario->name);
                    fclose(csv);
                    return false;
                }
                if (scenario->expect_exact_slow_cinematic &&
                    scene->pretip_state.first_cinematic_frame != 516U) {
                    (void)snprintf(failure, failure_size,
                                   "%s cinematic frame %u is not scheduler-derived 516",
                                   scenario->name,
                                   (unsigned)scene->pretip_state.first_cinematic_frame);
                    fclose(csv);
                    return false;
                }
            } else if (!tipoff_trace_tuple_equal(&frozen, &tuple)) {
                (void)snprintf(failure, failure_size,
                               "%s cinematic changed frozen jumper tuple",
                               scenario->name);
                fclose(csv);
                return false;
            }
            if (scene->pretip_state.phase_frame == 0U && !first_cinematic_saved) {
                if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                             scene->frame, "first-cinematic",
                                             pixels, rgba, image_path,
                                             sizeof(image_path))) {
                    (void)snprintf(failure, failure_size,
                                   "%s first-cinematic PNG failed", scenario->name);
                    fclose(csv);
                    return false;
                }
                first_cinematic_saved = true;
            }
            if (scene->pretip_state.phase_frame == 30U) {
                if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                             scene->frame, "cinematic-middle",
                                             pixels, rgba, image_path,
                                             sizeof(image_path))) {
                    (void)snprintf(failure, failure_size,
                                   "%s cinematic-middle PNG failed", scenario->name);
                    fclose(csv);
                    return false;
                }
            }
            if (scene->pretip_state.phase_frame == 59U) {
                if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                             scene->frame, "cinematic-last",
                                             pixels, rgba, image_path,
                                             sizeof(image_path))) {
                    (void)snprintf(failure, failure_size,
                                   "%s cinematic-last PNG failed", scenario->name);
                    fclose(csv);
                    return false;
                }
            }
        }
        if (frozen_set && scene->pretip_state.phase ==
                TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
            scene->pretip_state.phase_frame == 0U && !returned_saved) {
            TecmoCliTipoffTraceJumpTuple tuple;
            tipoff_trace_capture_tuple(scene, &tuple);
            if (!tipoff_trace_tuple_equal(&frozen, &tuple) ||
                (tuple.altitude_q8[0U] == 0U && tuple.altitude_q8[1U] == 0U) ||
                !tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "court-return", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s court return changed or grounded frozen tuple",
                               scenario->name);
                fclose(csv);
                return false;
            }
            returned_saved = true;
        }
        if (returned_saved && scene->pretip_state.phase ==
                TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
            scene->pretip_state.phase_frame == 1U) {
            TecmoCliTipoffTraceJumpTuple tuple;
            tipoff_trace_capture_tuple(scene, &tuple);
            if (tipoff_trace_tuple_equal(&frozen, &tuple) ||
                !tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "resumed-physics", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s court return did not resume jumper physics",
                               scenario->name);
                fclose(csv);
                return false;
            }
            saw_resumed = true;
        }
        if (scene->pretip_state.live_handoff &&
            scene->pretip_state.phase_frame == 0U && !live_handoff_saved) {
            if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "live-handoff", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s live-handoff PNG failed", scenario->name);
                fclose(csv);
                return false;
            }
            live_handoff_saved = true;
        }
        if (frozen_set && !landing_saved &&
            (!scene->pretip_state.away_jump_committed ||
             scene->pretip_state.away_actor_state == 0x13U) &&
            (!scene->pretip_state.home_jump_committed ||
             scene->pretip_state.home_actor_state == 0x13U)) {
            if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "natural-landing", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s natural-landing PNG failed", scenario->name);
                fclose(csv);
                return false;
            }
            landing_saved = true;
            saw_landing = true;
        }
        if (landing_saved && scene->pretip_state.live_handoff) {
            ++live_renders_after_landing;
            if (live_renders_after_landing >= 10U && !live_after_landing_saved) {
                if (!tipoff_trace_save_frame(output_directory, scenario->name,
                                             scene->frame, "live-after-landing",
                                             pixels, rgba, image_path,
                                             sizeof(image_path))) {
                    (void)snprintf(failure, failure_size,
                                   "%s live-after-landing PNG failed", scenario->name);
                    fclose(csv);
                    return false;
                }
                live_after_landing_saved = true;
            }
        }
        if (!scenario->expect_cinematic && scene->frame >= 600U &&
            !no_input_stall_saved) {
            if (scene->pretip_state.cinematic_visible ||
                scene->pretip_state.away_jump_committed ||
                scene->pretip_state.home_jump_committed ||
                !tipoff_trace_save_frame(output_directory, scenario->name,
                                         scene->frame, "no-input-stall", pixels,
                                         rgba, image_path, sizeof(image_path))) {
                (void)snprintf(failure, failure_size,
                               "%s no-input route unexpectedly resolved tip",
                               scenario->name);
                fclose(csv);
                return false;
            }
            no_input_stall_saved = true;
        }
        if (scenario->expect_cinematic && first_cinematic_saved &&
            returned_saved && saw_resumed && saw_landing &&
            live_after_landing_saved) {
            break;
        }
        if (!scenario->expect_cinematic && no_input_stall_saved) break;
    }
    fclose(csv);
    if (scenario->expect_cinematic &&
        (!frozen_set || !last_court_saved || !first_cinematic_saved ||
         !visible_airborne_saved || !returned_saved || !saw_resumed || !saw_landing ||
         !live_after_landing_saved)) {
        (void)snprintf(failure, failure_size,
                       "%s did not complete continuous tip lifecycle", scenario->name);
        return false;
    }
    if (!scenario->expect_cinematic && !no_input_stall_saved) {
        (void)snprintf(failure, failure_size,
                       "%s did not retain no-input stall", scenario->name);
        return false;
    }
    printf("tipoff-regression scenario=%s result=PASS csv=%s\n",
           scenario->name, csv_path);
    return true;
}

int tecmo_cli_run_tipoff_regression_trace(const char *project_root,
                                          const char *asset_pack_path,
                                          const char *output_directory)
{
    static const TecmoCliTipoffTraceScenario scenarios[] = {
        {
            "bulls-pacers-away-pulse", 3U, 10U,
            "CHICAGO", "BULLS", "INDIANA", "PACERS",
            {TECMO_GAMEPLAY_TEAM_AWAY, TECMO_GAMEPLAY_TEAM_HOME},
            TECMO_CLI_TIPOFF_TRACE_CAPTURE_AWAY, 0U, 1U, true, true
        },
        {
            "new-york-philadelphia-home-hold", 17U, 19U,
            "NEW YORK", "KNICKS", "PHILADELPHIA", "SEVENTY SIXERS",
            {TECMO_GAMEPLAY_TEAM_AWAY, TECMO_GAMEPLAY_TEAM_HOME},
            TECMO_CLI_TIPOFF_TRACE_CAPTURE_HOME, 3U, 3U, true, false
        },
        {
            "bulls-pacers-cpu-only", 3U, 10U,
            "CHICAGO", "BULLS", "INDIANA", "PACERS",
            {TECMO_GAMEPLAY_SCENE_NO_TEAM, TECMO_GAMEPLAY_SCENE_NO_TEAM},
            TECMO_CLI_TIPOFF_TRACE_CAPTURE_NONE, 0U, 0U, true, false
        },
        {
            "bulls-pacers-no-input", 3U, 10U,
            "CHICAGO", "BULLS", "INDIANA", "PACERS",
            {TECMO_GAMEPLAY_TEAM_AWAY, TECMO_GAMEPLAY_TEAM_HOME},
            TECMO_CLI_TIPOFF_TRACE_CAPTURE_NONE, 0U, 0U, false, false
        }
    };
    TecmoGameMemory memory;
    TecmoRuntime *runtime = NULL;
    void *permanent_block = NULL;
    void *transient_block = NULL;
    uint32_t *pixels = NULL;
    uint8_t *rgba = NULL;
    const size_t pixel_count = (size_t)TECMO_CLI_TIPOFF_TRACE_WIDTH *
        TECMO_CLI_TIPOFF_TRACE_HEIGHT;
    char failure[256];
    size_t scenario;
    int result = 1;

    if (project_root == NULL || asset_pack_path == NULL ||
        output_directory == NULL) {
        printf("Tip-off regression trace requires root, asset pack, and output directory\n");
        return 2;
    }
    memset(&memory, 0, sizeof(memory));
    memset(failure, 0, sizeof(failure));
    runtime = (TecmoRuntime *)calloc(1U, sizeof(*runtime));
    permanent_block = malloc(TECMO_CLI_TIPOFF_TRACE_PERMANENT_BYTES);
    transient_block = malloc(TECMO_CLI_TIPOFF_TRACE_TRANSIENT_BYTES);
    pixels = (uint32_t *)malloc(pixel_count * sizeof(*pixels));
    rgba = (uint8_t *)malloc(pixel_count * 4U);
    if (runtime == NULL || permanent_block == NULL || transient_block == NULL ||
        pixels == NULL || rgba == NULL) {
        printf("Tip-off regression trace allocation failed\n");
        goto done;
    }
    tecmo_arena_init(&memory.permanent, permanent_block,
                     TECMO_CLI_TIPOFF_TRACE_PERMANENT_BYTES);
    tecmo_arena_init(&memory.transient, transient_block,
                     TECMO_CLI_TIPOFF_TRACE_TRANSIENT_BYTES);
    if (!tecmo_runtime_init_with_flags(
            runtime, &memory, project_root,
            TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER)) {
        printf("Tip-off regression runtime initialization failed\n");
        goto done;
    }
    tecmo_gameplay_scene_destroy(&runtime->gameplay_scene);
    tecmo_gameplay_scene_init(&runtime->gameplay_scene);
    if (!tecmo_gameplay_scene_load(&runtime->gameplay_scene, project_root,
                                  asset_pack_path, &runtime->music_player)) {
        printf("Tip-off regression scene load failed: %s\n",
               runtime->gameplay_scene.status);
        goto done;
    }
    for (scenario = 0U;
         scenario < sizeof(scenarios) / sizeof(scenarios[0U]); ++scenario) {
        if (!tipoff_trace_run_scenario(runtime, &scenarios[scenario],
                                       output_directory, pixels, rgba,
                                       failure, sizeof(failure))) {
            printf("Tip-off regression trace failed: %s\n", failure);
            goto done;
        }
    }
    printf("TIP-OFF CONTINUOUS REGRESSION TRACE PASS\n");
    result = 0;

done:
    if (runtime != NULL) tecmo_runtime_shutdown(runtime);
    free(runtime);
    free(permanent_block);
    free(transient_block);
    free(pixels);
    free(rgba);
    return result;
}
