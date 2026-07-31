#ifndef TECMO_GAMEPLAY_PRETIP_H
#define TECMO_GAMEPLAY_PRETIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT 17U
#define TECMO_GAMEPLAY_PRETIP_SCREEN_ID 0x15U
#define TECMO_GAMEPLAY_PRETIP_PHASE_COUNT 8U
#define TECMO_GAMEPLAY_PRETIP_STATE_TAG 0x49545054U

typedef enum TecmoGameplayPreTipSourceKind {
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_DESCRIPTOR = 1,
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_STREAM = 2,
    TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_PALETTE = 3,
    TECMO_GAMEPLAY_PRETIP_SOURCE_WAIT_HELPERS = 4,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MATCHUP_SEQUENCE = 5,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_STRINGS = 6,
    TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_POINTERS = 7,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CHARACTER_MAP = 8,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_ENTRY = 9,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_PALETTE = 10,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_CONTROL = 11,
    TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_TIMING = 12,
    TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_SETUP = 13,
    TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_UPDATE = 14,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LAUNCH_BRIDGE = 15,
    TECMO_GAMEPLAY_PRETIP_SOURCE_LIVE_HANDOFF = 16,
    TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_SELECT = 17
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
    uint8_t descriptor[7];
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

typedef struct TecmoGameplayPreTipState {
    uint32_t contract_tag;
    TecmoGameplayPreTipPhase phase;
    uint16_t phase_frame;
    uint32_t total_frame;
    uint8_t away_tip_error;
    uint8_t home_tip_error;
    bool away_tip_sampled;
    bool home_tip_sampled;
    bool aborted;
    bool live_handoff;
} TecmoGameplayPreTipState;

void tecmo_gameplay_pretip_init(TecmoGameplayPreTipAssets *assets);
void tecmo_gameplay_pretip_destroy(TecmoGameplayPreTipAssets *assets);
bool tecmo_gameplay_pretip_load(TecmoGameplayPreTipAssets *assets,
                                const char *asset_pack_path);
bool tecmo_gameplay_pretip_state_initialize(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state);
bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_held_b,
    bool player_two_held_b);
bool tecmo_gameplay_pretip_is_presentation(
    const TecmoGameplayPreTipState *state);
const char *tecmo_gameplay_pretip_phase_name(TecmoGameplayPreTipPhase phase);
bool tecmo_gameplay_pretip_self_test(const char *asset_pack_path,
                                     char *message,
                                     size_t message_size);

#endif
