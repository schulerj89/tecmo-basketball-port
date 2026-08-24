#ifndef TECMO_GAMEPLAY_SCENE_H
#define TECMO_GAMEPLAY_SCENE_H

#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_actor_command_assignment.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_ball_dribble.h"
#include "tecmo_gameplay_backcourt.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_cpu_a0f3_launch.h"
#include "tecmo_gameplay_cpu_a8e9_velocity.h"
#include "tecmo_gameplay_fixed_rng.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_dunk_cutaway.h"
#include "tecmo_gameplay_fatigue.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_hud.h"
#include "tecmo_gameplay_jump_shots.h"
#include "tecmo_gameplay_live_foundation.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_gameplay_pretip.h"
#include "tecmo_gameplay_shot_resolution.h"
#include "tecmo_gameplay_rebound_audit.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_post_arena.h"
#include "tecmo_music.h"
#include "tecmo_player_stats.h"
#include "tecmo_team_data.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SCENE_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT 5U
#define TECMO_GAMEPLAY_SCENE_NO_ACTOR 0xFFU
#define TECMO_GAMEPLAY_SCENE_NO_TEAM 0xFFU
#define TECMO_GAMEPLAY_SCENE_TEAM_LIMIT 27U
#define TECMO_GAMEPLAY_SCENE_NES_WIDTH 256
#define TECMO_GAMEPLAY_SCENE_NES_HEIGHT 240
#define TECMO_GAMEPLAY_SCENE_COURT_COORDINATES_TAG 0x43434754U
#define TECMO_GAMEPLAY_SCENE_COURT_PROJECTION_TAG 0x50534754U
#define TECMO_GAMEPLAY_SCENE_COURT_SLICE_TAG 0x4C534754U
#define TECMO_GAMEPLAY_SCENE_COURT_FRAME_TAG 0x46534754U
#define TECMO_GAMEPLAY_SCENE_CPU_ACTOR_TAG 0x41434754U
#define TECMO_GAMEPLAY_SCENE_POSSESSION_TRACE_TAG 0x50544754U
#define TECMO_GAMEPLAY_SCENE_CLAIMANT_TRACE_TAG 0x43544754U
#define TECMO_GAMEPLAY_SCENE_CPU_NO_COMMAND_OFFSET 0xFFFFU
#define TECMO_GAMEPLAY_SCENE_OPCODE10_FRAME_CONTEXT_TAG 0x4630314FU
#define TECMO_GAMEPLAY_SCENE_OPCODE16_FRAME_CONTEXT_TAG 0x4636314FU
#define TECMO_GAMEPLAY_SCENE_A023_LATCH_FRAME_CONTEXT_TAG 0x464C4341U

/* The slot-3 trace spans 125 inclusive updates from CPU state-18 entry through
   launch. Native play uses that observed schedule until the original CPU
   positioning/script system is ported. Human attempts have no timer fallback. */
#define TECMO_GAMEPLAY_FREE_THROW_CPU_OBSERVED_LAUNCH_UPDATES 125U

typedef enum TecmoGameplaySceneSource {
    TECMO_GAMEPLAY_SCENE_PRESEASON = 0,
    TECMO_GAMEPLAY_SCENE_SEASON,
    TECMO_GAMEPLAY_SCENE_SOURCE_COUNT
} TecmoGameplaySceneSource;

typedef enum TecmoGameplaySceneShotKind {
    TECMO_GAMEPLAY_SCENE_SHOT_NONE = 0,
    TECMO_GAMEPLAY_SCENE_SHOT_JUMP,
    /* High-level meanings; scene-to-TGCS mapping preserves numeric provenance. */
    TECMO_GAMEPLAY_SCENE_SHOT_DUNK,
    TECMO_GAMEPLAY_SCENE_SHOT_LAYUP,
    /* Numeric TGCS identity 1.  It has a bounded native scene schedule but
       no inferred dunk/layup/contact semantic label. */
    TECMO_GAMEPLAY_SCENE_SHOT_NUMERIC_1,
    TECMO_GAMEPLAY_SCENE_SHOT_KIND_COUNT
} TecmoGameplaySceneShotKind;

