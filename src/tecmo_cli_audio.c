#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "asset_pack/tecmo_asset_pack_gameplay_audio.h"
#include "asset_pack/tecmo_asset_pack_music.h"
#include "asset_pack/tecmo_asset_pack_util.h"
#include "tecmo_audio_output.h"
#include "tecmo_frontend_audio.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_music.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

#define AUDIO_PROOF_CHUNK_SAMPLES 1024U
#define AUDIO_PROOF_FIXED_SAMPLES 4096U
#define AUDIO_PROOF_MAX_DMC_DRAIN_SAMPLES 200000U
#define AUDIO_PROOF_TMUS7_DRAIN_LIMIT 3000000U

_Static_assert(AUDIO_PROOF_TMUS7_DRAIN_LIMIT > 2000000U,
               "TMUS7 proof drain must exceed the 2M-sample review bound");

typedef struct TecmoAudioProof {
    int16_t *samples;
    size_t sample_count;
    size_t sample_capacity;
    FILE *events;
    uint32_t pcm_fingerprint;
    uint32_t event_fingerprint;
    uint64_t event_bytes;
    unsigned vector_count;
} TecmoAudioProof;

static uint32_t audio_proof_fnv_update(uint32_t fingerprint,
                                       const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        fingerprint ^= bytes[index];
        fingerprint *= 16777619U;
    }
    return fingerprint;
}

static uint32_t audio_proof_samples_fingerprint(const int16_t *samples,
                                                size_t sample_count)
{
    uint32_t fingerprint = 2166136261U;
    size_t index;
    for (index = 0U; index < sample_count; ++index) {
        uint16_t value = (uint16_t)samples[index];
        uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8U)};
        fingerprint = audio_proof_fnv_update(fingerprint, bytes, 2U);
    }
    return fingerprint;
}

static bool audio_proof_append(TecmoAudioProof *proof,
                               const int16_t *samples, size_t sample_count)
{
    size_t needed;
    size_t capacity;
    int16_t *grown;
    if (proof == NULL || (sample_count > 0U && samples == NULL) ||
        sample_count > SIZE_MAX - proof->sample_count)
        return false;
    needed = proof->sample_count + sample_count;
    if (needed > SIZE_MAX / sizeof(*proof->samples)) return false;
    if (needed > proof->sample_capacity) {
        capacity = proof->sample_capacity == 0U
            ? AUDIO_PROOF_CHUNK_SAMPLES
            : proof->sample_capacity;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2U) return false;
            capacity *= 2U;
        }
        if (capacity > SIZE_MAX / sizeof(*proof->samples)) return false;
        grown = (int16_t *)realloc(proof->samples,
                                   capacity * sizeof(*proof->samples));
        if (grown == NULL) return false;
        proof->samples = grown;
        proof->sample_capacity = capacity;
    }
    if (sample_count > 0U) {
        memcpy(proof->samples + proof->sample_count, samples,
               sample_count * sizeof(*samples));
        {
            size_t index;
            for (index = 0U; index < sample_count; ++index) {
                uint16_t value = (uint16_t)samples[index];
                uint8_t bytes[2] = {
                    (uint8_t)value, (uint8_t)(value >> 8U)
                };
                proof->pcm_fingerprint = audio_proof_fnv_update(
                    proof->pcm_fingerprint, bytes, 2U);
            }
        }
        proof->sample_count = needed;
    }
    return true;
}

static bool audio_proof_write_event(TecmoAudioProof *proof,
                                    const char *line)
{
    size_t length;
    if (proof == NULL || proof->events == NULL || line == NULL) return false;
    length = strlen(line);
    if (fwrite(line, 1U, length, proof->events) != length) return false;
    proof->event_fingerprint = audio_proof_fnv_update(
        proof->event_fingerprint, (const uint8_t *)line, length);
    proof->event_bytes += (uint64_t)length;
    return true;
}

