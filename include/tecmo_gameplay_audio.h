#ifndef TECMO_GAMEPLAY_AUDIO_H
#define TECMO_GAMEPLAY_AUDIO_H

#include "tecmo_music.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_SFX_EFFECT_COUNT 7U
#define TECMO_GAMEPLAY_SFX_CHANNEL_COUNT 4U
#define TECMO_GAMEPLAY_SFX_VOICE_COUNT 14U
#define TECMO_GAMEPLAY_SFX_PITCH_COUNT 75U
#define TECMO_GAMEPLAY_SFX_INSTRUCTION_COUNT 131U
#define TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE 2824U
#define TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32 0x968A5DE6U
#define TECMO_GAMEPLAY_DMC_CLIP_COUNT 5U
#define TECMO_GAMEPLAY_DMC_POOL_COUNT 3U
#define TECMO_GAMEPLAY_DMC_DATA_SIZE 2179U
#define TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE 2515U
#define TECMO_GAMEPLAY_DMC_PAYLOAD_FNV1A32 0xAD70E6E8U

typedef enum TecmoGameplayAudioEvent {
    TECMO_GAMEPLAY_AUDIO_CLOCK_BUZZER = 1,
    TECMO_GAMEPLAY_AUDIO_COUNTDOWN = 2,
    TECMO_GAMEPLAY_AUDIO_CROWD_RESPONSE = 3,
    TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_12 = 4,
    TECMO_GAMEPLAY_AUDIO_SIDE_RESULT_13 = 5,
    TECMO_GAMEPLAY_AUDIO_HELD_BALL_DRIBBLE = 6,
    TECMO_GAMEPLAY_AUDIO_VIOLATION_CUE = 7,
    TECMO_GAMEPLAY_AUDIO_BANK05_9FEC_CUE = 8
} TecmoGameplayAudioEvent;

/* A8D6 and A9C5 remain address-bound. ABF5 has only a bounded sequence-level
   correlation, not an impact/rim claim. */
typedef enum TecmoGameplayDmcClipId {
    TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT = 0,
    TECMO_GAMEPLAY_DMC_BANK05_A8D6_LONG = 1,
    TECMO_GAMEPLAY_DMC_BANK05_A9C5 = 2,
    TECMO_GAMEPLAY_DMC_LAYUP_SEQUENCE_ABF5 = 3,
    TECMO_GAMEPLAY_DMC_HELD_BALL_DRIBBLE = 4
} TecmoGameplayDmcClipId;

typedef struct TecmoGameplaySfxEffect {
    uint8_t id;
    uint32_t source_fingerprint;
    TecmoMusicChannelProgram channels[TECMO_GAMEPLAY_SFX_CHANNEL_COUNT];
} TecmoGameplaySfxEffect;

typedef struct TecmoGameplayDmcPool {
    uint32_t data_offset;
    uint32_t byte_count;
    uint32_t source_fingerprint;
} TecmoGameplayDmcPool;

typedef struct TecmoGameplayDmcClip {
    uint8_t id;
    uint8_t pool_index;
    uint8_t rate_index;
    uint32_t pool_offset;
    uint32_t byte_count;
    uint32_t trigger_fingerprint;
} TecmoGameplayDmcClip;

typedef struct TecmoGameplayAudioAsset {
    bool available;
    TecmoGameplaySfxEffect effects[TECMO_GAMEPLAY_SFX_EFFECT_COUNT];
    TecmoMusicVoice voices[TECMO_GAMEPLAY_SFX_VOICE_COUNT];
    uint16_t pitch_periods[TECMO_GAMEPLAY_SFX_PITCH_COUNT];
    TecmoMusicInstruction *instructions;
    TecmoGameplayDmcPool dmc_pools[TECMO_GAMEPLAY_DMC_POOL_COUNT];
    TecmoGameplayDmcClip dmc_clips[TECMO_GAMEPLAY_DMC_CLIP_COUNT];
    uint8_t *dmc_data;
    uint32_t revision_token;
    uint32_t sfx_payload_fingerprint;
    uint32_t dmc_payload_fingerprint;
    char status[160];
} TecmoGameplayAudioAsset;

typedef struct TecmoGameplayDmcState {
    uint32_t byte_index;
    uint32_t byte_count;
    uint32_t cycle_accumulator;
    uint16_t period_cycles;
    uint8_t pool_index;
    uint8_t shift_register;
    uint8_t bits_remaining;
    uint8_t output_level;
    bool active;
} TecmoGameplayDmcState;

typedef struct TecmoGameplayAudioPlayer {
    const TecmoGameplayAudioAsset *asset;
    TecmoMusicPlayer *music;
    TecmoMusicChannelState sfx_channels[TECMO_GAMEPLAY_SFX_CHANNEL_COUNT];
    uint64_t sample_tick_accumulator;
    uint64_t ticks_elapsed;
    uint16_t noise_lfsr;
    uint8_t current_sfx_id;
    uint8_t pending_sfx_id;
    bool sfx_pending;
    bool sfx_playing;
    bool render_guard_failed;
    TecmoGameplayDmcState dmc;
} TecmoGameplayAudioPlayer;

bool tecmo_gameplay_audio_asset_load(TecmoGameplayAudioAsset *asset,
                                     const char *project_root);
/* Strict explicit-pack variant used when a compound runtime contract must
   prove that gameplay visuals and audio came from the same container. */
bool tecmo_gameplay_audio_asset_load_from_pack(
    TecmoGameplayAudioAsset *asset,
    const char *asset_pack_path);
void tecmo_gameplay_audio_asset_shutdown(TecmoGameplayAudioAsset *asset);
void tecmo_gameplay_audio_player_init(TecmoGameplayAudioPlayer *player,
                                      const TecmoGameplayAudioAsset *asset,
                                      TecmoMusicPlayer *music);
bool tecmo_gameplay_audio_queue_event(TecmoGameplayAudioPlayer *player,
                                      TecmoGameplayAudioEvent event);
bool tecmo_gameplay_audio_queue_dmc_clip(TecmoGameplayAudioPlayer *player,
                                         TecmoGameplayDmcClipId clip_id);
bool tecmo_gameplay_audio_queue_game_music(TecmoGameplayAudioPlayer *player);
bool tecmo_gameplay_audio_queue_pregame_matchup_stinger(
    TecmoGameplayAudioPlayer *player);
void tecmo_gameplay_audio_set_game_music_enabled(
    TecmoGameplayAudioPlayer *player, bool enabled);
void tecmo_gameplay_audio_stop_all(TecmoGameplayAudioPlayer *player);
void tecmo_gameplay_audio_render_samples(TecmoGameplayAudioPlayer *player,
                                         int16_t *samples,
                                         size_t sample_count);
bool tecmo_gameplay_audio_self_test(const char *project_root,
                                    char *message,
                                    size_t message_size);

#endif