/* Bank05 $89D7 starts a pass in a passer-owned gather state, state $0F
   dispatches through $8695, $8999/$9C29 advances the captured packed-byte
   cadence, $86A8 releases only when the complete byte reaches $04, $B074
   locks the receiver, and genuine Bank05 $B24F transfers ownership at the
   catch. The current flight interpolation remains a native adapter pending
   strict $B42F/$BB9F/$BBA0 and $B1E7/$B500 assets. */
typedef enum TecmoGameplayScenePassPhase {
    TECMO_GAMEPLAY_SCENE_PASS_NONE = 0,
    TECMO_GAMEPLAY_SCENE_PASS_GATHER,
    TECMO_GAMEPLAY_SCENE_PASS_FLIGHT,
    TECMO_GAMEPLAY_SCENE_PASS_PHASE_COUNT
} TecmoGameplayScenePassPhase;

typedef struct TecmoGameplayScenePassState {
    TecmoGameplayScenePassPhase phase;
    uint8_t passer;
    uint8_t receiver;
    /* A human pad index, or NO_ACTOR for an autonomous CPU transport. */
    uint8_t controller;
    /* Capture-bounded $32->$22->$12->$02->$03 cadence; $04 releases. */
    uint8_t packed_animation_state;
    /* Bank05 $B074-$B0FD locks $037F[$030A] and swaps the source-side
       selected/candidate roles at launch; $0308 remains the passer until
       the genuine Bank05 $B24F catch. */
    bool receiver_locked;
    uint8_t reserved[2];
    uint16_t flight_frame;
    uint16_t flight_duration;
    TecmoGameplayCourtCoordinateQ8 start_position;
    TecmoGameplayCourtCoordinateQ8 target_position;
} TecmoGameplayScenePassState;

/*
 * Source-bounded restart transport.  Bank07 $EC9B-$ECD8 reaches Bank06
 * $9621-$9764 for the visible setup; Bank05's shared pass path supplies the
 * ordered gather/release/catch shape.  This is intentionally not a generic
 * player pass: primary ($0308), selected defender ($0309), and launch
 * candidate ($037F[$030A]) remain separate typed roles.
 */
typedef enum TecmoGameplaySceneInboundPhase {
    TECMO_GAMEPLAY_SCENE_INBOUND_NONE = 0,
    TECMO_GAMEPLAY_SCENE_INBOUND_SETUP,
    TECMO_GAMEPLAY_SCENE_INBOUND_GATHER,
    TECMO_GAMEPLAY_SCENE_INBOUND_FLIGHT,
    TECMO_GAMEPLAY_SCENE_INBOUND_PHASE_COUNT
} TecmoGameplaySceneInboundPhase;

typedef struct TecmoGameplaySceneInboundState {
    TecmoGameplaySceneInboundPhase phase;
    uint8_t passer;
    uint8_t receiver;
    uint8_t defender;
    uint8_t restart_team;
    /* Shared Bank05 release shape: $32, $22, $12, then full-byte $04. */
    uint8_t packed_animation_state;
    uint8_t reserved[2];
    uint16_t flight_frame;
    uint16_t flight_duration;
    TecmoGameplayCourtCoordinateQ8 start_position;
    TecmoGameplayCourtCoordinateQ8 target_position;
} TecmoGameplaySceneInboundState;

/* Retained grounded-ball ownership between a missed-shot terminal and the
   source-backed claimant transaction. In the ordinary admitted context,
   Bank05 slot-10 state $10 retains/retries object and claimant work; the
   native scene owns the exact claimant envelope but not the complete object
   scheduler or its earlier cancellation gates. */
typedef struct TecmoGameplaySceneLooseBallState {
    bool active;
    uint8_t shooting_team;
    /* Deterministic class-3 locomotion adapter actor, never a claimant until
       the independent source-backed eligibility scan accepts it. */
    uint8_t chase_actor;
    uint8_t reserved;
} TecmoGameplaySceneLooseBallState;

