#ifndef TECMO_MUSIC_H
#define TECMO_MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_MUSIC_TRACK_COUNT 4U
#define TECMO_MUSIC_CHANNEL_COUNT 4U
#define TECMO_MUSIC_VOICE_COUNT 37U
#define TECMO_MUSIC_PITCH_COUNT 75U
#define TECMO_MUSIC_LOOP_COUNT 1U
#define TECMO_MUSIC_CALL_DEPTH 16U
#define TECMO_MUSIC_SAMPLE_RATE 44100U
#define TECMO_MUSIC_TICK_NUMERATOR 39375000U
#define TECMO_MUSIC_TICK_DENOMINATOR 655171U
#define TECMO_MUSIC_PAYLOAD_SIZE 36784U
#define TECMO_MUSIC_INSTRUCTION_COUNT 2251U

typedef enum TecmoMusicTrackId {
    TECMO_MUSIC_TRACK_GAMEPLAY = 5,
    TECMO_MUSIC_TRACK_PRESENTATION = 6,
    TECMO_MUSIC_TRACK_OPENING = 7,
    TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER = 8,
    /* Source-compatible name retained for older callers. */
    TECMO_MUSIC_TRACK_PERIOD_STINGER = TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER
} TecmoMusicTrackId;

typedef enum TecmoMusicInstructionType {
    TECMO_MUSIC_NOTE = 1,
    TECMO_MUSIC_SET_VOICE = 2,
    TECMO_MUSIC_LEGATO = 3,
    TECMO_MUSIC_PITCH_DELTA = 4,
    TECMO_MUSIC_END = 5,
    TECMO_MUSIC_REST = 6,
    TECMO_MUSIC_LOOP = 7,
    TECMO_MUSIC_BIND_PHRASES = 8,
    TECMO_MUSIC_CALL = 9,
    TECMO_MUSIC_RETURN = 10
} TecmoMusicInstructionType;

typedef struct TecmoMusicInstruction {
    TecmoMusicInstructionType type;
    uint8_t value8;
    uint16_t value16;
    uint32_t next;
    uint32_t target;
    int16_t signed_value;
    uint16_t loop_slot;
} TecmoMusicInstruction;

typedef struct TecmoMusicVoice {
    uint8_t duty_and_timing;
    uint8_t initial_volume;
    uint8_t sweep;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain_ticks;
    uint8_t release;
    uint8_t peak_and_sustain_volume;
} TecmoMusicVoice;

typedef struct TecmoMusicChannelProgram {
    uint32_t first_instruction;
    uint32_t instruction_count;
} TecmoMusicChannelProgram;

typedef struct TecmoMusicTrack {
    uint8_t id;
    uint32_t source_fingerprint;
    TecmoMusicChannelProgram channels[TECMO_MUSIC_CHANNEL_COUNT];
} TecmoMusicTrack;

typedef struct TecmoMusicAsset {
    bool available;
    TecmoMusicTrack tracks[TECMO_MUSIC_TRACK_COUNT];
    TecmoMusicVoice voices[TECMO_MUSIC_VOICE_COUNT];
    uint16_t pitch_periods[TECMO_MUSIC_PITCH_COUNT];
    TecmoMusicInstruction *instructions;
    uint32_t instruction_count;
    uint32_t sample_rate;
    uint32_t tick_numerator;
    uint32_t tick_denominator;
    uint32_t payload_fingerprint;
    char status[160];
} TecmoMusicAsset;

typedef struct TecmoMusicChannelState {
    uint32_t pc;
    uint32_t call_stack[TECMO_MUSIC_CALL_DEPTH];
    uint16_t loop_remaining[TECMO_MUSIC_LOOP_COUNT];
    uint16_t duration_ticks;
    uint16_t period;
    int32_t pitch_delta;
    uint8_t call_depth;
    uint8_t voice_index;
    uint8_t note;
    uint8_t volume;
    uint8_t envelope_phase;
    uint8_t attack_timer;
    uint8_t decay_timer;
    uint8_t sustain_timer;
    uint16_t release_timer;
    bool active;
    bool note_on;
    bool legato_next;
    double phase;
    double noise_phase;
} TecmoMusicChannelState;

typedef struct TecmoMusicPlayer {
    const TecmoMusicAsset *asset;
    TecmoMusicChannelState channels[TECMO_MUSIC_CHANNEL_COUNT];
    uint64_t sample_tick_accumulator;
    uint64_t ticks_elapsed;
    uint16_t noise_lfsr;
    uint8_t current_track_id;
    uint8_t pending_track_id;
    bool playing;
    bool track_pending;
    bool game_music_enabled;
    bool opening_queued;
    bool render_guard_failed;
} TecmoMusicPlayer;

bool tecmo_music_asset_load(TecmoMusicAsset *asset, const char *project_root);
void tecmo_music_asset_shutdown(TecmoMusicAsset *asset);
void tecmo_music_player_init(TecmoMusicPlayer *player,
                             const TecmoMusicAsset *asset);
bool tecmo_music_queue_track(TecmoMusicPlayer *player, uint8_t track_id);
bool tecmo_music_queue_opening_once(TecmoMusicPlayer *player);
void tecmo_music_set_game_music_enabled(TecmoMusicPlayer *player,
                                        bool enabled);
void tecmo_music_stop(TecmoMusicPlayer *player);
void tecmo_music_render_samples(TecmoMusicPlayer *player,
                                int16_t *samples,
                                size_t sample_count);
int16_t tecmo_music_render_sample_with_overrides(TecmoMusicPlayer *player,
                                                 uint8_t channel_mask);
bool tecmo_music_self_test(const char *project_root,
                           char *message,
                           size_t message_size);

#endif