static bool audio_proof_record(
    TecmoAudioProof *proof, const char *name, const char *queue,
    const char *source, const char *termination, size_t start,
    size_t sample_count, const TecmoMusicPlayer *music,
    const TecmoGameplayAudioPlayer *gameplay)
{
    char line[1024];
    int written;
    uint64_t music_ticks;
    uint64_t music_accumulator;
    unsigned music_playing;
    unsigned sfx_id;
    unsigned sfx_playing;
    unsigned dmc_active;
    unsigned dmc_level;
    uint32_t pcm_fingerprint;
    if (proof == NULL || name == NULL || queue == NULL || source == NULL ||
        termination == NULL || start > proof->sample_count ||
        sample_count == 0U || sample_count > proof->sample_count - start ||
        proof->vector_count >= 1000U ||
        (sample_count > 0U && proof->samples == NULL))
        return false;
    music_ticks = music != NULL ? music->ticks_elapsed : 0U;
    music_accumulator = music != NULL ? music->sample_tick_accumulator : 0U;
    music_playing = music != NULL && music->playing ? 1U : 0U;
    sfx_id = gameplay != NULL ? gameplay->current_sfx_id : 0U;
    sfx_playing = gameplay != NULL && gameplay->sfx_playing ? 1U : 0U;
    dmc_active = gameplay != NULL && gameplay->dmc.active ? 1U : 0U;
    dmc_level = gameplay != NULL ? gameplay->dmc.output_level : 0U;
    pcm_fingerprint = audio_proof_samples_fingerprint(
        proof->samples + start, sample_count);
    written = snprintf(
        line, sizeof(line),
        "format=TECMAUDIOPROOF-1|vector=%03u|name=%-28.28s|start=%020llu|count=%020llu|queue=%-24.24s|source=%-24.24s|termination=%-24.24s|music_ticks=%020llu|music_acc=%020llu|music_playing=%u|sfx_id=%03u|sfx_playing=%u|dmc_active=%u|dmc_level=%03u|pcm_fnv=%08X\n",
        proof->vector_count, name,
        (unsigned long long)start, (unsigned long long)sample_count,
        queue, source, termination,
        (unsigned long long)music_ticks,
        (unsigned long long)music_accumulator,
        music_playing, sfx_id, sfx_playing, dmc_active, dmc_level,
        pcm_fingerprint);
    if (written < 0 || (size_t)written >= sizeof(line) ||
        !audio_proof_write_event(proof, line))
        return false;
    ++proof->vector_count;
    return true;
}

static bool audio_proof_render_music(TecmoAudioProof *proof,
                                     TecmoMusicPlayer *player,
                                     size_t sample_count)
{
    int16_t chunk[AUDIO_PROOF_CHUNK_SAMPLES];
    size_t remaining = sample_count;
    while (remaining > 0U) {
        size_t count = remaining > sizeof(chunk) / sizeof(chunk[0])
            ? sizeof(chunk) / sizeof(chunk[0]) : remaining;
        tecmo_music_render_samples(player, chunk, count);
        if (!audio_proof_append(proof, chunk, count)) return false;
        remaining -= count;
    }
    return true;
}

static bool audio_proof_render_gameplay(TecmoAudioProof *proof,
                                        TecmoGameplayAudioPlayer *player,
                                        size_t sample_count)
{
    int16_t chunk[AUDIO_PROOF_CHUNK_SAMPLES];
    size_t remaining = sample_count;
    while (remaining > 0U) {
        size_t count = remaining > sizeof(chunk) / sizeof(chunk[0])
            ? sizeof(chunk) / sizeof(chunk[0]) : remaining;
        tecmo_gameplay_audio_render_samples(player, chunk, count);
        if (!audio_proof_append(proof, chunk, count)) return false;
        remaining -= count;
    }
    return true;
}

static bool audio_proof_render_frontend(TecmoAudioProof *proof,
                                        TecmoFrontendAudioPlayer *player,
                                        size_t sample_count)
{
    int16_t chunk[AUDIO_PROOF_CHUNK_SAMPLES];
    size_t remaining = sample_count;
    while (remaining > 0U) {
        size_t count = remaining > sizeof(chunk) / sizeof(chunk[0])
            ? sizeof(chunk) / sizeof(chunk[0]) : remaining;
        tecmo_frontend_audio_render_samples(player, chunk, count);
        if (!audio_proof_append(proof, chunk, count)) return false;
        remaining -= count;
    }
    return true;
}

