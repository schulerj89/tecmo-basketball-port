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
#include <stdint.h>
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

static bool music_source_is_valid(const TecmoAudioOutput *output)
{
    return output != NULL && output->player != NULL &&
           output->player->asset != NULL &&
           output->player->asset->available &&
           output->player->asset->asset_pack_path[0] != '\0';
}

static bool gameplay_source_is_valid(const TecmoAudioOutput *output)
{
    return music_source_is_valid(output) &&
           output->gameplay_player != NULL &&
           output->gameplay_asset != NULL &&
           output->gameplay_asset->available &&
           output->gameplay_asset->asset_pack_path[0] != '\0' &&
           output->gameplay_player->asset == output->gameplay_asset &&
           output->gameplay_player->music == output->player &&
           strcmp(output->gameplay_asset->asset_pack_path,
                  output->player->asset->asset_pack_path) == 0;
}

static bool frontend_source_is_valid(const TecmoAudioOutput *output)
{
    return music_source_is_valid(output) &&
           output->frontend_player != NULL &&
           output->frontend_asset != NULL &&
           output->frontend_asset->sfx.available &&
           output->frontend_asset->sfx.asset_pack_path[0] != '\0' &&
           output->frontend_player->pack_identity_valid &&
           output->frontend_player->asset == output->frontend_asset &&
           output->frontend_player->sfx.asset ==
               &output->frontend_asset->sfx &&
           output->frontend_player->sfx.music == output->player &&
           strcmp(output->frontend_asset->sfx.asset_pack_path,
                  output->player->asset->asset_pack_path) == 0;
}

typedef struct TecmoAudioOutputCheckpoint {
    TecmoMusicPlayer *music_target;
    TecmoMusicPlayer music;
    TecmoGameplayAudioPlayer *gameplay_target;
    TecmoGameplayAudioPlayer gameplay;
    TecmoFrontendAudioPlayer *frontend_target;
    TecmoFrontendAudioPlayer frontend;
} TecmoAudioOutputCheckpoint;

static void checkpoint_capture(const TecmoAudioOutput *output,
                               TecmoAudioOutputCheckpoint *checkpoint)
{
    if (checkpoint == NULL) return;
    memset(checkpoint, 0, sizeof(*checkpoint));
    if (output == NULL) return;
    if (output->player != NULL) {
        checkpoint->music_target = output->player;
        checkpoint->music = *output->player;
    }
    if (gameplay_source_is_valid(output) &&
        (void *)output->gameplay_player !=
            (void *)checkpoint->music_target) {
        checkpoint->gameplay_target = output->gameplay_player;
        checkpoint->gameplay = *output->gameplay_player;
    }
    if (frontend_source_is_valid(output) &&
        (void *)output->frontend_player !=
            (void *)checkpoint->music_target &&
        (void *)output->frontend_player !=
            (void *)checkpoint->gameplay_target) {
        checkpoint->frontend_target = output->frontend_player;
        checkpoint->frontend = *output->frontend_player;
    }
}

static void checkpoint_restore(const TecmoAudioOutputCheckpoint *checkpoint)
{
    if (checkpoint == NULL) return;
    if (checkpoint->music_target != NULL)
        *checkpoint->music_target = checkpoint->music;
    if (checkpoint->gameplay_target != NULL)
        *checkpoint->gameplay_target = checkpoint->gameplay;
    if (checkpoint->frontend_target != NULL)
        *checkpoint->frontend_target = checkpoint->frontend;
}

void tecmo_audio_output_clear_frontend_player(TecmoAudioOutput *output)
{
    if (output == NULL) return;
    output->frontend_player = NULL;
    output->frontend_asset = NULL;
}

bool tecmo_audio_output_select_frontend_player(
    TecmoAudioOutput *output, TecmoFrontendAudioPlayer *frontend_player,
    const TecmoFrontendAudioAsset *frontend_asset)
{
    if (output == NULL || !output->initialized || output->player == NULL ||
        output->player->asset == NULL || !output->player->asset->available ||
        frontend_player == NULL || frontend_asset == NULL ||
        !frontend_asset->sfx.available ||
        !frontend_player->pack_identity_valid ||
        frontend_player->asset != frontend_asset ||
        frontend_player->sfx.asset != &frontend_asset->sfx ||
        frontend_player->sfx.music != output->player ||
        frontend_asset->sfx.asset_pack_path[0] == '\0' ||
        output->player->asset->asset_pack_path[0] == '\0' ||
        strcmp(frontend_asset->sfx.asset_pack_path,
               output->player->asset->asset_pack_path) != 0)
        return false;
    output->frontend_player = frontend_player;
    output->frontend_asset = frontend_asset;
    return true;
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
        gameplay_player->music != output->player ||
        gameplay_player->asset->asset_pack_path[0] == '\0' ||
        output->player->asset->asset_pack_path[0] == '\0' ||
        strcmp(gameplay_player->asset->asset_pack_path,
               output->player->asset->asset_pack_path) != 0)
        return false;
    output->gameplay_player = gameplay_player;
    output->gameplay_asset = gameplay_player->asset;
    return true;
}

