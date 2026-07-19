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

static void render_silence(int16_t *samples, size_t sample_count)
{
    if (samples != NULL)
        memset(samples, 0, sample_count * sizeof(*samples));
}

void tecmo_audio_output_clear_gameplay_player(TecmoAudioOutput *output)
{
    if (output == NULL) return;
    output->gameplay_player = NULL;
    output->gameplay_asset = NULL;
}

bool tecmo_audio_output_select_gameplay_player(
    TecmoAudioOutput *output, TecmoGameplayAudioPlayer *gameplay_player)
{
    if (output == NULL || !output->initialized || output->player == NULL ||
        output->player->asset == NULL || !output->player->asset->available ||
        gameplay_player == NULL ||
        gameplay_player->asset == NULL ||
        !gameplay_player->asset->available ||
        gameplay_player->music != output->player)
        return false;
    output->gameplay_player = gameplay_player;
    output->gameplay_asset = gameplay_player->asset;
    return true;
}

TecmoAudioOutputRenderSource tecmo_audio_output_render_samples(
    TecmoAudioOutput *output, int16_t *samples, size_t sample_count)
{
    TecmoGameplayAudioPlayer *gameplay_player;
    if (sample_count > SIZE_MAX / sizeof(int16_t))
        return TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
    if (output == NULL || !output->initialized) {
        render_silence(samples, sample_count);
        return TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
    }
    gameplay_player = output->gameplay_player;
    if (gameplay_player != NULL) {
        if (gameplay_player->asset == output->gameplay_asset &&
            output->gameplay_asset != NULL &&
            output->gameplay_asset->available &&
            gameplay_player->music == output->player) {
            tecmo_gameplay_audio_render_samples(gameplay_player, samples,
                                                sample_count);
            return TECMO_AUDIO_OUTPUT_RENDER_GAMEPLAY;
        }
        tecmo_audio_output_clear_gameplay_player(output);
    }
    if (output->player != NULL && output->player->asset != NULL &&
        output->player->asset->available) {
        tecmo_music_render_samples(output->player, samples, sample_count);
        return TECMO_AUDIO_OUTPUT_RENDER_MUSIC;
    }
    render_silence(samples, sample_count);
    return TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
}

