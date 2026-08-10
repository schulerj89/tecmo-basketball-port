#ifndef TECMO_GAMEPLAY_SCENE_INTERNAL_H
#define TECMO_GAMEPLAY_SCENE_INTERNAL_H

/* Private coordination contracts for the split native gameplay scene.  This
   header is intentionally not part of the public scene ABI. */
#include "tecmo_gameplay_scene.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SCENE_LIFECYCLE_TAG 0x53434E31U
#define TECMO_GAMEPLAY_TEAM_LIMIT TECMO_GAMEPLAY_SCENE_TEAM_LIMIT
#define TECMO_GAMEPLAY_BALL_POSE 64U
#define TECMO_GAMEPLAY_SHOT_TARGET_Y 0x008F
#define TECMO_GAMEPLAY_INITIAL_CAMERA_X 0x0084
#define TECMO_GAMEPLAY_LEFT_BOUNDARY_BASE 0x00DF
#define TECMO_GAMEPLAY_RIGHT_BOUNDARY_BASE 0x0220
#define TECMO_GAMEPLAY_MIN_Y TECMO_GAMEPLAY_COURT_WORLD_MIN_Y
#define TECMO_GAMEPLAY_MAX_Y TECMO_GAMEPLAY_COURT_WORLD_MAX_Y
#define TECMO_GAMEPLAY_CLOSE_DISTANCE_X 48
#define TECMO_GAMEPLAY_JUMP_SLOT0_DURATION 87U
#define TECMO_GAMEPLAY_JUMP_RATTLE_DURATION 103U
#define TECMO_GAMEPLAY_JUMP_RATTLE_BEGIN_FRAME 73U
#define TECMO_GAMEPLAY_JUMP_RATTLE_HANDOFF_FRAME 89U
#define TECMO_GAMEPLAY_JUMP_RATTLE_FRAME_SHIFT 16U
/* The visible side-0 route proves a negative incoming sign, not its exact
   horizontal magnitude. Only the sign affects state-$15 setup. */
#define TECMO_GAMEPLAY_JUMP_RATTLE_NEGATIVE_INCOMING_X_SENTINEL_Q6 (-1)
/* Bank05 $AD4E launches at the side target selected by $BDEF-$BDF2,
   with the shared target Y loaded as $8F. */
#define TECMO_GAMEPLAY_JUMP_RATTLE_SOURCE_TARGET_Y 0x008F
#define TECMO_GAMEPLAY_JUMP_MAKE_DURATION 111U
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_DURATION 64U
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RELEASE_FRAME 6U
/* The shared TGJS Q8 step with initial velocity $0308 requires 38 updates
   to reach the floor: approximate frames 7..44 inclusive.  Recovery then
   decrements the proven phase seed five times (45..49) and neutralizes on
   the sixth following frame (50). */
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_LAND_FRAME 44U
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_START_FRAME 45U
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_RECOVERY_LAST_FRAME 49U
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_NEUTRAL_FRAME 50U
/* The ball-arrival/score point is a bounded native approximation.  It is
   deliberately after actor neutralization so the terminal predicate remains
   physically consistent rather than requiring a grounded actor in mid-flight. */