typedef struct TecmoGameplaySceneLaunch {
    TecmoGameplaySceneSource source;
    uint16_t game_index;
    uint8_t away_team;
    uint8_t home_team;
    uint8_t regulation_minutes;
    uint8_t difficulty;
    uint8_t control_mode;
    uint8_t speed_value;
    uint8_t controller_team[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    bool game_music_enabled;
    /* Source/default-initializer compatibility binding. False preserves
       direct legacy/test/render
       callers and normalizes both sides to the deterministic identity
       lineup 0..4. Production launchers always set this true after copying
       and validating Team Management session starters by value. The stored
       launch is always canonicalized to bound=true; direct-call origin is
       retained only in the scene-owned compatibility bit below. */
    bool starter_binding_bound;
    uint8_t starter_roster_index[TECMO_GAMEPLAY_TEAM_COUNT]
                                [TECMO_GAMEPLAY_SCENE_TEAM_ACTOR_COUNT];
} TecmoGameplaySceneLaunch;

typedef struct TecmoGameplaySceneResult {
    TecmoGameplaySceneSource source;
    uint16_t game_index;
    uint8_t away_team;
    uint8_t home_team;
    uint16_t away_score;
    uint16_t home_score;
    uint8_t overtime_count;
    TecmoPlayerStatsGameLedger player_stats;
} TecmoGameplaySceneResult;

typedef struct TecmoGameplaySceneActor {
    TecmoGameplayCourtCoordinate position;
    TecmoGameplayCourtCoordinate anchor;
    uint16_t pose_index;
    uint8_t sprite_slot_base;
    uint8_t team;
    uint8_t roster_index;
    uint8_t movement_action_state;
    uint8_t movement_direction;
    uint8_t movement_fractional_accumulator;
    uint8_t movement_animation_phase;
    uint8_t condition;
    bool facing_right;
    bool pose_orientation_encoded;
    bool movement_boundary_latched;
    bool active;
} TecmoGameplaySceneActor;

/* Scene-owned state at the bounded native TGAI -> TGMO integration seam.
   command_offset remains the explicit no-command sentinel because the ROM
   play-stream lifecycle is not reconstructed; linked_actor, target fields,
   and decision_serial transactionally own the policy that replaces the old
   anchor chase. */
typedef struct TecmoGameplaySceneCpuActor {
    uint32_t contract_tag;
    uint32_t decision_serial;
    uint32_t snapshot_fingerprint;
    TecmoGameplayCourtCoordinate target_position;
    uint16_t command_offset;
    uint8_t linked_actor;
    uint8_t target_kind;
    uint8_t direction;
    uint8_t held_direction_bits;
    bool command_advance_pending;
    bool target_valid;
    bool writes_direction;
} TecmoGameplaySceneCpuActor;

/* Transactional public snapshot of every live object in one full-court
   coordinate plane. Player and hoop anchors are integer pixels; the ball
   retains Q8 precision in that same plane. */
typedef struct TecmoGameplaySceneCourtCoordinates {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate
        players[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayCourtCoordinateQ8 ball;
    TecmoGameplayCourtCoordinate
        hoops[TECMO_GAMEPLAY_COURT_ORIENTATION_COUNT];
} TecmoGameplaySceneCourtCoordinates;

/* One transactional TGCP projection of the canonical scene coordinates.
   Offscreen entries retain TGCP's neutral visible=false, X/Y-zero sentinel. */
typedef struct TecmoGameplaySceneCourtProjection {
    uint32_t contract_tag;
    uint16_t camera_x;
    uint16_t reserved;
    TecmoGameplayActorProjection
        players[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayActorProjection ball;
} TecmoGameplaySceneCourtProjection;

/* One transactional possession-aware TGCT view. The viewport camera must
   match the TGCP projection camera used for the same rendered frame. */
typedef struct TecmoGameplaySceneCourtSlice {
    uint32_t contract_tag;
    uint32_t transition_serial;
    uint8_t possession;
    uint8_t direction;
    uint16_t reserved;
    TecmoGameplayCourtViewport viewport;
} TecmoGameplaySceneCourtSlice;

/* One camera-coherent live frame. Scene rendering consumes this combined
   snapshot so the TGCT slice and every TGCP projection share one camera. */
typedef struct TecmoGameplaySceneCourtFrame {
    uint32_t contract_tag;
    uint32_t scene_frame;
    uint32_t camera_follow_count;
    uint32_t reserved;
    TecmoGameplaySceneCourtSlice slice;
    TecmoGameplaySceneCourtProjection projection;
} TecmoGameplaySceneCourtFrame;

/* A scene-owned snapshot of the one currently supported live-foul route.
   It is deliberately separate from TecmoGameplayFoulRequest/state: TPNL
   adjudicates, state transitions/counters, and this record only retains the
   already accepted presentation identity used by Bank02's overlay writer.
   No raw $07E3/$0478/$05A8 route values are carried past the bounded bridge. */
typedef struct TecmoGameplaySceneFoulPresentation {
    bool valid;
    TecmoGameplayTeam fouling_team;
    uint8_t actor_index;
    uint8_t roster_index;
    TecmoGameplayFoulClass foul_class;
    uint8_t individual_foul_delta;
    uint8_t team_foul_delta;
    uint8_t individual_fouls_after;
    uint8_t team_fouls_after;
    uint8_t free_throw_attempts;
    bool team_in_bonus;
    bool fouled_out;
} TecmoGameplaySceneFoulPresentation;

/* Opt-in developer snapshot of the typed LIVE ownership seam. The raw names
 * identify source-shaped fields rather than exposing ROM bytes. It is a
 * passive snapshot: normal play neither renders nor depends on it. */
typedef struct TecmoGameplayScenePossessionTraceSnapshot {
    uint32_t contract_tag;
    uint32_t sync_serial;
    uint8_t raw_0308_primary_actor;
    uint8_t raw_0309_defender_actor;
    uint8_t raw_030a_offense_side;
    uint8_t raw_030b_defense_side;
    /* Compatibility/proof-schema name only. Runtime captures show $01 in
       both user and CPU contexts; this stores the typed native controller
       projection and must not be interpreted as a raw RAM encoding. */
    uint8_t raw_030c_030d_control_mode[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t raw_000e_000f_selected_actor[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t raw_037f_0380_candidate_actor[TECMO_GAMEPLAY_TEAM_COUNT];
    uint8_t raw_04b0_selector_flags[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t raw_06cb_dynamic_link[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint16_t raw_0547_0551_stream_offset[
        TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    uint8_t raw_057c_actor_state[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveOpcode15Trace opcode15_trace;
    uint8_t semantic_scene_possession;
    uint8_t semantic_ball_holder;
    uint8_t semantic_live_last_possession;
    uint8_t semantic_live_last_ball_holder;
    bool semantic_live_synchronized;
} TecmoGameplayScenePossessionTraceSnapshot;

/* The one source-shaped claimant transition recorded by the LIVE scene. This
 * is populated only by the Bank05 $B87C-$B98A bridge; generic, make, restart,
 * and tip handoffs leave its serial unchanged. */
typedef struct TecmoGameplaySceneClaimantSettlementTrace {
    uint32_t contract_tag;
    uint32_t event_serial;
    bool valid;
    TecmoGameplayLiveClaimantSettlement transaction;
    TecmoGameplayScenePossessionTraceSnapshot before;
    TecmoGameplayScenePossessionTraceSnapshot after;
} TecmoGameplaySceneClaimantSettlementTrace;

/* Runtime-owned fixed cadence bound once per scene frame. This is a typed
   execution context, not a raw-RAM mirror and not the TPTI RNG bridge. */
typedef struct TecmoGameplaySceneOpcode10FrameContext {
    uint32_t contract_tag;
    bool available;
    uint8_t sample_6a;
    uint8_t timer_0798;
    uint8_t rate_index_075f;
    uint8_t timer_bias_0760;
} TecmoGameplaySceneOpcode10FrameContext;

/* Bank05's once-per-gameplay-loop primary-position snapshot for opcode 16.
   The captured source coordinate makes this typed pre-motion evidence, not a
   persistent RAM mirror or a per-actor recomputation. */
typedef struct TecmoGameplaySceneOpcode16FrameContext {
    uint32_t contract_tag;
    bool available;
    uint8_t primary_actor_0308;
    uint8_t orientation_035a;
    TecmoGameplayCourtCoordinate primary_position;
    uint16_t workspace_036e;
    uint16_t workspace_0370;
} TecmoGameplaySceneOpcode16FrameContext;

/* Testable attachment seam for an exact, already-proven B721/B783 event.
   Production does not bind this until the upstream raw A214 gates/object
   scheduler gain typed owners. The context is consumed by one following
   Bank06 traversal and never persists to a later scene frame. */
typedef struct TecmoGameplaySceneA023LatchFrameContext {
    uint32_t contract_tag;
    TecmoGameplayActorCommandAssignmentSameFrameLatch latch;
    bool available;
} TecmoGameplaySceneA023LatchFrameContext;

typedef struct TecmoGameplayScene {
    uint32_t lifecycle_tag;
    bool available;
    bool active;
    bool result_ready;
    char status[192];
    char asset_pack_path[1024];

    TecmoGameplayAssets assets;
    TecmoGameplayCourt court;
    TecmoGameplayCourtWorld court_world;
    TecmoGameplayCameraAssets camera_assets;
    TecmoGameplayMovementAssets movement_assets;
    TecmoGameplayBallDribbleAssets ball_dribble_assets;
    TecmoGameplayCpuSteeringAssets cpu_steering_assets;
    TecmoGameplayCpuA0f3Assets cpu_a0f3_assets;
    TecmoGameplayPenaltyAssets penalty_assets;
    TecmoGameplayViolationRefereeAssets violation_referee_assets;
    TecmoGameplayBackcourtAssets backcourt_assets;
    TecmoGameplayBackcourtState backcourt_state;
    TecmoGameplayFatigueAssets fatigue_assets;
    TecmoGameplayFatigueState fatigue_state;
    TecmoGameplayCameraState camera_state;
    TecmoGameplayCourtOrientationAssets court_orientation;
    TecmoGameplayCourtOrientationState orientation_state;
    TecmoGameplayFreeThrowLineupAssets free_throw_lineup_assets;
    TecmoGameplayHudAssets hud_assets;
    TecmoGameplayCloseShotAssets close_shots;
    TecmoGameplayDunkCutawayAssets dunk_cutaway;
    TecmoGameplayJumpShotAssets jump_shots;
    TecmoGameplayShotResolutionAssets shot_resolution;
    /* TGRB-1 is passive provenance/eligibility diagnostics only; it does not
       own a rebound counter or change the player-stat coverage mask. */
    TecmoGameplayReboundAuditAssets rebound_audit;
    TecmoGameplayPreTipAssets pretip_assets;
    TecmoGameplayPreTipState pretip_state;
    /* Native LIVE checkpoint of fixed `$0053/$0054/$006A`. PRETIP supplies
       continuity only; it is not claimed to reproduce the canonical global
       stream before this accepted handoff. */
    TecmoGameplayFixedRng fixed_rng;
    uint8_t pretip_jumper_actor[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint8_t pretip_jumper_selector[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint16_t pretip_jumper_standing_pose[
        TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint16_t pretip_jumper_altitude_q8[
        TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    bool pretip_jump_active;
    TecmoIntroWarriorsAsset *pretip_closeup;
    TecmoTeamDataAsset *pretip_team_data;
    TecmoGameplayAudioAsset audio_asset;
    TecmoGameplayAudioPlayer audio_player;
    TecmoGameplayState state;
    TecmoGameplayEventBuffer events;
    TecmoGameplaySceneLaunch launch;
    /* Internal origin bit: true only when an unbound direct/test launch was
       normalized to the identity lineup. It is not caller-controlled. */
    bool legacy_direct_launch;
    TecmoGameplaySceneResult result;
    TecmoPlayerStatsGameLedger player_stats;

    TecmoGameplaySceneActor actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplaySceneCpuActor
        cpu_actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT];
    TecmoGameplayLiveFoundation live_foundation;
    TecmoGameplaySceneOpcode10FrameContext opcode10_frame_context;
    TecmoGameplaySceneOpcode16FrameContext opcode16_frame_context;
    TecmoGameplaySceneA023LatchFrameContext a023_latch_frame_context;
    TecmoGameplaySceneClaimantSettlementTrace claimant_settlement_trace;
    uint8_t controlled_actor[TECMO_GAMEPLAY_CONTROLLER_COUNT];
    uint8_t ball_holder;
    TecmoGameplayScenePassState pass_state;
    TecmoGameplaySceneInboundState inbound_state;
    TecmoGameplaySceneLooseBallState loose_ball_state;
    TecmoGameplayCourtCoordinateQ8 ball_position;
    TecmoGameplayCourtCoordinateQ8 shot_start_position;
    TecmoGameplayCourtCoordinateQ8 shot_end_position;
    /* Immutable launch snapshot for TGOR direction/family/evaluator
       validation.  Claimant movement must not retarget a live shot. */
    TecmoGameplayCourtCoordinate shot_actor_launch_position;
    uint8_t shot_actor_team;
    uint8_t shot_actor_roster_index;
    bool shot_launch_facing_right;
    uint32_t shot_launch_frame;
    int16_t shot_target_delta_x;
    int16_t shot_target_delta_y;
    /* Captured close-vs-jump launch classification.  This is a neutral
       production selector result, not a semantic shot label. */
    bool shot_close_context;
    uint32_t camera_follow_count;
    uint16_t shot_frame;
    uint16_t shot_duration;
    uint16_t action_serial;
    uint16_t free_throw_frame;
    uint32_t free_throw_lineup_transition_serial;
    uint8_t free_throw_lineup_orientation;
    uint8_t free_throw_shooter;
    uint8_t free_throw_secondary;
    bool free_throw_lineup_active;
    uint8_t shot_points;
    /* Captured TGSR point-classification flags.  Production controller
       launches use zero; the owned settlement-only one-point fixture uses
       the source mask explicitly without claiming normal controller
       one-point selection. */
    uint8_t shot_flags;
    uint8_t shot_actor;
    uint8_t close_shot_step;
    TecmoGameplayCloseShotProfile close_shot_profile;
    TecmoGameplayCloseShotDirection close_shot_direction;
    TecmoGameplayCloseShotVariant close_shot_variant;
    /* Native FNV policy input with no Bank/address, RAM-byte, or ROM-RNG
       identity. Its low byte feeds the raw-shaped TGSR rim-route resolver;
       the upper bits keep the native outcome policy auditable. */
    uint32_t native_policy_sample;
    uint8_t shot_make_probability;
    bool shot_contact_context;
    bool shot_contest_context;
    /* Redundant launch-time binding of the derived policy sample/contact/
       contest classification. It is not an independent post-launch
       proximity recomputation from moving defenders. */
    uint32_t shot_context_signature;
    bool shot_result_awarded;
    TecmoGameplayShotOutcome shot_outcome;
    TecmoGameplayShotScheduleKind shot_schedule;
    uint8_t shot_rim_rattle_raw_selector;
    TecmoGameplayShotRimRoute shot_rim_route;
    bool shot_rim_rattle_selected;
    bool shot_rim_tail_active;
    uint8_t shot_rim_tail_frame;
    uint8_t shot_rim_tail_duration;
    uint16_t shot_rim_tail_base_frame;
    uint16_t jump_actor_altitude_q8;
    uint16_t jump_actor_velocity_q8;
    uint16_t jump_ball_altitude_q8;
    uint16_t jump_ball_bounce_q8;
    uint16_t jump_entry_pose_index;
    /* TGJS [family][profile][direction] result.  The entry pose remains the
       actor's captured first-four-tick pose; this separate value is the
       source-backed pose consumed by ordinary flight playback. */
    uint16_t jump_resolved_pose_index;
    uint8_t jump_actor_state;
    uint8_t jump_ball_state;
    uint8_t jump_phase_counter;
    uint8_t jump_pose_frame;
    /* Human pad for controller-owned jumps, or NO_ACTOR for typed automatic
       CPU playback. Close shots retain NO_ACTOR under both ownership modes. */
    uint8_t shot_controller;
    TecmoGameplayJumpShotFamily jump_family;
    TecmoGameplayJumpShotProfile jump_profile;
    TecmoGameplayJumpShotDirection jump_direction;
    /* Source-shaped object-10 planar launch state. Render altitude and the
       existing native visual interpolation remain separate presentation. */
    bool shot_a0f3_origin_valid;
    uint16_t shot_a0f3_origin_x;
    uint8_t shot_a0f3_origin_depth;
    bool shot_a0f3_preflight_valid;
    uint8_t shot_a0f3_preflight_raw_006a;
    uint8_t shot_a0f3_launch_raw_006a;
    TecmoGameplayCpuA0f3Result shot_a0f3_result;
    TecmoGameplayCpuA0f3Motion shot_a0f3_motion;
    bool shot_a0f3_motion_valid;
    bool shot_a0f3_raw_position_valid;
    uint16_t shot_a0f3_raw_x;
    uint8_t shot_a0f3_raw_depth;
    uint16_t shot_a0f3_tick_count;
    bool shot_a8e9_normalized_valid;
    TecmoGameplayCpuA8e9VelocityResult shot_a8e9_normalized;
    /* Scene-native playback and predicted-route bookkeeping with no direct
       RAM-byte identity. Bank05 $83E9-$842B and $8469-$847A supply bounded
       ordinary-jump pose-cycle evidence; live make/miss prediction remains
       native policy. */
    bool jump_playback_active;
    bool predicted_make_route;
    bool jump_b_released;
    TecmoGameplayShotOutcome jump_outcome;
    bool jump_actor_landed;
    bool jump_rim_rattle_debug;
    uint8_t jump_rim_rattle_raw_selector;
    uint8_t jump_rim_rattle_audio_repeats;
    TecmoGameplayShotRimRattle jump_rim_rattle;
    TecmoGameplayJumpShotMadeSettlement jump_made_settlement;
    TecmoGameplaySceneShotKind shot_kind;
    TecmoGameplaySceneFoulPresentation foul_presentation;
    TecmoGameplayPhase previous_phase;
    bool pretip_abort_pending;
    uint32_t frame;
} TecmoGameplayScene;

/* Initialize exactly once before load/destroy. */
void tecmo_gameplay_scene_init(TecmoGameplayScene *scene);

/* Loads TGPL-1, TGCT-1, TGCP-2, TGMO-1, TGBD-1, TGAI-3, TGFT-1, TPNL-1, TGOR-1, TGFL-1,
   THUD-1, TGCS-1, TGDK-1, TGJS-2, TGSR-4, TGRB-1, TSFX-1, and TDMC-1 from one pack.
   `asset_pack_path` may be NULL to use the strict runtime search order.
   Runtime data is never read from decompilation/capture paths. */
bool tecmo_gameplay_scene_load(TecmoGameplayScene *scene,
                               const char *project_root,
                               const char *asset_pack_path,
                               TecmoMusicPlayer *music_player);
void tecmo_gameplay_scene_destroy(TecmoGameplayScene *scene);

bool tecmo_gameplay_scene_launch(TecmoGameplayScene *scene,
                                 const TecmoGameplaySceneLaunch *launch);
bool tecmo_gameplay_scene_bind_opcode10_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneOpcode10FrameContext *context);
bool tecmo_gameplay_scene_bind_opcode16_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneOpcode16FrameContext *context);
bool tecmo_gameplay_scene_bind_a023_latch_frame_context(
    TecmoGameplayScene *scene,
    const TecmoGameplaySceneA023LatchFrameContext *context);
bool tecmo_gameplay_scene_update(TecmoGameplayScene *scene,
                                 const TecmoControlFrame *player_one,
                                 const TecmoControlFrame *player_two);
bool tecmo_gameplay_scene_result(const TecmoGameplayScene *scene,
                                 TecmoGameplaySceneResult *result);
bool tecmo_gameplay_scene_court_coordinates(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtCoordinates *coordinates_out);
bool tecmo_gameplay_scene_court_projection(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtProjection *projection_out);
bool tecmo_gameplay_scene_court_slice(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtSlice *slice_out);
bool tecmo_gameplay_scene_court_frame(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneCourtFrame *frame_out);
/* Captures the passive typed LIVE ownership snapshot used by opt-in developer
 * JSONL proof. It never reads decompilation, FCEUX, a ROM, or a debug log. */
bool tecmo_gameplay_scene_possession_trace_snapshot(
    const TecmoGameplayScene *scene,
    TecmoGameplayScenePossessionTraceSnapshot *snapshot_out);
/* Returns the exact TGFL-derived raw lineup currently bound to a live
   free-throw sequence. Inactive or malformed bindings leave output unchanged. */
bool tecmo_gameplay_scene_free_throw_lineup(
    const TecmoGameplayScene *scene,
    TecmoGameplayFreeThrowLineup *lineup_out);
bool tecmo_gameplay_scene_consume_pretip_abort(TecmoGameplayScene *scene);
bool tecmo_gameplay_scene_in_pretip(const TecmoGameplayScene *scene);
void tecmo_gameplay_scene_end(TecmoGameplayScene *scene);

/* Deterministic test/render route for the behavior-verified state-$15 prefix.
   It uses the observed raw selector $71 and a canonical four-pass source.
   Normal live shot selection never calls this API. */
bool tecmo_gameplay_scene_start_rim_rattle_debug(
    TecmoGameplayScene *scene);

/* Draws a TGCT-1 world slice at the persistent TGCP-2 camera, projects
   resolved ROM poses through that same camera, and overlays THUD-1's fixed
   live scoreboard rows whenever dynamic actors are included. Live
   R2 shot playback consumes the selected TGCS close profile/direction and
   TGJS jump family/profile/direction for every supported matrix entry. The
   captured schedule is exact only for the source-pinned three-point route;
   other shot arcs, full unproven $91BC/$AD6E inputs, and presentation details
   use bounded native approximations. This contract does not claim complete
   presentation mapping or unsupported ROM behavior. Presentation banners
   beyond the two live HUD rows are supplied by the runtime overlay. */
bool tecmo_gameplay_scene_draw(const TecmoGameplayScene *scene,
                               TecmoFramebuffer *framebuffer,
                               int origin_x,
                               int origin_y,
                               int scale,
                               bool include_actors);
/* Returns a stable, human-readable identifier for the first preflight
   contract which would reject a gameplay render.  It is intended for the
   runtime rejection screen and continuous regression proofs; it does not
   mutate scene or framebuffer state. */
const char *tecmo_gameplay_scene_render_diagnostic(
    const TecmoGameplayScene *scene,
    const TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale,
    bool include_actors);
bool tecmo_gameplay_scene_in_dunk_presentation(
    const TecmoGameplayScene *scene);

const char *tecmo_gameplay_scene_shot_name(TecmoGameplaySceneShotKind kind);
bool tecmo_gameplay_scene_self_test(const char *project_root,
                                    const char *asset_pack_path,
                                    TecmoMusicPlayer *music_player,
                                    char *message,
                                    size_t message_size);

#endif
