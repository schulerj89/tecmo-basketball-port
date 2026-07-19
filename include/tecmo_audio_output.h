#ifndef TECMO_AUDIO_OUTPUT_H
#define TECMO_AUDIO_OUTPUT_H

#include "tecmo_music.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct TecmoAudioOutput {
    void *platform;
    TecmoMusicPlayer *player;
    bool active;
    bool silent_fallback;
    char status[96];
} TecmoAudioOutput;

bool tecmo_audio_output_init(TecmoAudioOutput *output,
                             TecmoMusicPlayer *player);
void tecmo_audio_output_service(TecmoAudioOutput *output);
void tecmo_audio_output_shutdown(TecmoAudioOutput *output);
bool tecmo_audio_output_self_test(char *message, size_t message_size);

#endif