#define TECMO_GAMEPLAY_JUMP_APPROX_MAKE_SCORE_FRAME 56U
#define TECMO_GAMEPLAY_JUMP_MAKE_RELEASE_FRAME 9U
#define TECMO_GAMEPLAY_JUMP_MAKE_DECISION_FRAME 19U
#define TECMO_GAMEPLAY_JUMP_MAKE_FLIGHT_FRAME 20U
#define TECMO_GAMEPLAY_JUMP_MAKE_LAND_FRAME 57U
#define TECMO_GAMEPLAY_JUMP_MAKE_NEUTRAL_FRAME 63U
#define TECMO_GAMEPLAY_JUMP_MAKE_SCORE_FRAME 85U
#define TECMO_GAMEPLAY_JUMP_ENTRY_POSE_LAST_FRAME 4U
#define TECMO_GAMEPLAY_JUMP_TURN_POSE_FIRST_FRAME 5U
#define TECMO_GAMEPLAY_JUMP_TURN_POSE_LAST_FRAME 8U
#define TECMO_GAMEPLAY_JUMP_RELEASE_POSE_FRAME 9U
#define TECMO_GAMEPLAY_JUMP_FLIGHT_POSE_FRAME 10U
#define TECMO_GAMEPLAY_JUMP_SLOT0_INITIAL_ALTITUDE_Q8 0x02E8U
#define TECMO_GAMEPLAY_JUMP_SLOT0_ACTOR_VELOCITY_Q8 0x02E8U
#define TECMO_GAMEPLAY_JUMP_MAKE_ACTOR_VELOCITY_Q8 0x0308U
#define TECMO_GAMEPLAY_JUMP_SLOT0_IDLE_POSE 469U
#define TECMO_GAMEPLAY_JUMP_MAKE_GATHER_POSE 325U
#define TECMO_GAMEPLAY_JUMP_TURN_POSE 1060U
#define TECMO_GAMEPLAY_JUMP_RELEASE_POSE 1061U
#define TECMO_GAMEPLAY_JUMP_FLIGHT_POSE 213U
#define TECMO_GAMEPLAY_CLOSE_NUMERIC_1_DURATION 24U
#define TECMO_GAMEPLAY_SHOT_RIM_TAIL_RATTLE_UPDATES 16U
#define TECMO_GAMEPLAY_SHOT_RIM_TAIL_GROUND_UPDATE 1U
#define TECMO_GAMEPLAY_SCENE_RENDER_FNV1A32 0xCA583099U
#define TECMO_GAMEPLAY_SCENE_CENTER_SLICE_FNV1A32 0xAA4B4E7DU
#define TECMO_GAMEPLAY_SCENE_LEFT_SLICE_FNV1A32 0x770FAE95U
#define TECMO_GAMEPLAY_SCENE_RIGHT_SLICE_FNV1A32 0x2DBDF155U
/* Native live-foul bridge inputs. Bank05 $957E saves an ordinary pre-commit
 * route zero in $07E3 and installs $19 in $0478; $05A8 is not retained by
 * the scene. These constants are an explicitly limited adapter profile, not
 * reconstructed original RAM or a general route/collision model. */
#define TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_SAVED_ROUTE 0x00U
#define TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CURRENT_ROUTE 0x19U
#define TECMO_GAMEPLAY_LIVE_FOUL_BRIDGE_CONTACT_SELECTOR 0x00U
#define TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_0_CAMERA_X 0x0066U
#define TECMO_GAMEPLAY_FREE_THROW_ORIENTATION_1_CAMERA_X 0x0198U
#define TECMO_GAMEPLAY_PRETIP_DESCENT_START_Y 71
#define TECMO_GAMEPLAY_PRETIP_DESCENT_END_Y 145
#define TECMO_GAMEPLAY_PRETIP_DESCENT_MOVE_FRAMES 60U
/* Native presentation policy: the original source proves the tip setup and
   input gates, but not a complete jump trajectory. The 30-update TPTI
   contest/input window is intentionally decoupled from this readable,
   scene-owned 60-update visual schedule so late input cannot cut off
   landing. */
#define TECMO_GAMEPLAY_PRETIP_JUMP_DURATION \
    TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES
#define TECMO_GAMEPLAY_PRETIP_JUMP_CROUCH_LAST_FRAME 7U
#define TECMO_GAMEPLAY_PRETIP_JUMP_TAKEOFF_LAST_FRAME 15U
#define TECMO_GAMEPLAY_PRETIP_JUMP_RISE_LAST_FRAME 25U
#define TECMO_GAMEPLAY_PRETIP_JUMP_CONTACT_FRAME 26U
#define TECMO_GAMEPLAY_PRETIP_JUMP_APEX_LAST_FRAME 35U
#define TECMO_GAMEPLAY_PRETIP_JUMP_FALL_LAST_FRAME 51U
#define TECMO_GAMEPLAY_PRETIP_JUMP_LAND_FIRST_FRAME 52U
#define TECMO_GAMEPLAY_PRETIP_JUMP_MAX_ALTITUDE_Q8 6144U
#define TECMO_GAMEPLAY_HUD_PRIMARY_ROW 2U
#define TECMO_GAMEPLAY_HUD_SECONDARY_ROW 3U
#define TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT 2U
#define TECMO_GAMEPLAY_HUD_COLUMN_COUNT 32U
#define TECMO_GAMEPLAY_HUD_AWAY_SCORE_COLUMN 6U
#define TECMO_GAMEPLAY_HUD_CLOCK_COLUMN 13U
#define TECMO_GAMEPLAY_HUD_HOME_SCORE_COLUMN 28U
#define TECMO_GAMEPLAY_HUD_AWAY_NUMBER_COLUMN 1U
#define TECMO_GAMEPLAY_HUD_AWAY_PLAYER_COLUMN 4U
#define TECMO_GAMEPLAY_HUD_HOME_NUMBER_COLUMN 17U
#define TECMO_GAMEPLAY_HUD_HOME_PLAYER_COLUMN 20U
#define TECMO_GAMEPLAY_HUD_SCORE_WIDTH 3U
#define TECMO_GAMEPLAY_HUD_CLOCK_MINUTE_WIDTH 2U
#define TECMO_GAMEPLAY_HUD_CLOCK_SECOND_WIDTH 2U
#define TECMO_GAMEPLAY_HUD_PLAYER_WIDTH 11U
#define TECMO_GAMEPLAY_HUD_SURNAME_WIDTH 9U
#define TECMO_GAMEPLAY_HUD_COLON_TILE 0x16U