static bool audio_proof_drain_music(TecmoAudioProof *proof,
                                    TecmoMusicPlayer *player,
                                    size_t maximum_samples)
{
    int16_t chunk[AUDIO_PROOF_CHUNK_SAMPLES];
    size_t rendered = 0U;
    while (player->playing && rendered < maximum_samples) {
        size_t count = maximum_samples - rendered;
        if (count > sizeof(chunk) / sizeof(chunk[0]))
            count = sizeof(chunk) / sizeof(chunk[0]);
        tecmo_music_render_samples(player, chunk, count);
        if (!audio_proof_append(proof, chunk, count)) return false;
        rendered += count;
    }
    return !player->playing;
}

static bool audio_proof_drain_dmc(TecmoGameplayAudioPlayer *player)
{
    size_t rendered = 0U;
    while (player->dmc.active && rendered < AUDIO_PROOF_MAX_DMC_DRAIN_SAMPLES) {
        size_t count = AUDIO_PROOF_CHUNK_SAMPLES;
        if (count > AUDIO_PROOF_MAX_DMC_DRAIN_SAMPLES - rendered)
            count = AUDIO_PROOF_MAX_DMC_DRAIN_SAMPLES - rendered;
        tecmo_gameplay_audio_render_samples(player, NULL, count);
        rendered += count;
    }
    return !player->dmc.active;
}

static bool audio_proof_drain_dmc_capture(
    TecmoAudioProof *proof, TecmoGameplayAudioPlayer *player,
    size_t maximum_samples)
{
    int16_t chunk[AUDIO_PROOF_CHUNK_SAMPLES];
    size_t rendered = 0U;
    while (player->dmc.active && rendered < maximum_samples) {
        size_t count = maximum_samples - rendered;
        if (count > sizeof(chunk) / sizeof(chunk[0]))
            count = sizeof(chunk) / sizeof(chunk[0]);
        tecmo_gameplay_audio_render_samples(player, chunk, count);
        if (!audio_proof_append(proof, chunk, count)) return false;
        rendered += count;
    }
    return !player->dmc.active;
}

static bool audio_proof_cadence_ok(const TecmoMusicPlayer *player,
                                   size_t sample_count)
{
    uint64_t threshold;
    uint64_t numerator;
    if (player == NULL || player->asset == NULL) return false;
    threshold = (uint64_t)player->asset->sample_rate *
                player->asset->tick_denominator;
    numerator = (uint64_t)sample_count * player->asset->tick_numerator;
    return player->ticks_elapsed == numerator / threshold &&
           player->sample_tick_accumulator == numerator % threshold;
}

static bool audio_proof_write_u16(FILE *file, uint16_t value)
{
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8U)};
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static bool audio_proof_write_u32(FILE *file, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)value, (uint8_t)(value >> 8U),
        (uint8_t)(value >> 16U), (uint8_t)(value >> 24U)
    };
    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static bool audio_proof_write_wav(const char *path,
                                  const TecmoAudioProof *proof)
{
    FILE *file;
    uint64_t data_bytes64;
    uint32_t data_bytes;
    size_t index;
    if (path == NULL || proof == NULL ||
        proof->sample_count > (UINT32_MAX - 36U) / 2U)
        return false;
    data_bytes64 = (uint64_t)proof->sample_count * 2ULL;
    data_bytes = (uint32_t)data_bytes64;
    file = fopen(path, "wb");
    if (file == NULL) return false;
    if (fwrite("RIFF", 1U, 4U, file) != 4U ||
        !audio_proof_write_u32(file, 36U + data_bytes) ||
        fwrite("WAVEfmt ", 1U, 8U, file) != 8U ||
        !audio_proof_write_u32(file, 16U) ||
        !audio_proof_write_u16(file, 1U) ||
        !audio_proof_write_u16(file, 1U) ||
        !audio_proof_write_u32(file, TECMO_MUSIC_SAMPLE_RATE) ||
        !audio_proof_write_u32(file, TECMO_MUSIC_SAMPLE_RATE * 2U) ||
        !audio_proof_write_u16(file, 2U) ||
        !audio_proof_write_u16(file, 16U) ||
        fwrite("data", 1U, 4U, file) != 4U ||
        !audio_proof_write_u32(file, data_bytes)) {
        fclose(file);
        return false;
    }
    for (index = 0U; index < proof->sample_count; ++index) {
        if (!audio_proof_write_u16(file, (uint16_t)proof->samples[index])) {
            fclose(file);
            return false;
        }
    }
    if (fclose(file) != 0) return false;
    return true;
}

