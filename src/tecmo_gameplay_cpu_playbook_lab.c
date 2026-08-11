#include "tecmo_gameplay_cpu_playbook_lab.h"

#include "tecmo_gameplay_scene_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool cpu_playbook_lab_scene_eligible(const TecmoGameplayScene *scene)
{
    /* The preview invokes the native C TGAI->TGMO slice, not a ROM/RAM
       interpreter. Bank06 $81F7-$82D3 only applies to ordinary LIVE actors;
       presentation, shots, free throws, and legacy target policy stay out. */
    return scene != NULL && scene->available && scene->active &&
           scene->state.initialized &&
           scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
           scene->state.free_throws.attempts_remaining == 0U &&
           !tecmo_gameplay_scene_in_pretip(scene) &&
           scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           !scene->free_throw_lineup_active &&
           !tecmo_gameplay_scene_in_dunk_presentation(scene) &&
           !scene->legacy_direct_launch &&
           scene->cpu_steering_assets.available &&
           scene->movement_assets.available &&
           scene->ball_holder < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
}

static bool cpu_playbook_lab_actor_controlled(
    const TecmoGameplayScene *scene,
    uint8_t actor)
{
    size_t controller;
    if (scene == NULL || actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    for (controller = 0U; controller < TECMO_GAMEPLAY_CONTROLLER_COUNT;
         ++controller) {
        if (scene->launch.controller_team[controller] !=
                TECMO_GAMEPLAY_SCENE_NO_TEAM &&
            scene->controlled_actor[controller] == actor) {
            return true;
        }
    }
    return false;
}

static TecmoGameplayCpuPlaybookLabDeferReason
cpu_playbook_lab_slice_skip_reason(const TecmoGameplayScene *scene,
                                   uint8_t actor)
{
    const TecmoGameplayLiveFoundation *foundation;
    if (scene == NULL || actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_NONE;
    }
    foundation = &scene->live_foundation;
    if (cpu_playbook_lab_actor_controlled(scene, actor)) {
        return TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_CONTROLLED_ACTOR;
    }
    /* Bank06 $81F7-$82D3 does not run the selected defender's ordinary
       stream; the native $0309 handoff target is shown separately. */
    if (foundation->selected_defender_handoff_active &&
        foundation->defender_actor == actor) {
        return TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_SELECTED_DEFENDER_STREAM_EXCLUDED;
    }
    return TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_NONE;
}

static void cpu_playbook_lab_clear_step_evidence(
    TecmoGameplayCpuPlaybookLab *lab)
{
    if (lab == NULL) return;
    memset(lab->last_step_delta_x, 0, sizeof(lab->last_step_delta_x));
    memset(lab->last_step_delta_y, 0, sizeof(lab->last_step_delta_y));
    memset(lab->moved_last_step, 0, sizeof(lab->moved_last_step));
    memset(lab->boundary_latched_before, 0,
           sizeof(lab->boundary_latched_before));
    memset(lab->boundary_latched_after, 0,
           sizeof(lab->boundary_latched_after));
}

static void cpu_playbook_lab_reset_preview(TecmoGameplayCpuPlaybookLab *lab)
{
    if (lab == NULL || lab->baseline_scene == NULL ||
        lab->preview_scene == NULL) {
        return;
    }
    *lab->preview_scene = *lab->baseline_scene;
    lab->preview_tick = 0U;
    lab->last_step_status = TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_NONE;
    cpu_playbook_lab_clear_step_evidence(lab);
}

static bool cpu_playbook_lab_formation_buckets(
    const TecmoGameplayScene *scene,
    uint8_t *x_bucket_out,
    uint8_t *depth_bucket_out)
{
    const TecmoGameplayLiveFoundation *foundation;
    const TecmoGameplayCourtCoordinate *selected;
    if (scene == NULL || x_bucket_out == NULL || depth_bucket_out == NULL) {
        return false;
    }
    foundation = &scene->live_foundation;
    /* Bank06 $938B receives the selected $0308 actor position. The separate
       ball coordinate is valid opcode-4 C8 evidence, but is not formation
       selector input. */
    if (foundation->primary_actor >= TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        return false;
    }
    selected = &scene->actors[foundation->primary_actor].position;
    if (!tecmo_gameplay_court_coordinate_valid(selected)) return false;
    *x_bucket_out = (uint8_t)((uint16_t)selected->x >> 6U);
    *depth_bucket_out = (uint8_t)((uint16_t)selected->y >> 6U);
    return *x_bucket_out < 12U && *depth_bucket_out < 4U;
}

static bool cpu_playbook_lab_step(TecmoGameplayCpuPlaybookLab *lab)
{
    TecmoGameplayScene candidate;
    TecmoGameplaySceneCpuShotRequest shot_request;
    TecmoGameplayCourtCoordinate before[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool before_boundary[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    size_t actor;

    if (lab == NULL || !lab->active || lab->preview_scene == NULL ||
        !cpu_playbook_lab_scene_eligible(lab->preview_scene)) {
        if (lab != NULL) {
            lab->last_step_status =
                TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SCENE_REJECTED;
        }
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        before[actor] = lab->preview_scene->actors[actor].position;
        before_boundary[actor] =
            lab->preview_scene->actors[actor].movement_boundary_latched;
    }
    candidate = *lab->preview_scene;
    memset(&shot_request, 0, sizeof(shot_request));
    if (!scene_update_ai(&candidate, &shot_request)) {
        lab->last_step_status =
            TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SCENE_REJECTED;
        return false;
    }
    /* The lab's scope ends at the AI movement slice. A shot is a separate
       scene/animation lifecycle with unproven CPU policy inputs, so discard
       this candidate instead of presenting a partial shot as a play stream. */
    if (shot_request.requested ||
        candidate.shot_kind != TECMO_GAMEPLAY_SCENE_SHOT_NONE) {
        lab->last_step_status =
            TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SHOT_HANDOFF_REJECTED;
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        const TecmoGameplaySceneActor *after = &candidate.actors[actor];
        lab->last_step_delta_x[actor] = (int16_t)(after->position.x -
                                                   before[actor].x);
        lab->last_step_delta_y[actor] = (int16_t)(after->position.y -
                                                   before[actor].y);
        lab->moved_last_step[actor] =
            lab->last_step_delta_x[actor] != 0 ||
            lab->last_step_delta_y[actor] != 0;
        lab->boundary_latched_before[actor] = before_boundary[actor];
        lab->boundary_latched_after[actor] = after->movement_boundary_latched;
    }
    *lab->preview_scene = candidate;
    ++lab->preview_tick;
    lab->last_step_status = TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_ACCEPTED;
    return true;
}

void tecmo_gameplay_cpu_playbook_lab_init(TecmoGameplayCpuPlaybookLab *lab)
{
    if (lab != NULL) memset(lab, 0, sizeof(*lab));
}

bool tecmo_gameplay_cpu_playbook_lab_open(
    TecmoGameplayCpuPlaybookLab *lab,
    const TecmoGameplayScene *scene)
{
    TecmoGameplayScene *baseline;
    TecmoGameplayScene *preview;

    if (lab == NULL || scene == NULL || lab->active ||
        !cpu_playbook_lab_scene_eligible(scene)) {
        return false;
    }
    baseline = (TecmoGameplayScene *)calloc(1U, sizeof(*baseline));
    preview = (TecmoGameplayScene *)calloc(1U, sizeof(*preview));
    if (baseline == NULL || preview == NULL) {
        free(baseline);
        free(preview);
        return false;
    }
    /* These are intentionally shallow copies. They alias immutable loaded
       assets but no preview path may destroy, reload, or otherwise own them. */
    *baseline = *scene;
    *preview = *scene;
    tecmo_gameplay_cpu_playbook_lab_init(lab);
    lab->active = true;
    lab->paused = true;
    lab->selected_actor = 0U;
    lab->baseline_scene = baseline;
    lab->preview_scene = preview;
    return true;
}

void tecmo_gameplay_cpu_playbook_lab_close(TecmoGameplayCpuPlaybookLab *lab)
{
    if (lab == NULL) return;
    /* No restoration write is needed: every action stayed in preview_scene.
       Free only the shallow wrapper allocation, never aliased scene assets. */
    free(lab->baseline_scene);
    free(lab->preview_scene);
    tecmo_gameplay_cpu_playbook_lab_init(lab);
}

bool tecmo_gameplay_cpu_playbook_lab_update(
    TecmoGameplayCpuPlaybookLab *lab,
    const TecmoControlFrame *controls)
{
    if (lab == NULL || controls == NULL || !lab->active) return false;
    if (controls->pressed.violation_lab_previous) {
        lab->selected_actor = lab->selected_actor == 0U
            ? (uint8_t)(TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT - 1U)
            : (uint8_t)(lab->selected_actor - 1U);
    }
    if (controls->pressed.violation_lab_next) {
        lab->selected_actor = (uint8_t)(
            (lab->selected_actor + 1U) %
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT);
    }
    if (controls->pressed.violation_lab_path) {
        lab->showing_baseline = !lab->showing_baseline;
    }
    if (controls->pressed.violation_lab_play_pause) {
        lab->paused = !lab->paused;
    }
    if (controls->pressed.violation_lab_restart) {
        cpu_playbook_lab_reset_preview(lab);
        lab->paused = true;
    }
    if (controls->pressed.violation_lab_step) {
        lab->paused = true;
        (void)cpu_playbook_lab_step(lab);
    } else if (!lab->paused) {
        if (!cpu_playbook_lab_step(lab)) lab->paused = true;
    }
    return true;
}

bool tecmo_gameplay_cpu_playbook_lab_set_entry_provenance(
    TecmoGameplayCpuPlaybookLab *lab,
    bool direct_fixture_input,
    bool organic_live_entry)
{
    if (lab == NULL || !lab->active ||
        (direct_fixture_input && organic_live_entry)) {
        return false;
    }
    lab->direct_fixture_input = direct_fixture_input;
    lab->organic_live_entry = organic_live_entry;
    return true;
}

const TecmoGameplayScene *tecmo_gameplay_cpu_playbook_lab_render_scene(
    const TecmoGameplayCpuPlaybookLab *lab)
{
    if (lab == NULL || !lab->active) return NULL;
    return lab->showing_baseline ? lab->baseline_scene : lab->preview_scene;
}

bool tecmo_gameplay_cpu_playbook_lab_snapshot(
    const TecmoGameplayCpuPlaybookLab *lab,
    TecmoGameplayCpuPlaybookLabSnapshot *snapshot_out)
{
    const TecmoGameplayScene *scene;
    const TecmoGameplayLiveFoundation *foundation;
    size_t actor;

    if (lab == NULL || snapshot_out == NULL || !lab->active) {
        return false;
    }
    /* F7's table and machine-readable state must describe the same frozen
       scene rendered at left. Baseline has no preview-step delta evidence. */
    scene = tecmo_gameplay_cpu_playbook_lab_render_scene(lab);
    if (scene == NULL) return false;
    foundation = &scene->live_foundation;
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    snapshot_out->preview_tick = lab->preview_tick;
    snapshot_out->paused = lab->paused;
    snapshot_out->showing_baseline = lab->showing_baseline;
    snapshot_out->direct_fixture_input = lab->direct_fixture_input;
    snapshot_out->organic_live_entry = lab->organic_live_entry;
    snapshot_out->last_step_status = lab->last_step_status;
    snapshot_out->ball_holder = scene->ball_holder;
    snapshot_out->primary_actor = foundation->primary_actor;
    snapshot_out->defender_actor = foundation->defender_actor;
    snapshot_out->ball_object_slot =
        TECMO_GAMEPLAY_CPU_STEERING_BALL_OBJECT_SLOT;
    if (foundation->offense_side < TECMO_GAMEPLAY_CPU_STEERING_TEAM_COUNT &&
        foundation->candidate_actor_by_side[foundation->offense_side] <
            TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT) {
        snapshot_out->candidate_receiver_available = true;
        snapshot_out->candidate_receiver = foundation->candidate_actor_by_side[
            foundation->offense_side];
    } else {
        snapshot_out->candidate_receiver = TECMO_GAMEPLAY_SCENE_NO_ACTOR;
    }
    if (scene->ball_holder < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT &&
        tecmo_gameplay_court_coordinate_q8_floor(&scene->ball_position,
                                                 &snapshot_out->ball_position)) {
        snapshot_out->ball_position_available = true;
    }
    (void)cpu_playbook_lab_formation_buckets(
        scene, &snapshot_out->formation_x_bucket,
        &snapshot_out->formation_depth_bucket);
    /* `$938B-$9620` writes formation stream offsets. The stored index is
       shown only when the typed LIVE foundation validates against TGAI-2. */
    if (tecmo_gameplay_live_foundation_valid(&scene->cpu_steering_assets,
                                             foundation)) {
        snapshot_out->formation_available = true;
        snapshot_out->formation_index = foundation->formation_index;
    }
    /* No general Bank04 object lifecycle is retained. Slot 10's coordinate is
       the deliberate bounded opcode-4 C8 exception, not an eleventh actor. */
    snapshot_out->ball_object_state_available = false;
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        TecmoGameplayCpuPlaybookLabActorSnapshot *view =
            &snapshot_out->actor[actor];
        const TecmoGameplaySceneCpuActor *cpu = &scene->cpu_actors[actor];
        view->actor = (uint8_t)actor;
        view->active = scene->actors[actor].active;
        view->team = scene->actors[actor].team;
        view->controlled = cpu_playbook_lab_actor_controlled(scene,
                                                               (uint8_t)actor);
        view->stream_offset = foundation->play_state.stream_offset[actor];
        view->wait_counter = foundation->play_state.wait_counter[actor];
        view->actor_state = foundation->play_state.actor_state[actor];
        view->timer = foundation->play_state.timer[actor];
        view->source_target_object =
            foundation->play_state.target_object[actor];
        view->source_target.x = foundation->play_state.target_x[actor];
        view->source_target.y = foundation->play_state.target_depth[actor];
        view->source_target_valid = foundation->source_target_valid[actor];
        view->source_direction = foundation->source_direction[actor];
        view->source_direction_valid = foundation->source_direction_valid[actor];
        view->scene_target = cpu->target_position;
        view->scene_target_kind = cpu->target_kind;
        view->scene_direction = cpu->direction;
        view->scene_target_valid = cpu->target_valid;
        view->scene_writes_direction = cpu->writes_direction;
        view->source_deferred = foundation->deferred[actor];
        view->slice_skip_reason = cpu_playbook_lab_slice_skip_reason(
            scene, (uint8_t)actor);
        /* play_step retains the reason selected at Bank06 $8B90-$9237's
           bounded C conversion. It still does not retain or recreate the
           missing caller-owned workspace itself. */
        view->source_defer_detail_available =
            view->source_deferred &&
            foundation->deferred_reason[actor] >
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE &&
            foundation->deferred_reason[actor] <
                TECMO_GAMEPLAY_CPU_STEERING_DEFER_REASON_COUNT;
        view->retained_source_defer_reason =
            view->source_defer_detail_available
                ? foundation->deferred_reason[actor]
                : TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE;
        view->movement_evidence_available =
            !lab->showing_baseline &&
            lab->last_step_status ==
                TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_ACCEPTED;
        if (view->movement_evidence_available) {
            view->last_step_delta_x = lab->last_step_delta_x[actor];
            view->last_step_delta_y = lab->last_step_delta_y[actor];
            view->moved_last_step = lab->moved_last_step[actor];
            view->boundary_latched_before =
                lab->boundary_latched_before[actor];
            view->boundary_latched_after =
                lab->boundary_latched_after[actor];
        }
        /* `$C006->$CBE0` fetches an aligned Bank04 record. Decoder failure is
           surfaced as unavailable instead of decoding bytes from a RAM mirror. */
        view->command_available = tecmo_gameplay_cpu_steering_decode_command(
            &scene->cpu_steering_assets, view->stream_offset,
            &view->command);
    }
    return true;
}

const char *tecmo_gameplay_cpu_playbook_lab_defer_reason_name(
    TecmoGameplayCpuPlaybookLabDeferReason reason)
{
    switch (reason) {
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_NONE:
        return "no-defer-retained";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_CONTROLLED_ACTOR:
        return "controlled-actor-cpu-slice-skipped";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_SELECTED_DEFENDER_STREAM_EXCLUDED:
        return "selected-defender-ordinary-stream-excluded";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_REASON_COUNT:
    default:
        return "unavailable";
    }
}

const char *tecmo_gameplay_cpu_playbook_lab_step_status_name(
    TecmoGameplayCpuPlaybookLabStepStatus status)
{
    switch (status) {
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_NONE:
        return "not-stepped";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_ACCEPTED:
        return "cpu-slice-accepted";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SCENE_REJECTED:
        return "cpu-slice-rejected";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SHOT_HANDOFF_REJECTED:
        return "shot-handoff-out-of-scope";
    case TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_STATUS_COUNT:
    default:
        return "unavailable";
    }
}

static bool cpu_playbook_lab_json_append(char **cursor, size_t *remaining,
                                         const char *format, ...)
{
    va_list arguments;
    int written;
    if (cursor == NULL || *cursor == NULL || remaining == NULL ||
        *remaining == 0U || format == NULL) {
        return false;
    }
    va_start(arguments, format);
    written = vsnprintf(*cursor, *remaining, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= *remaining) return false;
    *cursor += written;
    *remaining -= (size_t)written;
    return true;
}

bool tecmo_gameplay_cpu_playbook_lab_write_json(
    const TecmoGameplayCpuPlaybookLab *lab,
    char *buffer,
    size_t buffer_size)
{
    TecmoGameplayCpuPlaybookLabSnapshot snapshot;
    char *cursor = buffer;
    size_t remaining = buffer_size;
    size_t actor;

    if (buffer == NULL || buffer_size == 0U ||
        !tecmo_gameplay_cpu_playbook_lab_snapshot(lab, &snapshot)) {
        return false;
    }
    buffer[0] = '\0';
    if (!cpu_playbook_lab_json_append(
            &cursor, &remaining,
            "{\"schema\":\"%s\",\"source\":{\"formation\":\"Bank06 $938B-$9620\",\"dispatch\":\"Bank06 $8B90-$9237\",\"trampoline\":\"fixed $C006->$CBE0\",\"corpus\":\"Bank04 $9F2E-$AC75\"},\"entry\":{\"direct_fixture_input\":%s,\"organic_live_entry\":%s,\"classification\":\"%s\"},\"preview\":{\"tick\":%u,\"paused\":%s,\"showing_baseline\":%s,\"last_step\":\"%s\"},\"formation\":{\"available\":%s,\"index\":%u,\"x_bucket\":%u,\"depth_bucket\":%u},\"selection\":{\"holder\":%u,\"primary\":%u,\"defender\":%u,\"candidate_receiver\":%u,\"candidate_receiver_available\":%s,\"candidate_receiver_is_pass\":false},\"ball\":{\"object_slot\":%u,\"typed_position_available\":%s,\"x\":%d,\"depth\":%d,\"object_state_available\":false},\"actors\":[",
            TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_SCHEMA,
            snapshot.direct_fixture_input ? "true" : "false",
            snapshot.organic_live_entry ? "true" : "false",
            snapshot.direct_fixture_input ? "direct-fixture" :
                (snapshot.organic_live_entry ? "organic-live-entry" :
                 "unclassified"),
            (unsigned)snapshot.preview_tick,
            snapshot.paused ? "true" : "false",
            snapshot.showing_baseline ? "true" : "false",
            tecmo_gameplay_cpu_playbook_lab_step_status_name(
                snapshot.last_step_status),
            snapshot.formation_available ? "true" : "false",
            (unsigned)snapshot.formation_index,
            (unsigned)snapshot.formation_x_bucket,
            (unsigned)snapshot.formation_depth_bucket,
            (unsigned)snapshot.ball_holder,
            (unsigned)snapshot.primary_actor,
            (unsigned)snapshot.defender_actor,
            (unsigned)snapshot.candidate_receiver,
            snapshot.candidate_receiver_available ? "true" : "false",
            (unsigned)snapshot.ball_object_slot,
            snapshot.ball_position_available ? "true" : "false",
            (int)snapshot.ball_position.x, (int)snapshot.ball_position.y)) {
        buffer[0] = '\0';
        return false;
    }
    for (actor = 0U; actor < TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT;
         ++actor) {
        const TecmoGameplayCpuPlaybookLabActorSnapshot *view =
            &snapshot.actor[actor];
        const TecmoGameplayCpuSteeringCommand *command = &view->command;
        if (!cpu_playbook_lab_json_append(
                &cursor, &remaining,
                "%s{\"slot\":%u,\"team\":%u,\"active\":%s,\"controlled\":%s,\"stream_offset\":\"%04X\",\"command\":{\"available\":%s,\"opcode\":%u,\"args\":[%u,%u,%u,%u],\"handler\":\"%04X\",\"effect\":\"%s\"},\"wait\":%u,\"state\":%u,\"timer\":%u,\"source_target\":{\"valid\":%s,\"object\":%u,\"x\":%d,\"depth\":%d},\"source_direction\":{\"valid\":%s,\"value\":%u},\"scene_target\":{\"valid\":%s,\"kind\":\"%s\",\"kind_value\":%u,\"x\":%d,\"depth\":%d,\"direction\":%u,\"writes_direction\":%s},\"defer\":{\"retained\":%s,\"slice_skip_reason\":\"%s\",\"typed_reason\":\"%s\",\"typed_detail_available\":%s,\"executed_record_retained\":false},\"movement\":{\"available\":%s,\"delta_x\":%d,\"delta_depth\":%d,\"moved\":%s,\"boundary_latch_before\":%s,\"boundary_latch_after\":%s,\"clamp_event_retained\":false}}",
                actor == 0U ? "" : ",", (unsigned)view->actor,
                (unsigned)view->team, view->active ? "true" : "false",
                view->controlled ? "true" : "false",
                (unsigned)view->stream_offset,
                view->command_available ? "true" : "false",
                (unsigned)(view->command_available ? command->opcode : 0U),
                (unsigned)(view->command_available ? command->arguments[0] : 0U),
                (unsigned)(view->command_available ? command->arguments[1] : 0U),
                (unsigned)(view->command_available ? command->arguments[2] : 0U),
                (unsigned)(view->command_available ? command->arguments[3] : 0U),
                (unsigned)(view->command_available ? command->handler_cpu : 0U),
                view->command_available
                    ? tecmo_gameplay_cpu_steering_effect_name(command->effect)
                    : "unavailable",
                (unsigned)view->wait_counter, (unsigned)view->actor_state,
                (unsigned)view->timer,
                view->source_target_valid ? "true" : "false",
                (unsigned)view->source_target_object,
                (int)view->source_target.x, (int)view->source_target.y,
                view->source_direction_valid ? "true" : "false",
                (unsigned)view->source_direction,
                view->scene_target_valid ? "true" : "false",
                view->scene_target_valid
                    ? tecmo_gameplay_cpu_steering_harness_target_kind_name(
                          (TecmoGameplayCpuSteeringHarnessTargetKind)
                              view->scene_target_kind)
                    : "unavailable",
                (unsigned)view->scene_target_kind,
                (int)view->scene_target.x, (int)view->scene_target.y,
                (unsigned)view->scene_direction,
                view->scene_writes_direction ? "true" : "false",
                view->source_deferred ? "true" : "false",
                tecmo_gameplay_cpu_playbook_lab_defer_reason_name(
                    view->slice_skip_reason),
                tecmo_gameplay_cpu_steering_deferred_reason_name(
                    view->retained_source_defer_reason),
                view->source_defer_detail_available ? "true" : "false",
                view->movement_evidence_available ? "true" : "false",
                (int)view->last_step_delta_x, (int)view->last_step_delta_y,
                view->moved_last_step ? "true" : "false",
                view->boundary_latched_before ? "true" : "false",
                view->boundary_latched_after ? "true" : "false")) {
            buffer[0] = '\0';
            return false;
        }
    }
    if (!cpu_playbook_lab_json_append(&cursor, &remaining, "]}")) {
        buffer[0] = '\0';
        return false;
    }
    return true;
}

static void cpu_playbook_lab_test_press(TecmoControlFrame *controls,
                                        TecmoControlButton button)
{
    memset(controls, 0, sizeof(*controls));
    tecmo_input_set_button(&controls->pressed, button, true);
}

bool tecmo_gameplay_cpu_playbook_lab_self_test(char *message,
                                               size_t message_size)
{
    TecmoGameplayScene scene;
    TecmoGameplayScene before;
    TecmoGameplayCpuPlaybookLab lab;
    TecmoGameplayCpuPlaybookLabSnapshot snapshot;
    TecmoControlFrame controls;
    char json[TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_JSON_CAPACITY];

    if (message == NULL || message_size == 0U) return false;
    memset(&scene, 0, sizeof(scene));
    scene.available = true;
    scene.active = true;
    scene.state.initialized = true;
    scene.state.phase = TECMO_GAMEPLAY_PHASE_LIVE;
    scene.state.banner = TECMO_GAMEPLAY_BANNER_NONE;
    scene.shot_kind = TECMO_GAMEPLAY_SCENE_SHOT_NONE;
    scene.ball_holder = 0U;
    scene.cpu_steering_assets.available = true;
    scene.movement_assets.available = true;
    /* Keep the typed ball far from the selected $0308-equivalent actor so
       the bucket regression proves the source selector does not use ball X/Y. */
    scene.live_foundation.primary_actor = 1U;
    scene.actors[1U].position.x = 384;
    scene.actors[1U].position.y = 128;
    before = scene;
    tecmo_gameplay_cpu_playbook_lab_init(&lab);
    if (!tecmo_gameplay_cpu_playbook_lab_open(&lab, &scene) || !lab.active ||
        !lab.paused || lab.baseline_scene == NULL || lab.preview_scene == NULL ||
        memcmp(&scene, &before, sizeof(scene)) != 0 ||
        !tecmo_gameplay_cpu_playbook_lab_snapshot(&lab, &snapshot) ||
        snapshot.actor[0U].command_available ||
        snapshot.direct_fixture_input || snapshot.organic_live_entry ||
        snapshot.formation_x_bucket != 6U ||
        snapshot.formation_depth_bucket != 2U ||
        snapshot.ball_object_state_available ||
        !tecmo_gameplay_cpu_playbook_lab_write_json(&lab, json,
                                                     sizeof(json)) ||
        strstr(json, "\"schema\":\"" TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_SCHEMA
                     "\"") == NULL ||
        strstr(json, "\"executed_record_retained\":false") == NULL ||
        strstr(json, "\"typed_detail_available\":false") == NULL ||
        strstr(json, "\"classification\":\"unclassified\"") == NULL ||
        strstr(json, "\"clamp_event_retained\":false") == NULL) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB OPEN/JSON FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    cpu_playbook_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_NEXT);
    if (!tecmo_gameplay_cpu_playbook_lab_update(&lab, &controls) ||
        lab.selected_actor != 1U) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB ACTOR SELECT FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    if (tecmo_gameplay_cpu_playbook_lab_set_entry_provenance(&lab, true,
                                                             true) ||
        !tecmo_gameplay_cpu_playbook_lab_set_entry_provenance(&lab, true,
                                                              false) ||
        !tecmo_gameplay_cpu_playbook_lab_snapshot(&lab, &snapshot) ||
        !snapshot.direct_fixture_input || snapshot.organic_live_entry) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB ENTRY PROVENANCE FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    /* Make preview-only cursor/evidence differ from the frozen baseline.
       F7 must expose baseline values, not the hidden preview's values. */
    lab.preview_scene->live_foundation.play_state.stream_offset[0U] = 5U;
    lab.preview_scene->live_foundation.deferred[0U] = true;
    lab.preview_scene->live_foundation.deferred_reason[0U] =
        TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA;
    lab.last_step_status = TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_ACCEPTED;
    lab.last_step_delta_x[0U] = 7;
    lab.moved_last_step[0U] = true;
    if (!tecmo_gameplay_cpu_playbook_lab_snapshot(&lab, &snapshot) ||
        snapshot.actor[0U].stream_offset != 5U ||
        !snapshot.actor[0U].movement_evidence_available ||
        snapshot.actor[0U].last_step_delta_x != 7 ||
        !snapshot.actor[0U].source_defer_detail_available ||
        snapshot.actor[0U].retained_source_defer_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_MISSING_COMMON_TAIL_BA ||
        !tecmo_gameplay_cpu_playbook_lab_write_json(&lab, json,
                                                     sizeof(json)) ||
        strstr(json, "\"typed_reason\":\"missing-ba-lifecycle\"") ==
            NULL) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB PREVIEW EVIDENCE FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    cpu_playbook_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PATH);
    if (!tecmo_gameplay_cpu_playbook_lab_update(&lab, &controls) ||
        !lab.showing_baseline ||
        tecmo_gameplay_cpu_playbook_lab_render_scene(&lab) !=
            lab.baseline_scene ||
        !tecmo_gameplay_cpu_playbook_lab_snapshot(&lab, &snapshot) ||
        snapshot.actor[0U].stream_offset != 0U ||
        snapshot.actor[0U].source_defer_detail_available ||
        snapshot.actor[0U].retained_source_defer_reason !=
            TECMO_GAMEPLAY_CPU_STEERING_DEFER_NONE ||
        snapshot.actor[0U].movement_evidence_available ||
        snapshot.actor[0U].last_step_delta_x != 0) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB F7 RENDER/SNAPSHOT FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    cpu_playbook_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_STEP);
    if (!tecmo_gameplay_cpu_playbook_lab_update(&lab, &controls) ||
        !lab.paused || lab.last_step_status !=
            TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SCENE_REJECTED ||
        memcmp(&scene, &before, sizeof(scene)) != 0) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB REJECT TRANSACTION FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    cpu_playbook_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_RESTART);
    if (!tecmo_gameplay_cpu_playbook_lab_update(&lab, &controls) ||
        lab.preview_tick != 0U || lab.last_step_status !=
            TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_NONE ||
        memcmp(lab.preview_scene, lab.baseline_scene, sizeof(scene)) != 0) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB RESTART FAILED");
        tecmo_gameplay_cpu_playbook_lab_close(&lab);
        return false;
    }
    tecmo_gameplay_cpu_playbook_lab_close(&lab);
    if (lab.active || lab.baseline_scene != NULL || lab.preview_scene != NULL ||
        memcmp(&scene, &before, sizeof(scene)) != 0) {
        (void)snprintf(message, message_size,
                       "CPU PLAYBOOK LAB CLOSE TRANSACTION FAILED");
        return false;
    }
    (void)snprintf(message, message_size,
                   "CPU PLAYBOOK LAB SELF TEST PASS");
    return true;
}
