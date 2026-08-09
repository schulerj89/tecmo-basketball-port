#ifndef TECMO_GAMEPLAY_PRETIP_H
#define TECMO_GAMEPLAY_PRETIP_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT 29U
#define TECMO_GAMEPLAY_PRETIP_SCREEN_ID 0x15U
#define TECMO_GAMEPLAY_PRETIP_PHASE_COUNT 8U
#define TECMO_GAMEPLAY_PRETIP_STATE_TAG 0x49545054U
#define TECMO_GAMEPLAY_PRETIP_GLYPH_COUNT 38U
#define TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT 4U
#define TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT 10U
#define TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT 2U
#define TECMO_GAMEPLAY_PRETIP_OBJECT_COUNT 11U
#define TECMO_GAMEPLAY_PRETIP_LINEUP_TAG 0x4C545031U
#define TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME UINT16_MAX
#define TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES 30U
#define TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES 60U
#define TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR 12U
#define TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR 11U
#define TECMO_GAMEPLAY_PRETIP_INPUT_MASK 0x40U
#define TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_BASE 0x3DU
#define TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_MASK 0x1FU
#define TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_SHIFT 2U
#define TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_HIGH_MIN 0x3AU
#define TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_MINUS_JUMPER_LIMIT 0x3AU
#define TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE 0x0BU
#define TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE 0x17U
#define TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED 0x07U
#define TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED 0x02U
/* Bounded native-approximate automatic calibration vectors. */
#define TECMO_GAMEPLAY_PRETIP_AUTOMATIC_BOTH_AWAY_FRAME 20U
#define TECMO_GAMEPLAY_PRETIP_AUTOMATIC_SINGLE_FRAME 21U
#define TECMO_GAMEPLAY_PRETIP_AUTOMATIC_BOTH_HOME_FRAME 22U
#define TECMO_GAMEPLAY_PRETIP_CLAIMANT_NONE 0xFFU
#define TECMO_GAMEPLAY_PRETIP_TPM2_VERSION 2U
#define TECMO_GAMEPLAY_PRETIP_TPM2_SIZE 96U
#define TECMO_GAMEPLAY_PRETIP_AWAY_WINNER 0U
#define TECMO_GAMEPLAY_PRETIP_HOME_WINNER 1U

typedef enum TecmoGameplayPreTipSourceKind {
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_DESCRIPTOR = 1,
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_STREAM = 2,
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_PALETTE = 3,
    TECMO_GAMEPLAY_PRETIP_SOURCE_WAIT_HELPERS = 4,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MATCHUP_SEQUENCE = 5,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_STRINGS = 6,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_POINTERS = 7,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CHARACTER_MAP = 8,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CHARACTER_TILES = 9,
    TECMO_GAMEPLAY_PRETIP_SOURCE_TEXT_CHR_SELECTORS = 10,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_ENTRY = 11,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_PALETTE = 12,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_CONTROL = 13,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_TIMING = 14,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_SPRITE_STAGING = 15,
    TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_SETUP = 16,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LATER_GENERAL_COLLISION_SETTLEMENT = 17,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LAUNCH_BRIDGE = 18,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LIVE_HANDOFF = 19,
    TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_ORDERING = 20,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CAPTURE_ERROR = 21,
    TECMO_GAMEPLAY_PRETIP_SOURCE_ACTOR_DISPATCHER = 22,
    TECMO_GAMEPLAY_PRETIP_SOURCE_AUTOMATIC_ACTOR_PATH = 23,
    TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_DISPATCHER = 24,
    TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_ACTOR_PATH = 25,
    TECMO_GAMEPLAY_PRETIP_SOURCE_JUMP_COMMIT = 26,
    TECMO_GAMEPLAY_PRETIP_SOURCE_SLOT10_CLAIM = 27,
    TECMO_GAMEPLAY_PRETIP_SOURCE_E56E_HOOK_ANCHOR = 28,
    TECMO_GAMEPLAY_PRETIP_SOURCE_RNG_MIX = 29
} TecmoGameplayPreTipSourceKind;

/* Source names retained as aliases for older focused callers. */
#define TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_UPDATE \
    TECMO_GAMEPLAY_PRETIP_SOURCE_LATER_GENERAL_COLLISION_SETTLEMENT
#define TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_SELECT \
    TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_ORDERING

typedef enum TecmoGameplayPreTipPhase {
    TECMO_GAMEPLAY_PRETIP_PRESEASON = 0,
    TECMO_GAMEPLAY_PRETIP_MATCHUP,
    TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD,
    TECMO_GAMEPLAY_PRETIP_CLOSEUP,
    TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP,
    TECMO_GAMEPLAY_PRETIP_BALL_DESCENT,
    TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP,
    TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST,
    TECMO_GAMEPLAY_PRETIP_LIVE
} TecmoGameplayPreTipPhase;

typedef struct TecmoGameplayPreTipSourceSpan {
    TecmoGameplayPreTipSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
    const uint8_t *bytes;
} TecmoGameplayPreTipSourceSpan;

typedef struct TecmoGameplayPreTipAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[192];
    uint8_t *storage;
    size_t storage_size;
    const uint8_t *nametables;
    const uint8_t *palette;
    const uint8_t *character_map;
    const uint8_t *character_tiles;
    uint8_t descriptor[7];
    uint8_t card_chr_selector[2];
    /* TPTI header bytes 178/179: Bank04-selected away/home tip actors. */
    uint8_t tip_actor_indices[TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT];
    uint16_t phase_frames[TECMO_GAMEPLAY_PRETIP_PHASE_COUNT];
    TecmoGameplayPreTipSourceSpan
        sources[TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT];
    uint32_t gameplay_core_fingerprint;
    uint32_t team_data_fingerprint;
    uint32_t music_fingerprint;
    uint32_t warriors_fingerprint;
    uint32_t chr_fingerprint32;
    uint64_t chr_fingerprint64;
    uint32_t mechanics_fingerprint32;
    uint64_t mechanics_fingerprint64;
    uint32_t tgjs_fingerprint;
    uint8_t tip_input_mask;
    uint8_t tip_no_sample_error;
    uint8_t tip_max_sample_error;
    uint8_t tip_auto_threshold_base;
    uint8_t tip_auto_threshold_mask;
    uint8_t tip_auto_threshold_shift;
    uint8_t tip_claim_ball_high_min;
    uint8_t tip_claim_ball_minus_jumper_limit;
    uint8_t tip_actor_jump_commit_state;
    uint8_t tip_slot10_claim_commit_state;
    uint16_t tip_selector_0380_address;
    uint16_t tip_selector_037f_address;
    uint16_t tip_jump_post_store_address;
} TecmoGameplayPreTipAssets;

typedef struct TecmoGameplayPreTipLineup {
    uint32_t contract_tag;
    TecmoGameplayCourtCoordinate
        players[TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT];
    TecmoGameplayCourtCoordinate ball;
    uint8_t player_states[TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT];
    uint8_t player_sprite_slot_bases[TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT];
    uint8_t player_facings[TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT];
    uint16_t player_pose_indices[TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT];
    uint8_t ball_state;
    uint8_t ball_sprite_slot_base;
    uint8_t ball_facing;
    uint16_t ball_pose_index;
} TecmoGameplayPreTipLineup;

typedef struct TecmoGameplayPreTipState {
    uint32_t contract_tag;
    TecmoGameplayPreTipPhase phase;
    /* The scene-facing contest presentation clock. */
    uint16_t phase_frame;
    /* The native input clock remains the bounded 30-frame TPTI window. */
    uint16_t contest_frame;
    uint32_t total_frame;
    uint8_t away_tip_error;
    uint8_t home_tip_error;
    uint16_t away_tip_sample_frame;
    uint16_t home_tip_sample_frame;
    bool away_tip_sampled;
    bool home_tip_sampled;
    bool card_cancel_enabled;
    bool aborted;
    bool live_handoff;
    bool away_tip_automatic;
    bool home_tip_automatic;
    /* Controlled callers may expose an automatic branch before it commits;
       retain that request so the bounded 20/21/22 calibration remains
       state-owned and validates the first commit as well as the second. */
    bool away_automatic_requested;
    bool home_automatic_requested;
    bool away_automatic_triggered;
    bool home_automatic_triggered;
    bool away_jump_committed;
    bool home_jump_committed;
    bool claim_resolved;
    bool claim_deferred;
    bool contest_stalled;
    uint16_t away_jump_commit_frame;
    uint16_t home_jump_commit_frame;
    /* Genuine visual Q8 trajectory units consumed by the scene. */
    uint16_t away_jump_velocity_q8;
    uint16_t home_jump_velocity_q8;
    uint16_t away_jump_altitude_q8;
    uint16_t home_jump_altitude_q8;
    /* Raw 8-bit $048F analogues; never compared as Q8 values. */
    uint8_t away_claim_height_raw;
    uint8_t home_claim_height_raw;
    /* Raw 8-bit $0499 analogue used by the strict subtract/compare seam. */
    uint8_t tip_ball_high_raw;
    uint8_t tip_rng_6a;
    uint8_t tip_rng_53;
    uint8_t tip_rng_mix_count;
    uint8_t raw_selector_0380;
    uint8_t raw_selector_037f;
    uint8_t claimant_jumper;
    uint8_t receiver_actor;
} TecmoGameplayPreTipState;

void tecmo_gameplay_pretip_init(TecmoGameplayPreTipAssets *assets);
void tecmo_gameplay_pretip_destroy(TecmoGameplayPreTipAssets *assets);
bool tecmo_gameplay_pretip_load(TecmoGameplayPreTipAssets *assets,
                                const char *asset_pack_path);
bool tecmo_gameplay_pretip_tip_lineup(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipLineup *lineup);
bool tecmo_gameplay_pretip_state_initialize(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool card_cancel_enabled);
bool tecmo_gameplay_pretip_state_validate(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state);
/* Card phases consume raw P1/P2 held-B levels for cancellation. This existing
   entry remains human-only. During JUMP_CONTEST, callers pass team-routed
   away/home held-B levels; automatic branches use the controlled entry below.
   The contest samples only the first 30 updates. */
bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b);
bool tecmo_gameplay_pretip_update_controlled(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b,
    bool away_automatic,
    bool home_automatic);
/* Rejects every phase before JUMP_CONTEST without changing winner. During
   JUMP_CONTEST and LIVE, returns only a resolved claimant's logical team;
   unresolved/equal/stalled contests fail closed. */
bool tecmo_gameplay_pretip_tip_winner(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state,
    uint8_t *winner);
bool tecmo_gameplay_pretip_claimant_jumper(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state,
    uint8_t *jumper);
bool tecmo_gameplay_pretip_is_presentation(
    const TecmoGameplayPreTipState *state);
const char *tecmo_gameplay_pretip_phase_name(TecmoGameplayPreTipPhase phase);
bool tecmo_gameplay_pretip_self_test(const char *asset_pack_path,
                                     char *message,
                                     size_t message_size);

#endif