static bool audio_proof_write_manifest(
    const char *path, const TecmoAudioProof *proof,
    const TecmoMusicAsset *music, const TecmoGameplayAudioAsset *gameplay,
    const TecmoFrontendAudioAsset *frontend)
{
    FILE *file;
    if (path == NULL || proof == NULL || music == NULL || gameplay == NULL ||
        frontend == NULL)
        return false;
    file = fopen(path, "wb");
    if (file == NULL) return false;
    if (fprintf(file,
                "manifest=TECMAUDIOPROOF-1\n"
                "source=explicit-validated-pack\n"
                "pack_identity=canonical-shared\n"
                "rom_runtime_artifact=none\n"
                "sample_rate=%u\n"
                "channels=1\n"
                "bits_per_sample=16\n"
                "sample_encoding=signed-little-endian\n"
                "tmus_payload_size=%u\n"
                "tmus_payload_fnv1a32=%08X\n"
                "tmus_instruction_count=%u\n"
                "tmus_voice_count=%u\n"
                "tsfx_payload_size=%u\n"
                "tsfx_payload_fnv1a32=%08X\n"
                "tsfx_instruction_count=%u\n"
                "tsfx_voice_count=%u\n"
                "tdmc_payload_size=%u\n"
                "tdmc_payload_fnv1a32=%08X\n"
                "tfsx_payload_size=%u\n"
                "tfsx_payload_fnv1a32=%08X\n"
                "tfsx_instruction_count=%u\n"
                "tfsx_voice_count=%u\n"
                "vector_count=%u\n"
                "wav_sample_count=%llu\n"
                "wav_data_bytes=%llu\n"
                "wav_pcm_fnv1a32=%08X\n"
                "events_byte_count=%llu\n"
                "events_fnv1a32=%08X\n",
                TECMO_MUSIC_SAMPLE_RATE,
                TECMO_MUSIC_PAYLOAD_SIZE, music->payload_fingerprint,
                music->instruction_count, TECMO_MUSIC_VOICE_COUNT,
                TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE,
                gameplay->sfx_payload_fingerprint,
                gameplay->instruction_count, gameplay->voice_count,
                TECMO_GAMEPLAY_DMC_PAYLOAD_SIZE,
                gameplay->dmc_payload_fingerprint,
                TECMO_FRONTEND_SFX_PAYLOAD_SIZE,
                frontend->sfx.sfx_payload_fingerprint,
                frontend->sfx.instruction_count, frontend->sfx.voice_count,
                proof->vector_count,
                (unsigned long long)proof->sample_count,
                (unsigned long long)proof->sample_count * 2ULL,
                proof->pcm_fingerprint,
                (unsigned long long)proof->event_bytes,
                proof->event_fingerprint) < 0) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static bool audio_proof_path(char *path, size_t path_size,
                             const char *directory, const char *leaf)
{
    int written;
    if (path == NULL || path_size == 0U || directory == NULL ||
        directory[0] == '\0' || leaf == NULL || leaf[0] == '\0')
        return false;
    written = snprintf(path, path_size, "%s/%s", directory, leaf);
    return written >= 0 && (size_t)written < path_size;
}

static bool tecmo_cli_audio_pack_identity_test(
    const char *music_pack_path, const char *gameplay_pack_path,
    const char *expectation, char *message, size_t message_size)
{
    TecmoMusicAsset music_asset;
    TecmoGameplayAudioAsset current_asset;
    TecmoGameplayAudioAsset candidate_asset;
    TecmoMusicPlayer music_player;
    TecmoGameplayAudioPlayer current_player;
    TecmoGameplayAudioPlayer candidate_player;
    TecmoAudioOutput output;
    bool expect_accept;
    bool selected;
    bool ok = false;
    memset(&music_asset, 0, sizeof(music_asset));
    memset(&current_asset, 0, sizeof(current_asset));
    memset(&candidate_asset, 0, sizeof(candidate_asset));
    memset(&output, 0, sizeof(output));
    expect_accept = expectation != NULL &&
                    strcmp(expectation, "accept") == 0;
    if (!expect_accept &&
        (expectation == NULL || strcmp(expectation, "reject") != 0))
        return false;
    if (!tecmo_music_asset_load_from_pack(&music_asset, music_pack_path) ||
        !tecmo_gameplay_audio_asset_load_from_pack(
            &current_asset, music_pack_path) ||
        !tecmo_gameplay_audio_asset_load_from_pack(
            &candidate_asset, gameplay_pack_path))
        goto cleanup;
    tecmo_music_player_init(&music_player, &music_asset);
    tecmo_gameplay_audio_player_init(&current_player, &current_asset,
                                     &music_player);
    tecmo_gameplay_audio_player_init(&candidate_player, &candidate_asset,
                                     &music_player);
    /* This is deliberately a device-free initialized routing object. */
    output.initialized = true;
    output.player = &music_player;
    if (expect_accept) {
        selected = tecmo_audio_output_select_gameplay_player(
            &output, &candidate_player);
        ok = selected && output.gameplay_player == &candidate_player &&
             output.gameplay_asset == &candidate_asset &&
             strcmp(music_asset.asset_pack_path,
                    candidate_asset.asset_pack_path) == 0;
    } else {
        selected = tecmo_audio_output_select_gameplay_player(
            &output, &current_player);
        if (!selected) goto cleanup;
        selected = tecmo_audio_output_select_gameplay_player(
            &output, &candidate_player);
        ok = !selected && output.gameplay_player == &current_player &&
             output.gameplay_asset == &current_asset &&
             strcmp(music_asset.asset_pack_path,
                    candidate_asset.asset_pack_path) != 0;
    }
    if (ok && message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "Audio pack identity test pass: %s",
                       expect_accept ? "accept" : "reject-preserve");

cleanup:
    tecmo_gameplay_audio_asset_shutdown(&candidate_asset);
    tecmo_gameplay_audio_asset_shutdown(&current_asset);
    tecmo_music_asset_shutdown(&music_asset);
    return ok;
}

static bool tecmo_cli_run_audio_proof(const char *pack_path,
                                      const char *output_directory,
                                      char *message, size_t message_size)
{
    TecmoMusicAsset music;
    TecmoGameplayAudioAsset gameplay;
    TecmoFrontendAudioAsset frontend;
    TecmoMusicPlayer music_player;
    TecmoGameplayAudioPlayer gameplay_player;
    TecmoFrontendAudioPlayer frontend_player;
    TecmoAudioProof proof;
    char wav_path[1024];
    char events_path[1024];
    char manifest_path[1024];
    unsigned dmc_id;
    bool ok = false;
    if (pack_path == NULL || output_directory == NULL ||
        pack_path[0] == '\0' || output_directory[0] == '\0')
        return false;
    memset(&music, 0, sizeof(music));
    memset(&gameplay, 0, sizeof(gameplay));
    memset(&frontend, 0, sizeof(frontend));
    memset(&proof, 0, sizeof(proof));
    proof.pcm_fingerprint = 2166136261U;
    proof.event_fingerprint = 2166136261U;
    if (!tecmo_music_asset_load_from_pack(&music, pack_path) ||
        !tecmo_gameplay_audio_asset_load_from_pack(&gameplay, pack_path) ||
        !tecmo_frontend_audio_asset_load_from_pack(&frontend, pack_path) ||
        strcmp(music.asset_pack_path, gameplay.asset_pack_path) != 0 ||
        strcmp(music.asset_pack_path, frontend.sfx.asset_pack_path) != 0 ||
        !audio_proof_path(wav_path, sizeof(wav_path), output_directory,
                          "audio-proof.wav") ||
        !audio_proof_path(events_path, sizeof(events_path), output_directory,
                          "audio-proof.events") ||
        !audio_proof_path(manifest_path, sizeof(manifest_path),
                          output_directory, "audio-proof.manifest"))
        goto cleanup;
    proof.events = fopen(events_path, "wb");
    if (proof.events == NULL ||
        !audio_proof_write_event(
            &proof,
            "format=TECMAUDIOPROOF-1|sample_rate=44100|channels=1|bits=16|byte_order=little|records=fixed-fields\n"))
        goto cleanup;

    tecmo_music_player_init(&music_player, &music);
    if (!tecmo_music_queue_track(&music_player, TECMO_MUSIC_TRACK_OPENING))
        goto cleanup;
    {
        size_t start = proof.sample_count;
        if (!audio_proof_render_music(&proof, &music_player,
                                      AUDIO_PROOF_FIXED_SAMPLES) ||
            !audio_proof_record(&proof, "TMUS7_START", "TRACK7_PASS",
                                "TMUS", "STARTUP", start,
                                proof.sample_count - start, &music_player,
                                NULL))
            goto cleanup;
    }
    tecmo_music_player_init(&music_player, &music);
    if (!tecmo_music_queue_track(&music_player, TECMO_MUSIC_TRACK_OPENING))
        goto cleanup;
    {
        size_t start = proof.sample_count;
        if (!audio_proof_drain_music(
                &proof, &music_player, AUDIO_PROOF_TMUS7_DRAIN_LIMIT) ||
            !audio_proof_record(&proof, "TMUS7_TAIL_END", "TRACK7_PASS",
                                "TMUS", "CLEAN_END", start,
                                proof.sample_count - start, &music_player,
                                NULL))
            goto cleanup;
    }
    for (dmc_id = 5U; dmc_id <= 6U; ++dmc_id) {
        const uint8_t track_id = (uint8_t)dmc_id;
        size_t start;
        tecmo_music_player_init(&music_player, &music);
        if (!tecmo_music_queue_track(&music_player, track_id)) goto cleanup;
        start = proof.sample_count;
        if (!audio_proof_render_music(&proof, &music_player, 1000000U) ||
            !audio_proof_cadence_ok(&music_player, 1000000U) ||
            !audio_proof_record(&proof,
                                track_id == 5U ? "TMUS5_LOOP" : "TMUS6_LOOP",
                                track_id == 5U ? "TRACK5_PASS" : "TRACK6_PASS",
                                "TMUS", "LOOP_REPRESENTATIVE", start,
                                proof.sample_count - start, &music_player,
                                NULL))
            goto cleanup;
    }
    tecmo_music_player_init(&music_player, &music);
    if (!tecmo_music_queue_track(
            &music_player, TECMO_MUSIC_TRACK_PREGAME_MATCHUP_STINGER))
        goto cleanup;
    {
        size_t start = proof.sample_count;
        if (!audio_proof_drain_music(&proof, &music_player, 1000000U) ||
            !audio_proof_record(&proof, "TMUS8_END", "TRACK8_PASS", "TMUS",
                                "CLEAN_END", start,
                                proof.sample_count - start, &music_player,
                                NULL))
            goto cleanup;
    }

    tecmo_frontend_audio_player_init(&frontend_player, &frontend, NULL);
    {
        size_t start = proof.sample_count;
        if (!tecmo_frontend_audio_queue_menu_accept(&frontend_player) ||
            !audio_proof_render_frontend(&proof, &frontend_player,
                                         AUDIO_PROOF_FIXED_SAMPLES) ||
            !audio_proof_record(&proof, "TFSX8_DRY", "SFX8_PASS", "TFSX",
                                "DRY_CUE", start,
                                proof.sample_count - start, NULL,
                                &frontend_player.sfx))
            goto cleanup;
    }
    tecmo_frontend_audio_player_init(&frontend_player, &frontend, NULL);
    {
        size_t start = proof.sample_count;
        if (!tecmo_frontend_audio_queue_title_confirm(&frontend_player) ||
            !audio_proof_render_frontend(&proof, &frontend_player,
                                         AUDIO_PROOF_FIXED_SAMPLES) ||
            !audio_proof_record(&proof, "TFSX10_DRY", "SFX10_PASS", "TFSX",
                                "DRY_CUE", start,
                                proof.sample_count - start, NULL,
                                &frontend_player.sfx))
            goto cleanup;
    }
    {
        static const uint8_t sfx_ids[7] = {3U, 5U, 6U, 11U, 12U, 13U, 14U};
        unsigned index;
        for (index = 0U; index < sizeof(sfx_ids) / sizeof(sfx_ids[0]); ++index) {
            char name[32];
            char queue[32];
            size_t start;
            tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay,
                                             NULL);
            (void)snprintf(name, sizeof(name), "TSFX%u_DRY", sfx_ids[index]);
            (void)snprintf(queue, sizeof(queue), "SFX%u_PASS", sfx_ids[index]);
            if (!tecmo_gameplay_audio_queue_sfx_id(
                    &gameplay_player, sfx_ids[index]))
                goto cleanup;
            start = proof.sample_count;
            if (!audio_proof_render_gameplay(&proof, &gameplay_player,
                                             AUDIO_PROOF_FIXED_SAMPLES) ||
                !audio_proof_record(&proof, name, queue, "TSFX", "DRY_CUE",
                                    start, proof.sample_count - start, NULL,
                                    &gameplay_player))
                goto cleanup;
        }
    }
    tecmo_music_player_init(&music_player, &music);
    tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay,
                                     &music_player);
    if (!tecmo_music_queue_track(&music_player, TECMO_MUSIC_TRACK_GAMEPLAY) ||
        !tecmo_gameplay_audio_queue_sfx_id(&gameplay_player, 3U))
        goto cleanup;
    {
        uint64_t music_ticks_before = music_player.ticks_elapsed;
        size_t start = proof.sample_count;
        if (!audio_proof_render_gameplay(&proof, &gameplay_player, 8192U) ||
            music_player.ticks_elapsed <= music_ticks_before ||
            !audio_proof_record(&proof, "TMUS5_TSFX3_OVERRIDE",
                                "TRACK5_SFX3_PASS", "MIXED", "MUSIC_TICKS",
                                start, proof.sample_count - start,
                                &music_player, &gameplay_player))
            goto cleanup;
    }
    for (dmc_id = 0U; dmc_id < TECMO_GAMEPLAY_DMC_CLIP_COUNT; ++dmc_id) {
        char name[32];
        char queue[32];
        size_t start;
        tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay, NULL);
        (void)snprintf(name, sizeof(name), "TDMC%u_CLIP", dmc_id);
        (void)snprintf(queue, sizeof(queue), "DMC%u_PASS", dmc_id);
        if (!tecmo_gameplay_audio_queue_dmc_clip(
                &gameplay_player, (TecmoGameplayDmcClipId)dmc_id))
            goto cleanup;
        start = proof.sample_count;
        if (!audio_proof_drain_dmc_capture(
                &proof, &gameplay_player,
                AUDIO_PROOF_MAX_DMC_DRAIN_SAMPLES) ||
            !audio_proof_render_gameplay(&proof, &gameplay_player,
                                         AUDIO_PROOF_FIXED_SAMPLES) ||
            !audio_proof_record(&proof, name, queue, "TDMC",
                                "INACTIVE_HELD_DAC",
                                start, proof.sample_count - start, NULL,
                                &gameplay_player))
            goto cleanup;
    }
    tecmo_gameplay_audio_player_init(&gameplay_player, &gameplay, NULL);
    if (!tecmo_gameplay_audio_queue_dmc_clip(
            &gameplay_player, TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT) ||
        !audio_proof_drain_dmc(&gameplay_player))
        goto cleanup;
    {
        uint8_t held_level = gameplay_player.dmc.output_level;
        uint8_t retrigger_level;
        size_t start = proof.sample_count;
        if (!audio_proof_render_gameplay(&proof, &gameplay_player,
                                         AUDIO_PROOF_FIXED_SAMPLES) ||
            gameplay_player.dmc.active ||
            gameplay_player.dmc.output_level != held_level ||
            !audio_proof_record(&proof, "TDMC_POST_END_HOLD",
                                "DMC0_END_PASS", "TDMC", "HELD_DAC",
                                start, proof.sample_count - start, NULL,
                                &gameplay_player))
            goto cleanup;
        if (!tecmo_gameplay_audio_queue_dmc_clip(
                &gameplay_player, TECMO_GAMEPLAY_DMC_BANK05_A8D6_SHORT))
            goto cleanup;
        start = proof.sample_count;
        if (!audio_proof_render_gameplay(&proof, &gameplay_player,
                                         AUDIO_PROOF_CHUNK_SAMPLES) ||
            !gameplay_player.dmc.active ||
            !audio_proof_record(&proof, "TDMC_RETRIGGER",
                                "DMC0_RETRIGGER_PASS", "TDMC", "ACTIVE",
                                start, proof.sample_count - start, NULL,
                                &gameplay_player))
            goto cleanup;
        retrigger_level = gameplay_player.dmc.output_level;
        tecmo_gameplay_audio_stop_all(&gameplay_player);
        start = proof.sample_count;
        if (!audio_proof_render_gameplay(&proof, &gameplay_player,
                                         AUDIO_PROOF_FIXED_SAMPLES) ||
            gameplay_player.dmc.active ||
            gameplay_player.dmc.output_level != retrigger_level ||
            !audio_proof_record(&proof, "TDMC_STOP_HOLD",
                                "STOP_ALL_PASS", "TDMC", "HELD_DAC",
                                start, proof.sample_count - start, NULL,
                                &gameplay_player))
            goto cleanup;
    }
    if (fclose(proof.events) != 0) {
        proof.events = NULL;
        goto cleanup;
    }
    proof.events = NULL;
    if (!audio_proof_write_wav(wav_path, &proof) ||
        !audio_proof_write_manifest(manifest_path, &proof, &music, &gameplay,
                                    &frontend))
        goto cleanup;
    ok = true;
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "Audio proof pass: vectors=%u samples=%llu",
                       proof.vector_count,
                       (unsigned long long)proof.sample_count);

