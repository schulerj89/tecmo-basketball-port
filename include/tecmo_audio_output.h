#ifndef TECMO_AUDIO_OUTPUT_H
#define TECMO_AUDIO_OUTPUT_H

#include "tecmo_gameplay_audio.h"
#include "tecmo_music.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum TecmoAudioOutputRenderSource {
    TECMO_AUDIO_OUTPUT_RENDER_SILENCE = 0,
    TECMO_AUDIO_OUTPUT_RENDER_MUSIC = 1,
    TECMO_AUDIO_OUTPUT_RENDER_GAMEPLAY = 2
} TecmoAudioOutputRenderSource;

typedef struct TecmoAudioOutput {
    void *platform;
    TecmoMusicPlayer *player;
    TecmoGameplayAudioPlayer *gameplay_player;
    const TecmoGameplayAudioAsset *gameplay_asset;
    bool active;
    bool silent_fallback;
    char status[96];
} TecmoAudioOutput;

bool tecmo_audio_output_init(TecmoAudioOutput *output,
                             TecmoMusicPlayer *player);
/* The gameplay player and asset are borrowed. Clear the selection before
   either object's storage lifetime ends. A rejected selection leaves the
   current source unchanged. A selected asset that later becomes unavailable
   is detached on the next render and falls back to the original music player. */
bool tecmo_audio_output_select_gameplay_player(
    TecmoAudioOutput *output, TecmoGameplayAudioPlayer *gameplay_player);
void tecmo_audio_output_clear_gameplay_player(TecmoAudioOutput *output);
/* Device-independent render path used by both waveOut prefill and refill.
   A NULL sample sink still advances a valid selected source exactly once. */
TecmoAudioOutputRenderSource tecmo_audio_output_render_samples(
    TecmoAudioOutput *output, int16_t *samples, size_t sample_count);
void tecmo_audio_output_service(TecmoAudioOutput *output);
void tecmo_audio_output_shutdown(TecmoAudioOutput *output);
bool tecmo_audio_output_self_test(char *message, size_t message_size);

#endif
