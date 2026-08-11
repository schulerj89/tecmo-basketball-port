#ifndef TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_H
#define TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_H

#include "tecmo_gameplay_scene.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_SCHEMA "tecmo.cpu-playbook-lab/TGPLAB-1"
#define TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_JSON_CAPACITY 16384U

/* This names why the current native CPU slice did not issue an ordinary
 * stream step for a slot.  It deliberately does not recreate unretained ROM
 * workspace branches or claim the currently decoded cursor was just run. */
typedef enum TecmoGameplayCpuPlaybookLabDeferReason {
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_NONE = 0,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_CONTROLLED_ACTOR,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_SELECTED_DEFENDER_STREAM_EXCLUDED,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_DEFER_REASON_COUNT
} TecmoGameplayCpuPlaybookLabDeferReason;

typedef enum TecmoGameplayCpuPlaybookLabStepStatus {
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_NONE = 0,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_ACCEPTED,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SCENE_REJECTED,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_SHOT_HANDOFF_REJECTED,
    TECMO_GAMEPLAY_CPU_PLAYBOOK_LAB_STEP_STATUS_COUNT
} TecmoGameplayCpuPlaybookLabStepStatus;

/* One read-only, typed-C view of a player stream.  `command` describes the
 * current aligned cursor only; Bank06's previously executed record is not
 * retained by the LIVE adapter. */
typedef struct TecmoGameplayCpuPlaybookLabActorSnapshot {
    uint8_t actor;
    uint8_t team;
    bool active;
    bool controlled;
    uint16_t stream_offset;
    bool command_available;
    TecmoGameplayCpuSteeringCommand command;
    uint8_t wait_counter;
    uint8_t actor_state;
    uint8_t timer;
    uint8_t source_target_object;
    TecmoGameplayCourtCoordinate source_target;
    bool source_target_valid;
    uint8_t source_direction;
    bool source_direction_valid;
    TecmoGameplayCourtCoordinate scene_target;
    uint8_t scene_target_kind;
    uint8_t scene_direction;
    bool scene_target_valid;
    bool scene_writes_direction;
    bool source_deferred;
    TecmoGameplayCpuPlaybookLabDeferReason slice_skip_reason;
    /* LIVE retains the typed reason produced by the bounded Bank06 handler.
       This is diagnostic ownership, not a reconstructed RAM workspace. */
    bool source_defer_detail_available;
    TecmoGameplayCpuSteeringDeferredReason retained_source_defer_reason;
    bool movement_evidence_available;
    int16_t last_step_delta_x;
    int16_t last_step_delta_y;
    bool moved_last_step;
    bool boundary_latched_before;
    bool boundary_latched_after;
} TecmoGameplayCpuPlaybookLabActorSnapshot;

/* Snapshot schema intentionally has no raw-RAM mirror.  Bank04 object slot
 * 10 is exposed only as the typed floored ball coordinate needed by the
 * bounded opcode-4 C8 target lookup; its general object state is unavailable. */
typedef struct TecmoGameplayCpuPlaybookLabSnapshot {
    uint32_t preview_tick;
    bool paused;
    bool showing_baseline;
    bool direct_fixture_input;
    bool organic_live_entry;
    TecmoGameplayCpuPlaybookLabStepStatus last_step_status;
    bool formation_available;
    uint8_t formation_index;
    uint8_t formation_x_bucket;
    uint8_t formation_depth_bucket;
    uint8_t ball_holder;
    uint8_t primary_actor;
    uint8_t defender_actor;
    uint8_t candidate_receiver;
    bool candidate_receiver_available;
    uint8_t ball_object_slot;
    TecmoGameplayCourtCoordinate ball_position;
    bool ball_position_available;
    bool ball_object_state_available;
    TecmoGameplayCpuPlaybookLabActorSnapshot actor[
        TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
} TecmoGameplayCpuPlaybookLabSnapshot;

/* Developer-only lab. The two heap snapshots are shallow typed-scene copies;
 * they never own or destroy shared asset storage. The ordinary runtime scene
 * is never written while this lab is active, so close discards the preview
 * instead of trying to replay an incomplete source state. */
typedef struct TecmoGameplayCpuPlaybookLab {
    bool active;
    bool paused;
    bool showing_baseline;
    bool direct_fixture_input;
    bool organic_live_entry;
    uint8_t selected_actor;
    uint32_t preview_tick;
    TecmoGameplayCpuPlaybookLabStepStatus last_step_status;
    int16_t last_step_delta_x[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    int16_t last_step_delta_y[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool moved_last_step[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool boundary_latched_before[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    bool boundary_latched_after[TECMO_GAMEPLAY_CPU_STEERING_ACTOR_COUNT];
    TecmoGameplayScene *baseline_scene;
    TecmoGameplayScene *preview_scene;
} TecmoGameplayCpuPlaybookLab;

void tecmo_gameplay_cpu_playbook_lab_init(TecmoGameplayCpuPlaybookLab *lab);

/* Opens only on an ordinary, initialized LIVE scene whose typed TGAI/TGMO
 * dependencies are present. It snapshots the scene before any lab preview.
 * `scene` remains byte-for-byte untouched by lab controls and close. */
bool tecmo_gameplay_cpu_playbook_lab_open(
    TecmoGameplayCpuPlaybookLab *lab,
    const TecmoGameplayScene *scene);
void tecmo_gameplay_cpu_playbook_lab_close(TecmoGameplayCpuPlaybookLab *lab);

/* F5/F6 select an actor, F7 flips frozen baseline/preview drawing, F8
 * pauses/plays preview slices, F9 restores the baseline preview, and F10
 * executes exactly one native C CPU slice against the private preview. */
bool tecmo_gameplay_cpu_playbook_lab_update(
    TecmoGameplayCpuPlaybookLab *lab,
    const TecmoControlFrame *controls);

/* The normal F4 path leaves entry provenance unclassified. Deterministic CLI
 * fixtures may set their direct setup explicitly; callers must never mark an
 * injected fixture as an organic PRETIP->LIVE observation. */
bool tecmo_gameplay_cpu_playbook_lab_set_entry_provenance(
    TecmoGameplayCpuPlaybookLab *lab,
    bool direct_fixture_input,
    bool organic_live_entry);

const TecmoGameplayScene *tecmo_gameplay_cpu_playbook_lab_render_scene(
    const TecmoGameplayCpuPlaybookLab *lab);
bool tecmo_gameplay_cpu_playbook_lab_snapshot(
    const TecmoGameplayCpuPlaybookLab *lab,
    TecmoGameplayCpuPlaybookLabSnapshot *snapshot_out);
bool tecmo_gameplay_cpu_playbook_lab_write_json(
    const TecmoGameplayCpuPlaybookLab *lab,
    char *buffer,
    size_t buffer_size);
const char *tecmo_gameplay_cpu_playbook_lab_defer_reason_name(
    TecmoGameplayCpuPlaybookLabDeferReason reason);
const char *tecmo_gameplay_cpu_playbook_lab_step_status_name(
    TecmoGameplayCpuPlaybookLabStepStatus status);

/* Isolated ownership/controls/JSON coverage. Asset provenance and command
 * decode are exercised by the canonical TGAI-2 suite and render checkpoint. */
bool tecmo_gameplay_cpu_playbook_lab_self_test(char *message,
                                               size_t message_size);

#endif