cleanup:
    if (proof.events != NULL) fclose(proof.events);
    free(proof.samples);
    tecmo_frontend_audio_asset_shutdown(&frontend);
    tecmo_gameplay_audio_asset_shutdown(&gameplay);
    tecmo_music_asset_shutdown(&music);
    if (!ok && message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "Audio proof generation failed at vector=%u samples=%llu.",
                       proof.vector_count,
                       (unsigned long long)proof.sample_count);
    return ok;
}


int tecmo_cli_run_audio_commands(const TecmoCliContext *context)
{
    const char *command;
    const char *root;
    int argc;
    char **argv;
    int index;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    root = context->root;
    argc = context->argc;
    argv = context->argv;
    index = context->index;
    if (strcmp(command, "--music-test") == 0) {
        char message[384];
        char output_message[64];
        if (!tecmo_music_self_test(root, message, sizeof(message))) {
            printf("Music test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_audio_output_self_test(output_message,
                                          sizeof(output_message))) {
            printf("Music output test failed: %s\n", output_message);
            return 1;
        }
        printf("%s %s\n", message, output_message);
        return 0;
    }

    if (strcmp(command, "--music-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (tecmo_asset_pack_music_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Music source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-audio-test") == 0) {
        char message[512] = {0};
        char output_message[64];
        if (!tecmo_gameplay_audio_self_test(root, message, sizeof(message))) {
            printf("Gameplay audio test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_audio_output_self_test(output_message,
                                          sizeof(output_message))) {
            printf("Gameplay audio output test failed: %s\n",
                   output_message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--frontend-audio-test") == 0) {
        char message[256] = {0};
        if (!tecmo_frontend_audio_self_test(
                root, message, sizeof(message))) {
            printf("Frontend audio test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--frontend-audio-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (tecmo_asset_pack_frontend_audio_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Frontend audio source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-audio-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (tecmo_asset_pack_gameplay_audio_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay audio source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--audio-pack-identity-test") == 0) {
        const char *music_pack = index < argc ? argv[index++] : NULL;
        const char *gameplay_pack = index < argc ? argv[index++] : NULL;
        const char *expectation = index < argc ? argv[index++] : NULL;
        char message[256] = {0};
        if (music_pack == NULL || gameplay_pack == NULL ||
            expectation == NULL || index != argc ||
            !tecmo_cli_audio_pack_identity_test(
                music_pack, gameplay_pack, expectation,
                message, sizeof(message))) {
            printf("Audio pack identity test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--audio-proof") == 0) {
        const char *pack_path = index < argc ? argv[index++] : NULL;
        const char *output_directory = index < argc ? argv[index++] : NULL;
        char message[256] = {0};
        if (pack_path == NULL || output_directory == NULL || index != argc ||
            !tecmo_cli_run_audio_proof(
                pack_path, output_directory, message, sizeof(message))) {
            printf("Audio proof failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--frontend-audio-cross-pack-test") == 0) {
        const char *frontend_pack = index < argc ? argv[index++] : NULL;
        const char *music_pack = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (!tecmo_frontend_audio_cross_pack_self_test(
                frontend_pack, music_pack, message, sizeof(message))) {
            printf("Frontend audio cross-pack test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
