#ifndef TECMO_GAMEPLAY_PRETIP_H
#define TECMO_GAMEPLAY_PRETIP_H

#include "tecmo_gameplay_court.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT 20U
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
/* The original automatic-path timing is not proven by the bounded source
   spans.  The native scene therefore uses one deterministic, visible-window
   CPU decision frame as an explicit approximation. */
#define TECMO_GAMEPLAY_PRETIP_CPU_SAMPLE_FRAME 8U
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
    TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_UPDATE = 17,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LAUNCH_BRIDGE = 18,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LIVE_HANDOFF = 19,
    TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_SELECT = 20
} TecmoGameplayPreTipSourceKind;

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
    bool away_b_latched;
    bool home_b_latched;
    bool away_jump_committed;
    bool home_jump_committed;
    bool claim_resolved;
    uint8_t away_actor_state;
    uint8_t home_actor_state;
    uint8_t away_animation_phase;
    uint8_t home_animation_phase;
    uint8_t away_raw_height;
    uint8_t home_raw_height;
    uint8_t ball_raw_height;
    uint8_t away_gate_countdown;
    uint8_t home_gate_countdown;
    uint8_t claimant_jumper;
    uint8_t receiver_actor;
    int16_t away_velocity_q8;
    int16_t home_velocity_q8;
    uint16_t away_altitude_q8;
    uint16_t home_altitude_q8;
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
/* Card phases consume raw P1/P2 held-B levels for cancellation. During
   JUMP_CONTEST, callers must pass team-routed away/home held-B levels. The
   contest samples only the first 30 updates; the remaining presentation
   frames are a native readability approximation. */
bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b);
bool tecmo_gameplay_pretip_update_controlled(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool away_held_b,
    bool home_held_b,
    bool away_cpu,
    bool home_cpu);
/* Rejects every phase before JUMP_CONTEST without changing winner. During
   JUMP_CONTEST and LIVE, returns the native approximate lower-error/tie-away
   contest result. */
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