bool tecmo_audio_output_init(TecmoAudioOutput *output,
                             TecmoMusicPlayer *player)
{
    if (output == NULL) return false;
    memset(output, 0, sizeof(*output));
    output->initialized = true;
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
            (void)tecmo_audio_output_render_samples(
                output, backend->samples[i], AUDIO_BUFFER_SAMPLES);
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
    if (output == NULL || !output->active || output->platform == NULL) return;
    backend = (Win32AudioBackend *)output->platform;
    for (i = 0U; i < AUDIO_BUFFER_COUNT; ++i) {
        WAVEHDR *header = &backend->headers[i];
        if ((header->dwFlags & WHDR_DONE) == 0U) continue;
        (void)tecmo_audio_output_render_samples(
            output, backend->samples[i], AUDIO_BUFFER_SAMPLES);
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
    const size_t render_count = AUDIO_BUFFER_SAMPLES;
    const uint64_t tick_threshold =
        (uint64_t)TECMO_MUSIC_SAMPLE_RATE * TECMO_MUSIC_TICK_DENOMINATOR;
    const uint64_t render_numerator =
        (uint64_t)render_count * TECMO_MUSIC_TICK_NUMERATOR;
    TecmoMusicAsset music_asset;
    TecmoMusicAsset unavailable_music_asset;
    TecmoGameplayAudioAsset gameplay_asset;
    TecmoGameplayAudioAsset unavailable_gameplay_asset;
    TecmoMusicPlayer player;
    TecmoMusicPlayer other_player;
    TecmoMusicPlayer uninitialized_music_player;
    TecmoMusicPlayer unavailable_music_player;
    TecmoMusicPlayer bounds_music_before;
    TecmoMusicPlayer frozen_before;
    TecmoGameplayAudioPlayer gameplay_player;
    TecmoGameplayAudioPlayer other_gameplay_player;
    TecmoGameplayAudioPlayer unavailable_gameplay_player;
    TecmoGameplayAudioPlayer null_music_gameplay_player;
    TecmoGameplayAudioPlayer uninitialized_music_gameplay_player;
    TecmoGameplayAudioPlayer unavailable_music_gameplay_player;
    TecmoGameplayAudioPlayer bounds_gameplay_before;
    TecmoAudioOutput output;
    TecmoAudioOutput silent_output;
    TecmoAudioOutput null_music_output;
    TecmoAudioOutput unavailable_music_output;
    int16_t samples[AUDIO_BUFFER_SAMPLES];
    int16_t bounds_sentinel;
    uint64_t gameplay_accumulator;
    uint64_t music_accumulator;
    uint64_t music_ticks;
    bool selection_ok;
    bool once_only;
    bool switching_ok;
    bool fallback_ok;
    bool silence_ok;
    bool bounds_ok;
    bool lifecycle_ok;
    bool frozen;
    bool all_ok;

    memset(&music_asset, 0, sizeof(music_asset));
    music_asset.available = true;
    music_asset.sample_rate = TECMO_MUSIC_SAMPLE_RATE;
    music_asset.tick_numerator = TECMO_MUSIC_TICK_NUMERATOR;
    music_asset.tick_denominator = TECMO_MUSIC_TICK_DENOMINATOR;
    memset(&unavailable_music_asset, 0, sizeof(unavailable_music_asset));
    memset(&gameplay_asset, 0, sizeof(gameplay_asset));
    gameplay_asset.available = true;
    gameplay_asset.revision_token = 0x12345678U;
    memset(&unavailable_gameplay_asset, 0,
           sizeof(unavailable_gameplay_asset));

    tecmo_music_player_init(&player, &music_asset);
    tecmo_music_player_init(&other_player, &music_asset);
    memset(&uninitialized_music_player, 0,
           sizeof(uninitialized_music_player));
    tecmo_music_player_init(&unavailable_music_player,
                            &unavailable_music_asset);
    tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay_asset,
                                     &player);
    tecmo_gameplay_audio_player_init(&other_gameplay_player, &gameplay_asset,
                                     &other_player);
    tecmo_gameplay_audio_player_init(&unavailable_gameplay_player,
                                     &unavailable_gameplay_asset, &player);
    tecmo_gameplay_audio_player_init(&null_music_gameplay_player,
                                     &gameplay_asset, NULL);
    tecmo_gameplay_audio_player_init(&uninitialized_music_gameplay_player,
                                     &gameplay_asset,
                                     &uninitialized_music_player);
    tecmo_gameplay_audio_player_init(&unavailable_music_gameplay_player,
                                     &gameplay_asset,
                                     &unavailable_music_player);
    memset(&output, 0, sizeof(output));
    output.player = &player;

    selection_ok =
        !tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player) &&
        output.gameplay_player == NULL && output.gameplay_asset == NULL;
    output.initialized = true;
    selection_ok = selection_ok &&
        !tecmo_audio_output_select_gameplay_player(&output, NULL) &&
        !tecmo_audio_output_select_gameplay_player(
            &output, &unavailable_gameplay_player) &&
        !tecmo_audio_output_select_gameplay_player(
            &output, &other_gameplay_player) &&
        tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player);
    output.initialized = false;
    selection_ok = selection_ok &&
        !tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player) &&
        output.gameplay_player == &gameplay_player &&
        output.gameplay_asset == &gameplay_asset;
    output.initialized = true;

    selection_ok = tecmo_audio_output_init(&null_music_output, NULL) &&
        selection_ok && null_music_output.initialized &&
        !tecmo_audio_output_select_gameplay_player(
            &null_music_output, &null_music_gameplay_player) &&
        null_music_output.gameplay_player == NULL &&
        null_music_output.gameplay_asset == NULL;
    tecmo_audio_output_shutdown(&null_music_output);
    selection_ok = tecmo_audio_output_init(
                       &unavailable_music_output,
                       &uninitialized_music_player) &&
        selection_ok && unavailable_music_output.initialized &&
        !tecmo_audio_output_select_gameplay_player(
            &unavailable_music_output,
            &uninitialized_music_gameplay_player) &&
        unavailable_music_output.gameplay_player == NULL &&
        unavailable_music_output.gameplay_asset == NULL;
    tecmo_audio_output_shutdown(&unavailable_music_output);
    selection_ok = tecmo_audio_output_init(
                       &unavailable_music_output,
                       &unavailable_music_player) &&
        selection_ok && unavailable_music_output.initialized &&
        !tecmo_audio_output_select_gameplay_player(
            &unavailable_music_output, &unavailable_music_gameplay_player) &&
        unavailable_music_output.gameplay_player == NULL &&
        unavailable_music_output.gameplay_asset == NULL;
    tecmo_audio_output_shutdown(&unavailable_music_output);

    bounds_music_before = player;
    bounds_gameplay_before = gameplay_player;
    bounds_sentinel = (int16_t)0x5A5A;
    bounds_ok =
        tecmo_audio_output_render_samples(
            &output, &bounds_sentinel,
            SIZE_MAX / sizeof(int16_t) + 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE &&
        bounds_sentinel == (int16_t)0x5A5A &&
        memcmp(&player, &bounds_music_before, sizeof(player)) == 0 &&
        memcmp(&gameplay_player, &bounds_gameplay_before,
               sizeof(gameplay_player)) == 0 &&
        tecmo_audio_output_render_samples(
            &output, NULL, SIZE_MAX / sizeof(int16_t) + 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE &&
        memcmp(&player, &bounds_music_before, sizeof(player)) == 0 &&
        memcmp(&gameplay_player, &bounds_gameplay_before,
               sizeof(gameplay_player)) == 0 &&
        output.gameplay_player == &gameplay_player &&
        output.gameplay_asset == &gameplay_asset;
    memset(samples, 0x7F, sizeof(samples));
    once_only =
        tecmo_audio_output_render_samples(&output, samples, render_count) ==
            TECMO_AUDIO_OUTPUT_RENDER_GAMEPLAY &&
        player.sample_tick_accumulator == render_numerator % tick_threshold &&
        player.ticks_elapsed == render_numerator / tick_threshold &&
        gameplay_player.sample_tick_accumulator ==
            render_numerator % tick_threshold &&
        gameplay_player.ticks_elapsed == render_numerator / tick_threshold;

    gameplay_accumulator = gameplay_player.sample_tick_accumulator;
    tecmo_audio_output_clear_gameplay_player(&output);
    switching_ok =
        output.gameplay_player == NULL && output.gameplay_asset == NULL &&
        tecmo_audio_output_render_samples(&output, NULL, render_count) ==
            TECMO_AUDIO_OUTPUT_RENDER_MUSIC &&
        gameplay_player.sample_tick_accumulator == gameplay_accumulator &&
        player.sample_tick_accumulator ==
            (render_numerator * 2U) % tick_threshold &&
        player.ticks_elapsed == (render_numerator * 2U) / tick_threshold;

    selection_ok = selection_ok &&
        tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player) &&
        !tecmo_audio_output_select_gameplay_player(
            &output, &unavailable_gameplay_player) &&
        output.gameplay_player == &gameplay_player;
    gameplay_asset.available = false;
    music_accumulator = player.sample_tick_accumulator;
    music_ticks = player.ticks_elapsed;
    gameplay_accumulator = gameplay_player.sample_tick_accumulator;
    fallback_ok =
        tecmo_audio_output_render_samples(&output, NULL, 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_MUSIC &&
        output.gameplay_player == NULL && output.gameplay_asset == NULL &&
        gameplay_player.sample_tick_accumulator == gameplay_accumulator &&
        (player.sample_tick_accumulator != music_accumulator ||
         player.ticks_elapsed != music_ticks);

    memset(&silent_output, 0, sizeof(silent_output));
    memset(samples, 0x7F, sizeof(samples));
    silence_ok =
        tecmo_audio_output_render_samples(&silent_output, samples,
                                          render_count) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
    if (silence_ok) {
        size_t index;
        for (index = 0U; index < render_count; ++index) {
            if (samples[index] != 0) {
                silence_ok = false;
                break;
            }
        }
    }
    silent_output.initialized = true;
    silent_output.player = &unavailable_music_player;
    silence_ok = silence_ok &&
        tecmo_audio_output_render_samples(&silent_output, NULL, 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE &&
        tecmo_audio_output_render_samples(NULL, NULL, 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE;

    gameplay_asset.available = true;
    selection_ok = selection_ok &&
        tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player);
    tecmo_audio_output_shutdown(&output);
    lifecycle_ok = !output.initialized && output.platform == NULL &&
        output.player == NULL &&
        output.gameplay_player == NULL && output.gameplay_asset == NULL &&
        !output.active && !output.silent_fallback &&
        !tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player) &&
        output.gameplay_player == NULL && output.gameplay_asset == NULL &&
        tecmo_audio_output_render_samples(&output, NULL, 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_SILENCE;

    memset(&player, 0, sizeof(player));
    player.sample_tick_accumulator = 1234567U;
    player.ticks_elapsed = 89U;
    player.current_track_id = 7U;
    player.pending_track_id = 5U;
    player.playing = true;
    player.track_pending = true;
    memset(&frozen_before, 0, sizeof(frozen_before));
    frozen_before = player;
    memset(&output, 0, sizeof(output));
    output.initialized = true;
    output.player = &player;
    output.silent_fallback = true;
    tecmo_audio_output_service(&output);
    frozen = memcmp(&player, &frozen_before, sizeof(player)) == 0;
    all_ok = selection_ok && once_only && switching_ok && fallback_ok &&
             silence_ok && bounds_ok && lifecycle_ok && frozen;
    if (message != NULL && message_size > 0U) {
        (void)snprintf(message, message_size,
                       "output=%s ring=%ux%u",
                       all_ok ? "frozen-fallback" : "fail",
                       AUDIO_BUFFER_COUNT, AUDIO_BUFFER_SAMPLES);
    }
    return all_ok;
}
