#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tecmo_audio_output.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_BUFFER_COUNT 8U
#define AUDIO_BUFFER_SAMPLES 1024U

_Static_assert(AUDIO_BUFFER_COUNT == 8U,
               "native waveOut must retain the bounded eight-buffer ring");
_Static_assert(AUDIO_BUFFER_SAMPLES == 1024U,
               "native waveOut must retain the bounded buffer size");

#ifdef _WIN32
typedef struct Win32AudioBackend {
    HWAVEOUT device;
    WAVEHDR headers[AUDIO_BUFFER_COUNT];
    int16_t samples[AUDIO_BUFFER_COUNT][AUDIO_BUFFER_SAMPLES];
    unsigned prepared_count;
} Win32AudioBackend;

static void close_backend(Win32AudioBackend *backend)
{
    unsigned i;
    if (backend == NULL) return;
    if (backend->device != NULL) {
        (void)waveOutReset(backend->device);
        for (i = 0U; i < backend->prepared_count; ++i)
            (void)waveOutUnprepareHeader(backend->device, &backend->headers[i],
                                         sizeof(backend->headers[i]));
        (void)waveOutClose(backend->device);
    }
    free(backend);
}
#endif

bool tecmo_audio_output_init(TecmoAudioOutput *output,
                             TecmoMusicPlayer *player)
{
    if (output == NULL) return false;
    memset(output, 0, sizeof(*output));
    output->player = player;
    if (player == NULL || player->asset == NULL || !player->asset->available) {
        output->silent_fallback = true;
        (void)snprintf(output->status, sizeof(output->status),
                       "silent: TMUS-1 unavailable");
        return true;
    }
#ifdef _WIN32
    {
        Win32AudioBackend *backend = (Win32AudioBackend *)calloc(1U, sizeof(*backend));
        WAVEFORMATEX format;
        unsigned i;
        MMRESULT result;
        if (backend == NULL) {
            output->silent_fallback = true;
            (void)snprintf(output->status, sizeof(output->status),
                           "silent: audio allocation failed");
            return true;
        }
        memset(&format, 0, sizeof(format));
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1U;
        format.nSamplesPerSec = TECMO_MUSIC_SAMPLE_RATE;
        format.wBitsPerSample = 16U;
        format.nBlockAlign = 2U;
        format.nAvgBytesPerSec = TECMO_MUSIC_SAMPLE_RATE * 2U;
        result = waveOutOpen(&backend->device, WAVE_MAPPER, &format,
                             0U, 0U, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            close_backend(backend);
            output->silent_fallback = true;
            (void)snprintf(output->status, sizeof(output->status),
                           "silent: waveOut device unavailable");
            return true;
        }
        for (i = 0U; i < AUDIO_BUFFER_COUNT; ++i) {
            WAVEHDR *header = &backend->headers[i];
            tecmo_music_render_samples(player, backend->samples[i],
                                       AUDIO_BUFFER_SAMPLES);
            header->lpData = (LPSTR)backend->samples[i];
            header->dwBufferLength = sizeof(backend->samples[i]);
            if (waveOutPrepareHeader(backend->device, header,
                                     sizeof(*header)) != MMSYSERR_NOERROR) {
                close_backend(backend);
                output->silent_fallback = true;
                (void)snprintf(output->status, sizeof(output->status),
                               "silent: waveOut buffer preparation failed");
                return true;
            }
            ++backend->prepared_count;
            if (waveOutWrite(backend->device, header,
                             sizeof(*header)) != MMSYSERR_NOERROR) {
                close_backend(backend);
                output->silent_fallback = true;
                (void)snprintf(output->status, sizeof(output->status),
                               "silent: waveOut queue failed");
                return true;
            }
        }
        output->platform = backend;
        output->active = true;
        (void)snprintf(output->status, sizeof(output->status),
                       "waveOut PCM 44100 Hz mono");
        return true;
    }
#else
    output->silent_fallback = true;
    (void)snprintf(output->status, sizeof(output->status),
                   "silent: platform output unavailable");
    return true;
#endif
}

void tecmo_audio_output_service(TecmoAudioOutput *output)
{
#ifdef _WIN32
    Win32AudioBackend *backend;
    unsigned i;
    if (output == NULL || !output->active || output->platform == NULL ||
        output->player == NULL) return;
    backend = (Win32AudioBackend *)output->platform;
    for (i = 0U; i < AUDIO_BUFFER_COUNT; ++i) {
        WAVEHDR *header = &backend->headers[i];
        if ((header->dwFlags & WHDR_DONE) == 0U) continue;
        tecmo_music_render_samples(output->player, backend->samples[i],
                                   AUDIO_BUFFER_SAMPLES);
        if (waveOutWrite(backend->device, header,
                         sizeof(*header)) != MMSYSERR_NOERROR) {
            output->active = false;
            output->silent_fallback = true;
            (void)snprintf(output->status, sizeof(output->status),
                           "silent: waveOut refill failed");
            return;
        }
    }
#else
    (void)output;
#endif
}

void tecmo_audio_output_shutdown(TecmoAudioOutput *output)
{
    if (output == NULL) return;
#ifdef _WIN32
    close_backend((Win32AudioBackend *)output->platform);
#endif
    memset(output, 0, sizeof(*output));
}

bool tecmo_audio_output_self_test(char *message, size_t message_size)
{
    TecmoMusicPlayer player;
    TecmoMusicPlayer before;
    TecmoAudioOutput output;
    bool frozen;
    memset(&player, 0, sizeof(player));
    player.sample_tick_accumulator = 1234567U;
    player.ticks_elapsed = 89U;
    player.current_track_id = 7U;
    player.pending_track_id = 5U;
    player.playing = true;
    player.track_pending = true;
    memset(&before, 0, sizeof(before));
    before = player;
    memset(&output, 0, sizeof(output));
    output.player = &player;
    output.silent_fallback = true;
    tecmo_audio_output_service(&output);
    frozen = memcmp(&player, &before, sizeof(player)) == 0;
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size,
                       "output=%s ring=%ux%u",
                       frozen ? "frozen-fallback" : "fail",
                       AUDIO_BUFFER_COUNT, AUDIO_BUFFER_SAMPLES);
    }
    return frozen;
}