typedef struct TecmoGameplayPreparedHud {
    bool occupied[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                 [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    bool chr_resolved[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                     [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    uint8_t tiles[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                 [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
    uint32_t chr_offsets[TECMO_GAMEPLAY_HUD_VISIBLE_ROW_COUNT]
                        [TECMO_GAMEPLAY_HUD_COLUMN_COUNT];
} TecmoGameplayPreparedHud;

typedef struct TecmoGameplaySceneCpuShotRequest {
    bool requested;
    uint8_t actor_index;
    bool playback_supported;
    bool deferred;
} TecmoGameplaySceneCpuShotRequest;

/* Lower-layer court snapshot and invariant seams. */
bool scene_court_controller_team_valid(uint8_t team);
bool scene_court_free_throw_lineup_matches(
    const TecmoGameplayScene *scene);
bool scene_ownership_valid(const TecmoGameplayScene *scene);
/* Legacy/direct render adapters intentionally do not carry a starter
   binding.  Their shot boundary still uses the deep owned shot-state
   invariants; bound production callers use scene_ownership_valid(). */
bool scene_shot_state_valid(const TecmoGameplayScene *scene);

/* Actor locomotion, ball attachment, and CPU adapter seam. */
bool scene_movement_pose_index(
    const TecmoGameplayScene *scene,
    const TecmoGameplayMovementState *movement,
    const TecmoGameplayCourtCoordinate *linked_position,
    uint16_t *pose_index_out);
bool scene_actor_movement_pose_index(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    size_t actor_index,
    const TecmoGameplayMovementState *movement,
    uint16_t *pose_index_out);
TecmoGameplayTeam scene_other_team(TecmoGameplayTeam team);
bool scene_goal_facing_right_for_team(
    const TecmoGameplayScene *scene,
    TecmoGameplayTeam team,
    bool *facing_right_out);
bool scene_apply_goal_facing(
    const TecmoGameplayScene *scene,
    TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT]);
bool scene_actor_coordinate_valid(const TecmoGameplayCourtCoordinate *coordinate);
bool scene_actor_world_position_valid(const TecmoGameplaySceneActor *actor);
void scene_clamp_actor_world(TecmoGameplaySceneActor *actor);
bool scene_actor_movement_state(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor,
    TecmoGameplayMovementState *state_out);
const TecmoTeamDataPlayer *scene_actor_player(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor *actor);
bool scene_live_ball_frame_for_actors(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    uint8_t holder_index,
    TecmoGameplayBallDribbleFrame *frame_out);
bool scene_move_controlled_actor(TecmoGameplayScene *scene,
                                 size_t controller,
                                 const TecmoControlFrame *controls);
uint8_t scene_first_actor_for_team(TecmoGameplayTeam team);
bool scene_attach_ball(TecmoGameplayScene *scene);

/* Stable TGSR sample input retained at the shot boundary.  The native
   substitution preserves the accepted low-byte sample stream and adds
   identity-preserving FNV steps for every nonzero upper frame byte, so all
   captured frame bits remain fail-closed without changing ordinary frames. */
uint32_t scene_shot_stable_sample_from_inputs(
    int16_t actor_x,
    int16_t actor_y,
    uint8_t point_value,
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint8_t actor_team,
    uint8_t actor_roster_index,
    uint32_t launch_frame);
uint32_t scene_shot_context_signature(
    uint32_t stable_sample,
    bool contact_context,
    bool contest_context);
bool scene_shot_captured_rattle_orientation(
    const TecmoGameplayScene *scene,
    uint8_t *orientation_out);

/* Neutral bounded source substitution for close numeric-2 reachability in
   vertical sectors whose physical close approach is necessarily <=24. */
int16_t scene_close_variant_selection_approach(
    int approach_distance_x,
    TecmoGameplayShotDirectionSlot direction,
    uint32_t stable_sample);
bool scene_ball_position_for_actors(
    const TecmoGameplayScene *scene,
    const TecmoGameplaySceneActor
        actors[TECMO_GAMEPLAY_SCENE_ACTOR_COUNT],
    uint8_t holder_index,
    TecmoGameplayCourtCoordinateQ8 *position_out);
bool scene_attached_ball_position(
    const TecmoGameplaySceneActor *holder,
    TecmoGameplayCourtCoordinateQ8 *position_out);
bool scene_settle_boundary_latch(TecmoGameplayScene *scene,
                                 bool *settled_out);
bool scene_settle_backcourt(TecmoGameplayScene *scene,
                            bool *settled_out);
uint32_t scene_distance_squared(const TecmoGameplaySceneActor *a,
                                const TecmoGameplaySceneActor *b);
uint8_t scene_next_teammate(const TecmoGameplayScene *scene,
                            uint8_t actor_index);
uint8_t scene_nearest_actor_for_team(const TecmoGameplayScene *scene,
                                     TecmoGameplayTeam team,
                                     uint8_t target);
bool scene_pass_or_switch(TecmoGameplayScene *scene, size_t controller);
bool scene_update_selection_candidates(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *controls[TECMO_GAMEPLAY_CONTROLLER_COUNT]);
bool scene_sync_live_foundation(TecmoGameplayScene *scene);
size_t scene_controller_for_team(const TecmoGameplayScene *scene,
                                 TecmoGameplayTeam team);
bool scene_cpu_actor_state_valid(
    const TecmoGameplayScene *scene,
    size_t actor,
    const TecmoGameplaySceneCpuActor *cpu);
bool scene_cpu_target_for_source_direction(
    const TecmoGameplayCpuSteeringAssets *assets,
    const TecmoGameplayCourtCoordinate *actor_position,
    uint8_t source_direction,
    TecmoGameplayCourtCoordinate *target_out);
bool scene_update_ai(
    TecmoGameplayScene *scene,
    TecmoGameplaySceneCpuShotRequest *shot_request_out);
bool scene_tick_fatigue(TecmoGameplayScene *scene);

/* Shot/contact/possession orchestration seam. */
bool scene_shot_is_close(TecmoGameplaySceneShotKind kind);
bool scene_close_pose_for_step(const TecmoGameplayScene *scene,
                               uint8_t step,
                               uint16_t *pose_index);
uint8_t scene_shot_family_for_context(
    int16_t target_delta_x,
    int16_t target_delta_y,
    uint32_t stable_sample);
bool scene_start_shot_actor(TecmoGameplayScene *scene,
                            size_t controller,
                            uint8_t actor_index);
bool scene_start_shot(TecmoGameplayScene *scene, size_t controller);
bool scene_handoff_possession(TecmoGameplayScene *scene,
                              TecmoGameplayTeam possession,
                              uint8_t holder);
bool scene_handoff_tip_possession(TecmoGameplayScene *scene,
                                  TecmoGameplayTeam possession,
                                  uint8_t holder);
bool scene_update_shot(TecmoGameplayScene *scene,
                       const TecmoControlFrame *shooting_controls);
bool scene_update_jump_miss(
    TecmoGameplayScene *scene,
    const TecmoControlFrame *shooting_controls);
bool scene_try_defense_action(TecmoGameplayScene *scene,
                              size_t controller);
void scene_shot_clear_jump_playback(TecmoGameplayScene *scene);
bool scene_shot_queue_result_audio(TecmoGameplayScene *scene,
                                   TecmoGameplayTeam scoring_team);

/* Rendering/pre-tip/HUD seam. */
bool tecmo_gameplay_scene_render_build_background_context(
    const TecmoGameplayScene *scene,
    TecmoGameplayLiveBackgroundContext *context);
bool tecmo_gameplay_scene_render_prepare_live_hud(
    const TecmoGameplayScene *scene,
    const TecmoGameplayLiveBackgroundContext *context,
    TecmoGameplayPreparedHud *prepared);
bool tecmo_gameplay_scene_render_resolve_pose(
    const TecmoGameplayScene *scene,
    uint16_t pointer_index,
    uint8_t actor_slot_base,
    uint8_t actor_attributes,
    uint8_t palette_group,
    bool apply_uniform_color,
    uint8_t uniform_color,
    TecmoGameplayResolvedPose *pose);
bool tecmo_gameplay_scene_render_resolve_actor_pose(
    const TecmoGameplayScene *scene,
    size_t actor_index,
    TecmoGameplayResolvedPose *resolved);

#endif
