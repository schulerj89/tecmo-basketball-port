#include "tecmo_controls.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_pretip.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_win32_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

#define TECMO_CLI_PRETIP_CONTEST_START_FRAME 661U
#define TECMO_CLI_PRETIP_LIVE_START_FRAME \
    (TECMO_CLI_PRETIP_CONTEST_START_FRAME + \
     TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES)
#define TECMO_CLI_TIPOFF_PROOF_LAST_FRAME \
    (TECMO_CLI_PRETIP_LIVE_START_FRAME + 4U)
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
    bool ball_bounce;
    bool cpu_steering;
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
    bool ball_bounce = false;
    bool cpu_steering = false;
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
        checkpoint = TECMO_CLI_PRETIP_CONTEST_START_FRAME;
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
                   mode_name, "gameplay-ball-bounce-frame", &checkpoint)) {
        ball_bounce = true;
    } else if (tecmo_cli_parse_render_frame_suffix(
                   mode_name, "gameplay-cpu-steering-frame", &checkpoint)) {
        cpu_steering = true;
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
        (checkpoint < TECMO_CLI_PRETIP_CONTEST_START_FRAME ||
         checkpoint > TECMO_CLI_TIPOFF_PROOF_LAST_FRAME)) {
        return false;
    }
    if (ball_bounce && (checkpoint == 0U || checkpoint > 15U)) {
        return false;
    }
    if (cpu_steering && (checkpoint == 0U || checkpoint > 240U)) {
        return false;
    }
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
    config->ball_bounce = ball_bounce;
    config->cpu_steering = cpu_steering;
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
        (away->facing_right ||
         scene->pretip_jumper_altitude_q8[0U] != 0U ||
         scene->pretip_jumper_altitude_q8[1U] != 0U)) {
        return false;
    }
    printf(
        "tipoff-proof frame=%u pretip=%s pretip-frame=%u contest-frame=%u "
        "away-actor=%u away-world-y=%d away-screen-y=%u away-visible=%u "
        "away-pose=%u away-altitude-q8=%u away-facing-right=%u "
        "away-pose-encoded=%u away-anchor-x=%d home-actor=%u "
        "home-world-y=%d "
        "home-screen-y=%u home-visible=%u home-pose=%u "
        "home-altitude-q8=%u home-facing-right=%u home-pose-encoded=%u "
        "home-anchor-x=%d left-actor=%u left-facing-right=%u "
        "right-actor=%u right-facing-right=%u "
        "away-sampled=%u away-sample-frame=%u away-error=%u "
        "home-sampled=%u home-sample-frame=%u home-error=%u "
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
        (unsigned)away->pose_index,
        (unsigned)scene->pretip_jumper_altitude_q8[0U],
        away->facing_right ? 1U : 0U,
        away->pose_orientation_encoded ? 1U : 0U,
        (int)away->anchor.x,
        (unsigned)home_actor, (int)home->position.y,
        (unsigned)court_frame.projection.players[home_actor].screen_y,
        court_frame.projection.players[home_actor].visible ? 1U : 0U,
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
        (unsigned)scene->pretip_state.away_tip_error,
        scene->pretip_state.home_tip_sampled ? 1U : 0U,
        (unsigned)scene->pretip_state.home_tip_sample_frame,
        (unsigned)scene->pretip_state.home_tip_error,
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
    const bool tipoff_proof = config->tipoff_proof;
    const bool ball_bounce = config->ball_bounce;
    const bool cpu_steering = config->cpu_steering;
    const int possession_slice = config->possession_slice;
    const int free_throw_orientation = config->free_throw_orientation;
    const unsigned first_contest_update =
        TECMO_CLI_PRETIP_CONTEST_START_FRAME + 1U;
    const unsigned home_cpu_sample_frame =
        TECMO_CLI_PRETIP_CONTEST_START_FRAME +
        TECMO_GAMEPLAY_PRETIP_AUTOMATIC_SINGLE_FRAME + 1U;
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
    launch.controller_team[0] = cpu_steering
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
                scene->frame == TECMO_CLI_PRETIP_CONTEST_START_FRAME &&
                scene->pretip_state.phase ==
                    TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
                scene->pretip_state.phase_frame == 0U) {
                if (!gameplay_checkpoint_run_fast_x_bridge(
                    runtime, &keyboard, controls, &input_evidence)) {
                    return false;
                }
            } else if (!gameplay_checkpoint_run_adapter_update(
                           runtime, &keyboard, controls, &input_evidence)) {
                return false;
            }
        }
        if (tipoff_proof) {
            *done_out = true;
            if (runtime->mode != TECMO_MODE_COURT || !scene->active ||
                scene->frame != checkpoint ||
                (checkpoint < TECMO_CLI_PRETIP_LIVE_START_FRAME) !=
                    tecmo_gameplay_scene_in_pretip(scene) ||
                (checkpoint == TECMO_CLI_PRETIP_CONTEST_START_FRAME &&
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
                  !scene->pretip_state.away_tip_sampled ||
                  scene->pretip_state.away_tip_sample_frame != 0U ||
                  scene->pretip_state.away_tip_error != 0U)) ||
                (checkpoint < first_contest_update &&
                 (input_evidence.bridge_used ||
                  input_evidence.raw_x_down ||
                  input_evidence.raw_x_up ||
                  input_evidence.bridge_begin_count != checkpoint ||
                  input_evidence.bridge_end_count != checkpoint ||
                  input_evidence.bridge_update_players_count != checkpoint ||
                  input_evidence.fast_x_pulse_frame != 0xFFFFFFFFU)) ||
                (checkpoint < home_cpu_sample_frame &&
                 scene->pretip_state.home_tip_sampled) ||
                (checkpoint >= home_cpu_sample_frame &&
                 (!scene->pretip_state.home_tip_sampled ||
                  scene->pretip_state.home_tip_sample_frame !=
                      TECMO_GAMEPLAY_PRETIP_AUTOMATIC_SINGLE_FRAME ||
                  scene->pretip_state.home_tip_error !=
                      TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR)) ||
                (checkpoint >= TECMO_CLI_PRETIP_LIVE_START_FRAME &&
                 (scene->state.possession != TECMO_GAMEPLAY_TEAM_AWAY ||
                  scene->ball_holder != 0U))) {
                return false;
            }
            return gameplay_checkpoint_report_tipoff_proof(
                scene, &input_evidence);
        }
        if (live_start) {
            *done_out = true;
            return runtime->mode == TECMO_MODE_COURT && scene->active &&
                   !tecmo_gameplay_scene_in_pretip(scene) &&
                   scene->state.possession == TECMO_GAMEPLAY_TEAM_AWAY &&
                   scene->ball_holder == 0U;
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
    for (update = 0U; update < TECMO_CLI_PRETIP_LIVE_START_FRAME; ++update)
        tecmo_runtime_update(runtime, &input);
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
        for (update = 0U; update < checkpoint; ++update) {
            tecmo_runtime_update(runtime, &input);
        }
        *done_out = true; return runtime->mode == TECMO_MODE_COURT && scene->active &&
               scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
               scene->frame == TECMO_CLI_PRETIP_LIVE_START_FRAME + checkpoint &&
               scene->ball_holder == 0U &&
               scene->actors[0].position.x < 0x0160 &&
               scene->cpu_actors[0].decision_serial == checkpoint &&
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
        if (possession_slice == 1) {
            return runtime->mode == TECMO_MODE_COURT &&
                   scene->active &&
                   tecmo_gameplay_scene_court_frame(
                       scene, &court_frame) &&
                    court_frame.slice.viewport.camera_x == 0x0084U &&
                    court_frame.projection.camera_x == 0x0084U &&
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
        actor->position.x = 0x013CU;
        actor->anchor.x = actor->position.x;
        runtime->gameplay_scene.ball_position.x_q8 =
            (int32_t)(actor->position.x + 7) * 256;
        if (jump_make) {
            actor->position.y = 180;
            actor->anchor.y = 180;
            runtime->gameplay_scene.ball_position.y_q8 =
                (int32_t)(180 - 18) * 256;
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