TecmoAudioOutputRenderSource tecmo_audio_output_render_samples(
    TecmoAudioOutput *output, int16_t *samples, size_t sample_count)
{
    TecmoGameplayAudioPlayer *gameplay_player;
    TecmoFrontendAudioPlayer *frontend_player;
    if (sample_count > SIZE_MAX / sizeof(int16_t))
        return TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
    if (output == NULL || !output->initialized) {
        render_silence(samples, sample_count);
        return TECMO_AUDIO_OUTPUT_RENDER_SILENCE;
    }
    frontend_player = output->frontend_player;
    if (frontend_player != NULL) {
        if (output->frontend_asset == NULL ||
            !output->frontend_asset->sfx.available ||
            !frontend_player->pack_identity_valid ||
            frontend_player->asset != output->frontend_asset ||
            frontend_player->sfx.asset !=
                &output->frontend_asset->sfx ||
            frontend_player->sfx.music != output->player ||
            output->player == NULL || output->player->asset == NULL ||
            !output->player->asset->available ||
            output->frontend_asset->sfx.asset_pack_path[0] == '\0' ||
            output->player->asset->asset_pack_path[0] == '\0' ||
            strcmp(output->frontend_asset->sfx.asset_pack_path,
                   output->player->asset->asset_pack_path) != 0) {
            tecmo_audio_output_clear_frontend_player(output);
        } else if (tecmo_frontend_audio_is_active(frontend_player)) {
            tecmo_frontend_audio_render_samples(
                frontend_player, samples, sample_count);
            return TECMO_AUDIO_OUTPUT_RENDER_FRONTEND;
        }
    }
    gameplay_player = output->gameplay_player;
    if (gameplay_player != NULL) {
        if (gameplay_source_is_valid(output)) {
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

/* Portable test seam for the device transactions.  A real waveOut submit
   calls the same render path, then either commits the borrowed player state
   when the API accepts the buffer or restores only that refill checkpoint
   when the API rejects it. */
static bool test_refill_transaction(TecmoAudioOutput *output,
                                    int16_t *samples, size_t sample_count,
                                    bool accepted)
{
    TecmoAudioOutputCheckpoint checkpoint;
    checkpoint_capture(output, &checkpoint);
    (void)tecmo_audio_output_render_samples(output, samples, sample_count);
    if (!accepted) checkpoint_restore(&checkpoint);
    return accepted;
}

static bool test_initial_transaction(TecmoAudioOutput *output,
                                     int16_t *samples, size_t sample_count,
                                     int fail_after_render)
{
    TecmoAudioOutputCheckpoint checkpoint;
    unsigned index;
    checkpoint_capture(output, &checkpoint);
    for (index = 0U; index < AUDIO_BUFFER_COUNT; ++index) {
        (void)tecmo_audio_output_render_samples(
            output, samples, sample_count);
        if (fail_after_render >= 0 &&
            (unsigned)fail_after_render == index) {
            checkpoint_restore(&checkpoint);
            return false;
        }
    }
    return true;
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
        TecmoAudioOutputCheckpoint initial_checkpoint;
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
        checkpoint_capture(output, &initial_checkpoint);
        for (i = 0U; i < AUDIO_BUFFER_COUNT; ++i) {
            WAVEHDR *header = &backend->headers[i];
            (void)tecmo_audio_output_render_samples(
                output, backend->samples[i], AUDIO_BUFFER_SAMPLES);
            header->lpData = (LPSTR)backend->samples[i];
            header->dwBufferLength = sizeof(backend->samples[i]);
            if (waveOutPrepareHeader(backend->device, header,
                                     sizeof(*header)) != MMSYSERR_NOERROR) {
                checkpoint_restore(&initial_checkpoint);
                close_backend(backend);
                output->silent_fallback = true;
                (void)snprintf(output->status, sizeof(output->status),
                               "silent: waveOut buffer preparation failed");
                return true;
            }
            ++backend->prepared_count;
            if (waveOutWrite(backend->device, header,
                             sizeof(*header)) != MMSYSERR_NOERROR) {
                checkpoint_restore(&initial_checkpoint);
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
        TecmoAudioOutputCheckpoint refill_checkpoint;
        if ((header->dwFlags & WHDR_DONE) == 0U) continue;
        checkpoint_capture(output, &refill_checkpoint);
        (void)tecmo_audio_output_render_samples(
            output, backend->samples[i], AUDIO_BUFFER_SAMPLES);
        if (waveOutWrite(backend->device, header,
                         sizeof(*header)) != MMSYSERR_NOERROR) {
            checkpoint_restore(&refill_checkpoint);
            /* Keep the device and previously accepted queued headers alive.
               The failed refill is restored and no further refill is
               attempted; shutdown owns the eventual reset/close. */
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
    TecmoGameplayAudioAsset distinct_gameplay_asset;
    TecmoGameplayAudioAsset unavailable_gameplay_asset;
    TecmoFrontendAudioAsset frontend_asset;
    TecmoFrontendAudioPlayer frontend_player;
    TecmoMusicInstruction frontend_end_instruction;
    TecmoMusicPlayer player;
    TecmoMusicPlayer other_player;
    TecmoMusicPlayer uninitialized_music_player;
    TecmoMusicPlayer unavailable_music_player;
    TecmoMusicPlayer bounds_music_before;
    TecmoMusicPlayer frozen_before;
    TecmoGameplayAudioPlayer gameplay_player;
    TecmoGameplayAudioPlayer distinct_gameplay_player;
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
    bool transaction_ok;
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
    (void)snprintf(music_asset.asset_pack_path,
                   sizeof(music_asset.asset_pack_path),
                   "canonical-pack.assetpack");
    (void)snprintf(gameplay_asset.asset_pack_path,
                   sizeof(gameplay_asset.asset_pack_path),
                   "canonical-pack.assetpack");
    distinct_gameplay_asset = gameplay_asset;
    (void)snprintf(distinct_gameplay_asset.asset_pack_path,
                   sizeof(distinct_gameplay_asset.asset_pack_path),
                   "distinct-container.assetpack");
    memset(&unavailable_gameplay_asset, 0,
           sizeof(unavailable_gameplay_asset));
    memset(&frontend_asset, 0, sizeof(frontend_asset));
    memset(&frontend_end_instruction, 0, sizeof(frontend_end_instruction));
    frontend_asset.sfx.available = true;
    frontend_asset.sfx.effect_count = 1U;
    frontend_asset.sfx.effects[0].id = 8U;
    frontend_asset.sfx.instruction_count = 1U;
    frontend_asset.sfx.instructions = &frontend_end_instruction;
    frontend_end_instruction.type = TECMO_MUSIC_END;
    frontend_asset.title_confirm_sfx_id = 8U;
    (void)snprintf(frontend_asset.sfx.asset_pack_path,
                   sizeof(frontend_asset.sfx.asset_pack_path),
                   "canonical-pack.assetpack");

    tecmo_music_player_init(&player, &music_asset);
    tecmo_music_player_init(&other_player, &music_asset);
    memset(&uninitialized_music_player, 0,
           sizeof(uninitialized_music_player));
    tecmo_music_player_init(&unavailable_music_player,
                            &unavailable_music_asset);
    tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay_asset,
                                     &player);
    tecmo_gameplay_audio_player_init(&distinct_gameplay_player,
                                     &distinct_gameplay_asset, &player);
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
    tecmo_frontend_audio_player_init(&frontend_player, &frontend_asset,
                                     &player);
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
    selection_ok = selection_ok &&
        !tecmo_audio_output_select_gameplay_player(
            &output, &distinct_gameplay_player) &&
        output.gameplay_player == &gameplay_player &&
        output.gameplay_asset == &gameplay_asset;
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
    gameplay_asset.available = true;
    selection_ok = selection_ok &&
        tecmo_audio_output_select_gameplay_player(&output,
                                                  &gameplay_player);
    (void)snprintf(gameplay_asset.asset_pack_path,
                   sizeof(gameplay_asset.asset_pack_path),
                   "mutated-container.assetpack");
    music_accumulator = player.sample_tick_accumulator;
    music_ticks = player.ticks_elapsed;
    gameplay_accumulator = gameplay_player.sample_tick_accumulator;
    fallback_ok = fallback_ok &&
        tecmo_audio_output_render_samples(&output, NULL, 1U) ==
            TECMO_AUDIO_OUTPUT_RENDER_MUSIC &&
        output.gameplay_player == NULL && output.gameplay_asset == NULL &&
        gameplay_player.sample_tick_accumulator == gameplay_accumulator &&
        (player.sample_tick_accumulator != music_accumulator ||
         player.ticks_elapsed != music_ticks);
    (void)snprintf(gameplay_asset.asset_pack_path,
                   sizeof(gameplay_asset.asset_pack_path),
                   "canonical-pack.assetpack");

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
    player.asset = &music_asset;
    player.game_music_enabled = true;
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
    transaction_ok = true;
    {
        TecmoMusicPlayer initial_state = player;
        transaction_ok = !test_initial_transaction(
                             &output, samples, render_count, 0) &&
                         memcmp(&player, &initial_state,
                                sizeof(player)) == 0;
        transaction_ok = transaction_ok &&
            !test_initial_transaction(&output, samples, render_count, 4) &&
            memcmp(&player, &initial_state, sizeof(player)) == 0;
        transaction_ok = transaction_ok &&
            test_initial_transaction(&output, samples, render_count, -1) &&
            memcmp(&player, &initial_state, sizeof(player)) != 0;
        {
            TecmoMusicPlayer accepted_state = player;
            transaction_ok = transaction_ok &&
                test_refill_transaction(&output, samples, render_count,
                                        true) &&
                memcmp(&player, &accepted_state, sizeof(player)) != 0;
            accepted_state = player;
            transaction_ok = transaction_ok &&
                !test_refill_transaction(&output, samples, render_count,
                                         false) &&
                memcmp(&player, &accepted_state, sizeof(player)) == 0;
        }
        {
            TecmoGameplayAudioPlayer gameplay_before;
            TecmoGameplayAudioPlayer gameplay_accepted;
            TecmoMusicPlayer music_accepted;
            output.frontend_player = NULL;
            output.frontend_asset = NULL;
            transaction_ok = transaction_ok &&
                tecmo_audio_output_select_gameplay_player(
                    &output, &gameplay_player);
            gameplay_before = gameplay_player;
            transaction_ok = transaction_ok &&
                test_refill_transaction(&output, samples, render_count,
                                        true) &&
                memcmp(&gameplay_player, &gameplay_before,
                       sizeof(gameplay_player)) != 0;
            gameplay_accepted = gameplay_player;
            music_accepted = player;
            gameplay_before = gameplay_player;
            transaction_ok = transaction_ok &&
                !test_refill_transaction(&output, samples, render_count,
                                         false) &&
                memcmp(&gameplay_player, &gameplay_before,
                       sizeof(gameplay_player)) == 0 &&
                memcmp(&player, &music_accepted, sizeof(player)) == 0;
            (void)gameplay_accepted;
        }
        {
            TecmoMusicPlayer music_before_invalid;
            TecmoGameplayAudioPlayer gameplay_before_invalid;
            output.frontend_player = NULL;
            output.frontend_asset = NULL;
            transaction_ok = transaction_ok &&
                tecmo_audio_output_select_gameplay_player(
                    &output, &gameplay_player);
            (void)snprintf(gameplay_asset.asset_pack_path,
                           sizeof(gameplay_asset.asset_pack_path),
                           "mutated-container.assetpack");
            music_before_invalid = player;
            gameplay_before_invalid = gameplay_player;
            transaction_ok = transaction_ok &&
                !test_refill_transaction(&output, samples, render_count,
                                         false) &&
                memcmp(&player, &music_before_invalid,
                       sizeof(player)) == 0 &&
                memcmp(&gameplay_player, &gameplay_before_invalid,
                       sizeof(gameplay_player)) == 0 &&
                output.gameplay_player == NULL &&
                output.gameplay_asset == NULL;
            (void)snprintf(gameplay_asset.asset_pack_path,
                           sizeof(gameplay_asset.asset_pack_path),
                           "canonical-pack.assetpack");
        }
        {
            TecmoFrontendAudioPlayer frontend_before;
            TecmoFrontendAudioPlayer frontend_accepted;
            TecmoMusicPlayer music_accepted;
            transaction_ok = transaction_ok &&
                tecmo_audio_output_select_frontend_player(
                    &output, &frontend_player, &frontend_asset) &&
                tecmo_frontend_audio_queue_title_confirm(&frontend_player);
            output.gameplay_player = &frontend_player.sfx;
            output.gameplay_asset = &frontend_asset.sfx;
            frontend_before = frontend_player;
            transaction_ok = transaction_ok &&
                test_refill_transaction(&output, samples, render_count,
                                        true) &&
                memcmp(&frontend_player, &frontend_before,
                       sizeof(frontend_player)) != 0;
            frontend_accepted = frontend_player;
            music_accepted = player;
            transaction_ok = transaction_ok &&
                tecmo_frontend_audio_queue_title_confirm(&frontend_player);
            frontend_before = frontend_player;
            transaction_ok = transaction_ok &&
                !test_refill_transaction(&output, samples, render_count,
                                         false) &&
                memcmp(&frontend_player, &frontend_before,
                       sizeof(frontend_player)) == 0 &&
                memcmp(&player, &music_accepted, sizeof(player)) == 0;
            (void)frontend_accepted;
        }
    }
    frozen_before = player;
    tecmo_audio_output_service(&output);
    frozen = memcmp(&player, &frozen_before, sizeof(player)) == 0;
    frozen = frozen && transaction_ok;
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
